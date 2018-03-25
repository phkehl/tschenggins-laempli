/*!
    \file
    \brief flipflip's Tschenggins Lämpli: wifi and network things (see \ref FF_WIFI)

    - Copyright (c) 2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli
*/

#include "stdinc.h"

#include <lwip/api.h>
#include <lwip/netif.h>

#include "stuff.h"
#include "debug.h"
#include "wifi.h"
#include "cfg_gen.h"

#if (!defined FF_CFG_STASSID || !defined FF_CFG_STAPASS || !defined FF_CFG_BACKENDURL)
#  define HAVE_CONFIG 0
#  warning incomplete or missing configuration
#else
#  define HAVE_CONFIG 1
#endif

/* ********************************************************************************************** */

#if (HAVE_CONFIG > 0)

typedef enum WIFI_STATE_e
{
    WIFI_STATE_UNKNOWN,   // don't know --> initialise
    WIFI_STATE_OFFLINE,   // initialised, offline --> connect station
    WIFI_STATE_ONLINE,    // station online --> connect to backend
    WIFI_STATE_CONNECTED, // backend connected
    WIFI_STATE_FAIL,      // failure (e.g. connection lost) --> initialise
} WIFI_STATE_t;

static WIFI_STATE_t sWifiState = WIFI_STATE_UNKNOWN;

static const char *sWifiStateStr(const WIFI_STATE_t state)
{
    switch (state)
    {
        case WIFI_STATE_UNKNOWN:    return "UNKNOWN";
        case WIFI_STATE_OFFLINE:    return "OFFLINE";
        case WIFI_STATE_ONLINE:     return "ONLINE";
        case WIFI_STATE_CONNECTED:  return "CONNECTED";
        case WIFI_STATE_FAIL:       return "FAIL";
    }
    return "???";
}

static bool sWifiInit(void)
{
    sdk_wifi_station_disconnect();
    sdk_wifi_station_set_auto_connect(false);
    if (sdk_wifi_station_dhcpc_status() == DHCP_STOPPED)
    {
        if (!sdk_wifi_station_dhcpc_start())
        {
            ERROR("wifi: sdk_wifi_station_dhcpc_start() fail!");
            return false;
        }
    }
    if (sdk_wifi_get_opmode() != ONLINE_MODE)
    {
        if (!sdk_wifi_set_opmode(ONLINE_MODE))
        {
            ERROR("wifi: sdk_wifi_set_opmode() fail!");
            return false;
        }
        if (!sdk_wifi_set_opmode_current(ONLINE_MODE))
        {
            ERROR("wifi: sdk_wifi_set_opmode_current() fail!");
            return false;
        }
    }
    if (sdk_wifi_get_opmode() != PHY_MODE_11N)
    {
        if (!sdk_wifi_set_phy_mode(PHY_MODE_11N))
        {
            ERROR("wifi: sdk_wifi_set_phy_mode(PHY_MODE_11N) fail!");
            return false;
        }
    }
    if (sdk_wifi_get_sleep_type() != WIFI_SLEEP_MODEM)
    {
        if (!sdk_wifi_set_sleep_type(WIFI_SLEEP_MODEM))
        {
            ERROR("wifi: sdk_wifi_set_sleep_type(WIFI_SLEEP_MODEM) fail!");
            return false;
        }
    }
    return true;
}

static bool sWifiConnect(void)
{
    struct sdk_station_config config =
    {
        .ssid = FF_CFG_STASSID, .password = FF_CFG_STAPASS, .bssid_set = false, .bssid = { 0 }
    };
    sdk_wifi_station_set_config(&config);

    char name[32];
    getSystemName(name, sizeof(name));
    //sdk_wifi_station_set_hostname(sStaName);
#if LWIP_NETIF_HOSTNAME
    struct netif *netif = sdk_system_get_netif(ONLINE_IF);
    netif_set_hostname(netif, name);
#endif

    if (!sdk_wifi_station_connect())
    {
        ERROR("wifi: sdk_wifi_station_connect() fail");
        return false;
    }

    // wait for connection to come up
    bool connected = false;
    {
        const uint32_t now = osTime();
        const uint32_t timeout = now + 15000;
        while (osTime() < timeout)
        {
            const uint8_t status = sdk_wifi_station_get_connect_status();
            struct ip_info ipinfo;
            sdk_wifi_get_ip_info(ONLINE_IF, &ipinfo);
            DEBUG("wifi: status=%s ip="IPSTR" mask="IPSTR" gw="IPSTR,
                sdkStationConnectStatusStr(status),
                IP2STR(&ipinfo.ip), IP2STR(&ipinfo.netmask), IP2STR(&ipinfo.gw));
            if ( (status == ONLINE_GOT_IP) && (ipinfo.ip.addr != 0) )
            {
                connected = true;
                break;
            }
            osSleep(250);
        }
    }

    return connected;
}

static void sWifiConnectBackend(void)
{
//    int err = getaddrinfo(WEB_SERVER, "80", &hints, &res);
}


