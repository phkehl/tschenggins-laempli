/*!
    \file
    \brief flipflip's Tschenggins Lämpli: wifi things (see \ref FF_WIFI)

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

#if (HAVE_CONFIG > 0)
static void sWifiTask(void *pArg)
{
    osSleep(1500);

    sdk_wifi_station_disconnect();
    sdk_wifi_station_dhcpc_stop();

    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_set_phy_mode(PHY_MODE_11N);
    struct sdk_station_config config =
    {
        .ssid = FF_CFG_STASSID, .password = FF_CFG_STAPASS, .bssid_set = false, .bssid = { 0 }
    };
    sdk_wifi_station_set_config(&config);

    //sdk_wifi_set_phy_mode(PHY_MODE_11N);
    sdk_wifi_set_sleep_type(WIFI_SLEEP_MODEM);

    char name[32];
    getSystemName(name, sizeof(name));
    //sdk_wifi_station_set_hostname(sStaName);
#if LWIP_NETIF_HOSTNAME
    struct netif *netif = sdk_system_get_netif(STATION_IF);
    netif_set_hostname(netif, name);
#endif

    if (!sdk_wifi_station_dhcpc_start())
    {
        ERROR("wifi: sta: sdk_wifi_station_dhcpc_start() fail");
    }
    if (!sdk_wifi_station_connect())
    {
        ERROR("wifi: sta: sdk_wifi_station_connect() fail");
    }

    /*
    while (true)
    {
        const uint8_t wifiStatus = sdk_wifi_station_get_connect_status();
        const enum sdk_dhcp_status dhcpStatus = sdk_wifi_station_dhcpc_status();
        PRINT("wifi: sta: %s %s",
            getWifiStationConnectStatusStr(wifiStatus),
            getDhcpStatusStr(dhcpStatus));
        osSleep(500);
    }
    */

    while (true)
    {
        DEBUG("wifi: ...");
        osSleep(1000);
    }
}



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
    sdk_wifi_set_opmode_current(STATION_MODE);
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

void wifiMonStatus(void)
{
    const char *mode   = sdkWifiOpmodeStr( sdk_wifi_get_opmode() );
    const char *status = sdkStationConnectStatusStr( sdk_wifi_station_get_connect_status() );
    const char *dhcp   = sdkDhcpStatusStr( sdk_wifi_station_dhcpc_status() );
    const char *phy    = sdkWifiPhyModeStr( sdk_wifi_get_phy_mode() );
    const char *sleep  = sdkWifiSleepTypeStr( sdk_wifi_get_sleep_type() );
    const uint8_t ch   = sdk_wifi_get_channel();
    uint8_t mac[6];
    sdk_wifi_get_macaddr(STATION_IF, mac);
    DEBUG("mon: wifi: mode=%s status=%s dhcp=%s phy=%s sleep=%s ch=%u mac="MACSTR,
        mode, status, dhcp, phy, sleep, ch, MAC2STR(mac));

    struct ip_info ipinfo;
    sdk_wifi_get_ip_info(STATION_IF, &ipinfo);
#if LWIP_NETIF_HOSTNAME
    struct netif *netif = sdk_system_get_netif(STATION_IF);
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

    sdk_wifi_set_opmode(NULL_MODE);
    sdk_wifi_set_opmode_current(NULL_MODE);
    sdk_wifi_station_set_auto_connect(false);
    //sdk_wifi_status_led_install(2, PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);

    xTaskCreate(sWifiTask, "ff_wifi", 320, NULL, 2, NULL);
}

// eof