static void sWifiTask(void *pArg)
{
    WIFI_STATE_t oldState = WIFI_STATE_UNKNOWN;
    while (true)
    {
        if (oldState != sWifiState)
        {
            DEBUG("wifi: %s -> %s", sWifiStateStr(oldState), sWifiStateStr(sWifiState));
            oldState = sWifiState;
        }

        switch (sWifiState)
        {
            // initialise
            case WIFI_STATE_UNKNOWN:
            {
                PRINT("wifi: state unknow, initialising...");
                if (!sWifiInit())
                {
                    sWifiState = WIFI_STATE_FAIL;
                }
                else
                {
                    sWifiState = WIFI_STATE_OFFLINE;
                }
                break;
            }

            // we're offline --> connect station
            case WIFI_STATE_OFFLINE:
            {
                PRINT("wifi: state offline, connecting station...");
                if (sWifiConnect())
                {
                    sWifiState = WIFI_STATE_ONLINE;
                }
                else
                {
                    sWifiState = WIFI_STATE_FAIL;
                }

                break;
            }

            case WIFI_STATE_ONLINE:
            {
                PRINT("wifi: state station connected!");
                osSleep(10000);
                break;
            }

            case WIFI_STATE_CONNECTED:
            {

                break;
            }

            case WIFI_STATE_FAIL:
            {
                PRINT("wifi: failure...");
                osSleep(5000);
                sWifiState = WIFI_STATE_UNKNOWN;
                break;
            }
        }

        osSleep(100);
    }
}



/* ********************************************************************************************** */

#else // (HAVE_CONFIG > 0)

#define WIFI_SCAN_PERIOD 5000

void sWifiScanDoneCb(void *pArg, sdk_scan_status_t status)
{
    static const char * const skScanStatusStrs [] =
    {
        [SCAN_OK] = "OK", [SCAN_FAIL] = "FAIL", [SCAN_PENDING] = "PENDING", [SCAN_BUSY] = "BUSY", [SCAN_CANCEL] = "CANCEL"
    };
    switch (status)
    {
        case SCAN_FAIL:
        case SCAN_PENDING:
        case SCAN_BUSY:
        case SCAN_CANCEL:
            ERROR("wifi: scan fail: %s", skScanStatusStrs[status]);
            return;
        case SCAN_OK:
            break;
        default:
            ERROR("wifi: scan fail: %u", status);
            return;
    }

    // we get a list of found access points
    const struct sdk_bss_info *pkBss = (const struct sdk_bss_info *)pArg;
    pkBss = STAILQ_NEXT(pkBss, next); // the first is rubbish

    while (pkBss != NULL)
    {
        char ssid[34];
        strncpy(ssid, (const char *)pkBss->ssid, sizeof(ssid) - 1);
        const int len = strlen(ssid);
        if (pkBss->is_hidden != 0)
        {
            ssid[len] = '*';
            ssid[len + 1] = '\0';
        }

        PRINT("wifi: scan: ssid=%-33s bssid="MACSTR" channel=%02u rssi=%02d auth=%s",
            ssid, MAC2STR(pkBss->bssid), pkBss->channel, pkBss->rssi, sdkAuthModeStr(pkBss->authmode));

        pkBss = STAILQ_NEXT(pkBss, next);
    }
}

static void sWifiTask(void *pArg)
{
    sdk_wifi_set_opmode_current(ONLINE_MODE);
    while (true)
    {
        static uint32_t sTick;
        vTaskDelayUntil(&sTick, MS2TICK(WIFI_SCAN_PERIOD));
        PRINT("wifi: no config -- initiating wifi scan");
        struct sdk_scan_config cfg = { .ssid = NULL, .bssid = NULL, .channel = 0, .show_hidden = true };
        sdk_wifi_station_scan(&cfg, sWifiScanDoneCb);
    }
}

#endif // (HAVE_CONFIG > 0)


/* ********************************************************************************************** */

void wifiMonStatus(void)
{
    DEBUG("mon: wifi: state=%s", sWifiStateStr(sWifiState));

    const char *mode   = sdkWifiOpmodeStr( sdk_wifi_get_opmode() );
    const char *status = sdkStationConnectStatusStr( sdk_wifi_station_get_connect_status() );
    const char *dhcp   = sdkDhcpStatusStr( sdk_wifi_station_dhcpc_status() );
    const char *phy    = sdkWifiPhyModeStr( sdk_wifi_get_phy_mode() );
    const char *sleep  = sdkWifiSleepTypeStr( sdk_wifi_get_sleep_type() );
    const uint8_t ch   = sdk_wifi_get_channel();
    uint8_t mac[6];
    sdk_wifi_get_macaddr(ONLINE_IF, mac);
    DEBUG("mon: wifi: mode=%s status=%s dhcp=%s phy=%s sleep=%s ch=%u mac="MACSTR,
        mode, status, dhcp, phy, sleep, ch, MAC2STR(mac));

    struct ip_info ipinfo;
    sdk_wifi_get_ip_info(ONLINE_IF, &ipinfo);
#if LWIP_NETIF_HOSTNAME
    struct netif *netif = sdk_system_get_netif(ONLINE_IF);
    const char *name   = netif_get_hostname(netif);
#else
    const char *name   = "???";
#endif
    DEBUG("mon: wifi: name=%s ip="IPSTR" mask="IPSTR" gw="IPSTR,
        name, IP2STR(&ipinfo.ip), IP2STR(&ipinfo.netmask), IP2STR(&ipinfo.gw));
}


void wifiInit(void)
{
    DEBUG("wifiInit()");

    //sdk_wifi_status_led_install(2, PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);

    xTaskCreate(sWifiTask, "ff_wifi", 320, NULL, 2, NULL);
}

/* ********************************************************************************************** */

// eof
