/*!
    \file
    \brief flipflip's Tschenggins Lämpli: WiFi access point and station (see \ref USER_WIFI)

    - Copyright (c) 2017 Philippe Kehl <flipflip at oinkzwurgl dot org>,
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

    \addtogroup USER_WIFI

    @{
*/

#include "user_wifi.h"
#include "user_stuff.h"
#include "user_status.h"
#include "user_cfg.h"
#include "user_httpd.h"
#include "html_gen.h"
#include "user_config.h"


/* ***** initialisation and status ************************************************************** */

static const char skOpmodeStrs[][12] PROGMEM =
{
    { "NULL\0" }, { "STATION\0" }, { "SOFTAP\0" }, { "STATIONAP\0" }
};

static const char skStatusStrs[][16] PROGMEM =
{
    { "IDLE\0" }, { "CONNECTING\0" }, { "WRONG_PASSWORD\0" }, { "NO_AP_FOUND\0" }, { "CONNECT_FAIL\0" }, { "GOT_IP\0" }
};
static const char skPhymodeStrs[][4] PROGMEM =
{
    { "???\0" }, { "11B\0" }, { "11G\0" }, { "11N\0" }
};
static const char skSleeptypeStrs[][8] PROGMEM =
{
    { "NONE\0" }, { "LIGHT\0" }, { "MODEM\0" }
};
static const char skDhcpStatusStrs[][8] PROGMEM =
{
    { "STOPPED\0" }, { "STARTED\0" }
};
static const char skFixedRateStrs[][4] PROGMEM =
{
    { "48\0" }, { "24\0" }, { "12\0" }, { "6\0" }, { "54\0" }, { "36\0" }, { "18\0" }, { "9\0" }
};
#if (USER_WIFI_USE_AP > 0)
static const char skAuthModeStrs[][16] PROGMEM =
{
    { "OPEN\0" }, { "WEP\0" }, { "WPA_PSK\0" }, { "WPA2_PSK\0" }, { "WPA_WPA2_PSK\0" }
};
#endif

// -------------------------------------------------------------------------------------------------

#if (USER_WIFI_USE_AP > 0)
#  define WIFI_OPMODE STATIONAP_MODE
#else
#  define WIFI_OPMODE STATION_MODE
#endif

static bool sWifiStatusRequestCb(struct espconn *pConn, const HTTPD_REQCB_INFO_t *pkInfo);

// initialise wifi stuff
void ICACHE_FLASH_ATTR wifiInit(void)
{
    DEBUG("wifi: init (%s)", skOpmodeStrs[WIFI_OPMODE]);
    wifi_set_opmode_current(NULL_MODE);
    wifi_station_set_auto_connect(false);
    wifi_set_phy_mode(PHY_MODE_11G); // 11B, 11G, 11N
    //wifi_station_set_reconnect_policy(true);

    httpdRegisterRequestCb(PSTR("/wifi"), HTTPD_AUTH_ADMIN, sWifiStatusRequestCb);
}

void ICACHE_FLASH_ATTR user_rf_pre_init(void)
{
    // the defaults are:
    //const uint32_t chipId = system_get_chip_id();
    //uint8_t defaultStaMac[] =
    //{
    //    0x5c, 0xcf, 0x7f, // Espressif Inc.
    //    (chipId >> 16) & 0xff,
    //    (chipId >>  8) & 0xff,
    //     chipId        & 0xff
    //}
    //uint8_t defaultApMac[] =
    //{
    //    0x5e, 0xcf, 0x7f, // ???
    //    (chipId >> 16) & 0xff,
    //    (chipId >>  8) & 0xff,
    //     chipId        & 0xff
    //}

    //uint8_t mac[] = { 0x5c, 0xcf, 0x7f, 0x01, 0x02, 0x03 };
    //wifi_set_macaddr(STATION_IF, mac);
}

// FIXME: what is this good for? is it needed?
uint32_t ICACHE_FLASH_ATTR user_rf_cal_sector_set(void)
{
    enum flash_size_map size_map = system_get_flash_size_map();
    uint32 rf_cal_sec = 0;

    switch (size_map)
    {
        case FLASH_SIZE_4M_MAP_256_256:
            rf_cal_sec = 128 - 5;
            break;

        case FLASH_SIZE_8M_MAP_512_512:
            rf_cal_sec = 256 - 5;
            break;

        case FLASH_SIZE_16M_MAP_512_512:
        case FLASH_SIZE_16M_MAP_1024_1024:
            rf_cal_sec = 512 - 5;
            break;

        case FLASH_SIZE_32M_MAP_512_512:
        case FLASH_SIZE_32M_MAP_1024_1024:
            rf_cal_sec = 1024 - 5;
            break;

        case FLASH_SIZE_2M:
        default:
            rf_cal_sec = 0;
            break;
    }
    return rf_cal_sec;

    // from esp-open-sdk:
    //extern char flashchip;
    //SpiFlashChip *flash = (SpiFlashChip*)(&flashchip + 4);
    //// We know that sector size in 4096
    ////uint32_t sec_num = flash->chip_size / flash->sector_size;
    //uint32_t sec_num = flash->chip_size >> 12;
    //return sec_num - 5;

}


// -------------------------------------------------------------------------------------------------

// cheap mac -> rssi hash table with signal strength seen for clients

#if (USER_WIFI_USE_AP > 0)

#define MACHASH(mac) \
        ((uint32_t)mac[5] <<  0) ^ \
        ((uint32_t)mac[4] <<  6) ^ \
        ((uint32_t)mac[3] << 12) ^ \
        ((uint32_t)mac[2] << 18) ^ \
        ((uint32_t)mac[1] << 22) ^ \
        ((uint32_t)mac[0] << 26)

static uint32_t sWifiStaObsRssiKey[10];
static int8_t   sWifiStaObsRssiVal[NUMOF(sWifiStaObsRssiKey)];

static void ICACHE_FLASH_ATTR sWifiSetRssi(const uint8_t mac[6], const int8_t rssi)
{
    uint32_t hash = MACHASH(mac);
    //DEBUG("sWifiSetRssi(): mac="MACSTR" rssi=%d hash=0x%08x", MAC2STR(mac), rssi, hash);
    int ix = NUMOF(sWifiStaObsRssiKey);
    // update existing entry
    while (ix--)
    {
        if (sWifiStaObsRssiKey[ix] == hash)
        {
            sWifiStaObsRssiVal[ix] = rssi;
            //DEBUG("upd ix=%d: %d", ix, rssi);
            return;
        }
    }
    // populate empty entry
    ix = NUMOF(sWifiStaObsRssiKey);
    while (ix--)
    {
        if (sWifiStaObsRssiKey[ix] == 0)
        {
            sWifiStaObsRssiKey[ix] = hash;
            sWifiStaObsRssiVal[ix] = rssi;
            //DEBUG("new ix=%d: %d", ix, rssi);
            return;
        }
    }
    // overwrite random entry
    ix = os_random() % NUMOF(sWifiStaObsRssiKey);
    sWifiStaObsRssiKey[ix] = hash;
    sWifiStaObsRssiVal[ix] = rssi;
    //DEBUG("rnd ix=%d: %d", ix, rssi);
}

static int8_t ICACHE_FLASH_ATTR sWifiGetRssi(const uint8_t mac[6])
{
    uint32_t hash = MACHASH(mac);
    int ix = NUMOF(sWifiStaObsRssiKey);
    // find existing entry
    while (ix--)
    {
        if (sWifiStaObsRssiKey[ix] == hash)
        {
            return sWifiStaObsRssiVal[ix];
        }
    }
    return 0;
}
#endif // (USER_WIFI_USE_AP > 0)


// -------------------------------------------------------------------------------------------------

// print wifi status information, see sMonitorTimerCb()
void ICACHE_FLASH_ATTR wifiStatus(void)
{
    const unsigned int opmode = wifi_get_opmode();
    const char *opmodeStr = (opmode < NUMOF(skOpmodeStrs)) ? skOpmodeStrs[opmode] : PSTR("???");

    const unsigned int status = wifi_station_get_connect_status();
    const char *statusStr = (status < NUMOF(skStatusStrs)) ? skStatusStrs[status] : PSTR("???");

    const unsigned int phymode = wifi_get_phy_mode();
    const char *phymodeStr = skPhymodeStrs[phymode < NUMOF(skPhymodeStrs) ? phymode : 0 ];

    const unsigned int sleeptype = wifi_get_sleep_type();
    const char *sleeptypeStr = (sleeptype < NUMOF(skSleeptypeStrs)) ? skSleeptypeStrs[sleeptype] : PSTR("???");

    const int dhcpcStatus = wifi_station_dhcpc_status();
    const char *dhcpcStatusStr = (dhcpcStatus > 0) && (dhcpcStatus < (int)NUMOF(skDhcpStatusStrs)) ?
        skDhcpStatusStrs[dhcpcStatus] : PSTR("???");

    const int rssi = wifi_station_get_rssi();
    struct ip_info ipinfo;
    wifi_get_ip_info(STATION_IF, &ipinfo);

    uint8_t rateEnable = 0;
    uint8_t fixedRate = 0;
    const int rateRes = wifi_get_user_fixed_rate(&rateEnable, &fixedRate);
    const char *fixedRateEnStr =
         fixedRate & FIXED_RATE_MASK_ALL ? PSTR("ALL") :
        (fixedRate & FIXED_RATE_MASK_STA ? PSTR("STA") :
        (fixedRate & FIXED_RATE_MASK_AP  ? PSTR("AP")  : PSTR("NONE")));
    uint8_t fixedRateStrIx = rateRes && (fixedRate != FIXED_RATE_MASK_NONE) ? (fixedRate - PHY_RATE_48) : 0;
    const char *fixedRateStr = rateRes && !(fixedRate != FIXED_RATE_MASK_NONE) ?
        (  fixedRateStrIx < NUMOF(skFixedRateStrs) ? skFixedRateStrs[fixedRateStrIx] : PSTR("???") ): PSTR("n/a");

    const uint8_t rateLimitMask = wifi_get_user_limit_rate_mask();
    const char *rateLimitStr =
            rateLimitMask & LIMIT_RATE_MASK_ALL ? PSTR("ALL") :
           (rateLimitMask & LIMIT_RATE_MASK_STA ? PSTR("STA") :
           (rateLimitMask & LIMIT_RATE_MASK_AP  ? PSTR("AP")  : PSTR("NONE")));

    DEBUG("mon: wifi: hostname=%s ip="IPSTR" mask="IPSTR" gw="IPSTR,
        wifi_station_get_hostname(),
        IP2STR(&ipinfo.ip), IP2STR(&ipinfo.netmask), IP2STR(&ipinfo.gw));

    DEBUG("mon: wifi: mode=%s status=%s dhcpc=%s phy=%s rssi=%d sleep=%s fixed=%s@%s limit=%s",
        opmodeStr, statusStr, dhcpcStatusStr, phymodeStr, rssi, sleeptypeStr,
        fixedRateEnStr, fixedRateStr, rateLimitStr);

#if (USER_WIFI_USE_AP > 0)
    struct softap_config apCfg;
    if (wifi_softap_get_config(&apCfg))
    {
        const char *authModeStr = apCfg.authmode < NUMOF(skAuthModeStrs) ?
            skAuthModeStrs[apCfg.authmode] : PSTR("???");
        const uint8_t nSta = wifi_softap_get_station_num();
        const int dhcpsStatus = wifi_softap_dhcps_status();
        const char *dhcpsStatusStr = (dhcpsStatus > 0) && (dhcpsStatus < (int)NUMOF(skDhcpStatusStrs)) ?
            skDhcpStatusStrs[dhcpsStatus] : PSTR("???");

        DEBUG("mon: wifi: ap: ssid=%s%s ch=%d auth=%s conn=%d/%d int=%ums dhcps=%s",
            apCfg.ssid, apCfg.ssid_hidden ? "*" : "", apCfg.channel,
            authModeStr, nSta, apCfg.max_connection, apCfg.beacon_interval, dhcpsStatusStr);

        const struct station_info *pkSta = wifi_softap_get_station_info();
        int staNo = 1;
        while (pkSta)
        {
            DEBUG("mon: wifi: ap: sta[%d]: bssid="MACSTR" ip="IPSTR" rssi=%d",
                staNo++, MAC2STR(pkSta->bssid), IP2STR(&pkSta->ip),
                sWifiGetRssi(pkSta->bssid));
            pkSta = STAILQ_NEXT(pkSta, next);
        }
        wifi_softap_free_station_info();
    }
    else
    {
        DEBUG("mon: wifi: ap: no config");
    }
#endif
}


/* ***** wifi control main ********************************************************************** */

// forward declarations
static void sWifiEventCb(System_Event_t *evt);
#if (USER_WIFI_USE_AP > 0)
static void sWifiStartAp(void);
#endif
static void sWifiStartSta(void);

// start wifi networking
void ICACHE_FLASH_ATTR wifiStart(const bool sta, const bool ap)
{
    // FIXME: handle errors
    tic(0);

    if (!wifi_set_opmode_current(WIFI_OPMODE))
    {
        ERROR("wifi: wifi_set_opmode_current() fail");
    }

    espconn_mdns_disable();

#if (USER_WIFI_USE_AP > 0)
    if (ap)
    {
        // start access point and http server
        sWifiStartAp();
    }
    else
    {
        // FIXME: does this stop it?
        struct softap_config apConfig;
        os_memset(&apConfig, 0, sizeof(apConfig));
        wifi_softap_set_config_current(&apConfig);
    }
#endif

    if (sta)
    {
        // start station
        sWifiStartSta();
    }
    else
    {
        wifi_station_disconnect();
        wifi_station_dhcpc_stop();
    }

    // this won't actually enter modem sleep mode if SoftAP is enabled
    wifi_set_sleep_type(MODEM_SLEEP_T);

    DEBUG("wifiStart(%d, %d) %ums", sta, ap, toc(0)); // FIXME: the measured time doesn't seem right (WTF?!)
}


// start wifi client FIXME: handle errors
static void ICACHE_FLASH_ATTR sWifiStartSta(void)
{
    USER_CFG_t userCfg;
    cfgGet(&userCfg);
    const int staNameLen = os_strlen(userCfg.staName);
    const int staSsidLen = os_strlen(userCfg.staSsid);
    const int staPassLen = os_strlen(userCfg.staPass);

    // stop station
    wifi_station_disconnect();
    wifi_station_dhcpc_stop();

    if ( (staNameLen == 0) || (staSsidLen == 0) )
    {
        PRINT("wifi sta: stop");

        // don't continue configuring station
        return;
    }

    uint8_t staMac[6];
    wifi_get_macaddr(STATION_IF, staMac);

    wifi_station_set_hostname(userCfg.staName);

    static struct station_config stationConfig;
    os_memset(&stationConfig, 0, sizeof(stationConfig));
    stationConfig.bssid_set = 0;
    os_strncpy(stationConfig.ssid,     userCfg.staSsid, sizeof(stationConfig.ssid));
    os_strncpy(stationConfig.password, userCfg.staPass, sizeof(stationConfig.password));

    PRINT("wifi sta: hostname=%s pass=%d staMac="MACSTR,
        userCfg.staName, staPassLen, MAC2STR(staMac));

    if (!wifi_station_set_config_current(&stationConfig))
    {
        ERROR("wifi sta: wifi_station_set_config_current() fail");
    }

    wifi_set_event_handler_cb(sWifiEventCb);

    wifi_station_disconnect();
    wifi_station_dhcpc_start();
    wifi_station_dhcpc_set_maxtry(10);
    wifi_station_set_reconnect_policy(true);
    //wifi_status_led_install(2, PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2); // NodeMCU built-in LED
    //wifi_wps_disable();

    if (!wifi_station_connect())
    {
        ERROR("wifi sta: wifi_station_connect() fail");
    }
}


// -------------------------------------------------------------------------------------------------

static void ICACHE_FLASH_ATTR sWifiEventCb(System_Event_t *evt)
{
    switch (evt->event)
    {
        case EVENT_STAMODE_CONNECTED:
        {
            const Event_StaMode_Connected_t *pkI = &evt->event_info.connected;
            PRINT("wifi sta connect: %s ("MACSTR") channel %02d",
                pkI->ssid, MAC2STR(pkI->bssid), pkI->channel);
            break;
        }

        case EVENT_STAMODE_DISCONNECTED:
        {
            const Event_StaMode_Disconnected_t *pkI = &evt->event_info.disconnected;
            //if (wifi_station_get_connect_status
            WARNING("wifi sta disconnect: %s ("MACSTR"): %s",
                pkI->ssid, MAC2STR(pkI->bssid), wifiErrStr(pkI->reason));
            //deep_sleep_set_option( 0 );
            //system_deep_sleep( 60 * 1000 * 1000 );  // 60 seconds
            break;
        }

        case EVENT_STAMODE_GOT_IP:
        {
            const Event_StaMode_Got_IP_t *pkI = &evt->event_info.got_ip;
            PRINT("wifi sta online: ip=" IPSTR " mask=" IPSTR " gw=" IPSTR,
                IP2STR(&pkI->ip), IP2STR(&pkI->mask), IP2STR(&pkI->gw));
            statusSet(USER_STATUS_HEARTBEAT);
            break;
        }

        case EVENT_STAMODE_AUTHMODE_CHANGE:
        {
            const Event_StaMode_AuthMode_Change_t *pkI = &evt->event_info.auth_change;
            static const char modeStrs[][16] PROGMEM =
            {
                { "OPEN\0" }, { "WEP\0" }, { "WPA_PSK\0" }, { "WPA2_PSK\0" }, { "WPA_WPA2_PSK\0" }, { "???\0" }
            };
            PRINT("wifi sta auth change: %s -> %s",
                modeStrs[ pkI->old_mode < NUMOF(modeStrs) ? pkI->old_mode : NUMOF(modeStrs) - 1 ],
                modeStrs[ pkI->new_mode < NUMOF(modeStrs) ? pkI->new_mode : NUMOF(modeStrs) - 1 ]);
            break;
        }

        case EVENT_STAMODE_DHCP_TIMEOUT:
        {
            WARNING("wifi sta dhcp: timeout");
            break;
        }
#if (USER_WIFI_USE_AP > 0)
        case EVENT_SOFTAPMODE_STACONNECTED:
        {
            const Event_SoftAPMode_StaConnected_t *pkI = &evt->event_info.sta_connected;
            DEBUG("wifi ap sta connected: mac="MACSTR" aid=%d", MAC2STR(pkI->mac), pkI->aid);
            break;
        }
        case EVENT_SOFTAPMODE_STADISCONNECTED:
        {
            const Event_SoftAPMode_StaDisconnected_t *pkI = &evt->event_info.sta_disconnected;
            DEBUG("wifi ap sta disconnected: mac="MACSTR" aid=%d", MAC2STR(pkI->mac), pkI->aid);
            break;
        }
        case EVENT_SOFTAPMODE_PROBEREQRECVED:
        {
            const Event_SoftAPMode_ProbeReqRecved_t *pkI = &evt->event_info.ap_probereqrecved;
            sWifiSetRssi(pkI->mac, pkI->rssi);
            // very noisy!
            //DEBUG("wifi ap probe req recv: mac="MACSTR" rssi=%d", MAC2STR(pkI->mac), pkI->rssi);
            break;
        }
#else
        case EVENT_SOFTAPMODE_STACONNECTED:
        case EVENT_SOFTAPMODE_STADISCONNECTED:
        case EVENT_SOFTAPMODE_PROBEREQRECVED:
#endif
        default:
            WARNING("wifi: event %d", evt->event);
            break;
    }
}


// -------------------------------------------------------------------------------------------------

bool ICACHE_FLASH_ATTR wifiIsOnline(void)
{
    struct ip_info ipinfo;
    wifi_get_ip_info(STATION_IF, &ipinfo);
    return
        (wifi_station_get_connect_status() == STATION_GOT_IP) &&
        (ipinfo.ip.addr != 0) ? true : false;
}


// -------------------------------------------------------------------------------------------------

#if (USER_WIFI_USE_AP > 0)

static struct ip_info sWifiDhcpsIp;

// wifi access point FIXME: handle errors
static void ICACHE_FLASH_ATTR sWifiStartAp(void)
{
    USER_CFG_t userCfg;
    cfgGet(&userCfg);
    const int apSsidLen = os_strlen(userCfg.apSsid);
    const int apPassLen = os_strlen(userCfg.apPass);

    // ap config (part 1)
    static struct softap_config apConfig;
    os_memset(&apConfig, 0, sizeof(apConfig));
    apConfig.max_connection = 4;
    apConfig.beacon_interval = 100;
    apConfig.channel = 6; // doesn't matter, will be same as the station channel
    apConfig.ssid_hidden = 0;

    // stop DHCP server
    if (!wifi_softap_dhcps_stop())
    {
        WARNING("wifi ap: wifi_softap_dhcps_stop() fail");
    }

    // stop AP (?) unless configured
    if (apSsidLen == 0)
    {
        PRINT("wifi ap: stop");
        if (!wifi_softap_set_config_current(&apConfig))
        {
            WARNING("wifi ap: wifi_softap_set_config_current() fail");
        }

        // don't continue configuring AP
        return;
    }

    // configure DHCP server
    IP4_ADDR(&sWifiDhcpsIp.ip,      192, 168,   1,   1);
    IP4_ADDR(&sWifiDhcpsIp.gw,        0,   0,   0,   0); // FIXME: ?
    IP4_ADDR(&sWifiDhcpsIp.netmask, 255, 255, 255,   0);
    if (!wifi_set_ip_info(SOFTAP_IF, &sWifiDhcpsIp))
    {
        WARNING("wifi ap: wifi_set_ip_info() fail");
    }
    struct dhcps_lease dhcpsLease;
    dhcpsLease.enable = true;
    IP4_ADDR(&dhcpsLease.start_ip, 192, 168,   1, 100);
    IP4_ADDR(&dhcpsLease.end_ip,   192, 168,   1, 150);
    if (!wifi_softap_set_dhcps_lease(&dhcpsLease))
    {
        WARNING("wifi ap: wifi_softap_set_dhcps_lease() fail");
    }
    uint8_t mode = true; // FIXME: ?
    if (!wifi_softap_set_dhcps_offer_option(OFFER_ROUTER, &mode))
    {
        WARNING("wifi ap: wifi_softap_set_dhcps_offer_option() fail");
    }

    // start DHCP server
    if (!wifi_softap_dhcps_start())
    {
        WARNING("wifi ap: wifi_softap_dhcps_start() fail");
    }

    uint8_t apMac[6];
    wifi_get_macaddr(SOFTAP_IF, apMac);
    PRINT("wifi ap: ssid=%s pass=%d apMac="MACSTR,
        userCfg.apSsid, apPassLen,  MAC2STR(apMac));
    PRINT("wifi ap: ip="IPSTR" gw="IPSTR" mask="IPSTR,
        IP2STR(&sWifiDhcpsIp.ip), IP2STR(&sWifiDhcpsIp.gw), IP2STR(&sWifiDhcpsIp.netmask));
    PRINT("wifi ap: dhcp="IPSTR"-"IPSTR,
        IP2STR(&dhcpsLease.start_ip), IP2STR(&dhcpsLease.end_ip));

    // configure AP
    apConfig.ssid_len = apSsidLen;
    os_strcpy((char *)apConfig.ssid, userCfg.apSsid);
    if (apPassLen)
    {
        os_strcpy(apConfig.password, userCfg.apPass); // must be long enough! (>= 8 ?)
        apConfig.authmode = AUTH_WPA2_PSK;
    }
    else
    {
        apConfig.authmode = AUTH_OPEN;
    }

    if (!wifi_softap_set_config_current(&apConfig))
    {
        ERROR("wifi ap: wifi_softap_set_config_current() fail");
    }
}

bool ICACHE_FLASH_ATTR wifiIsApNet(const uint8_t ip[4])
{
    struct ip_addr addr;
    IP4_ADDR(&addr, ip[0], ip[1], ip[2], ip[3]);
    const bool res = ip_addr_netcmp(&sWifiDhcpsIp.ip, &addr, &sWifiDhcpsIp.netmask) ? true : false;
    //DEBUG("wifiIsApNet()ip="IPSTR " sip="IPSTR " mask="IPSTR" res=%d",
    //    IP2STR(&addr), IP2STR(&sWifiDhcpsIp.ip), IP2STR(&sWifiDhcpsIp.netmask), res);
    return res;
}

#else
bool ICACHE_FLASH_ATTR wifiIsApNet(const uint8_t ip[4])
{
    UNUSED(ip);
    return false;
}
#endif

/* ***** configuration ************************************************************************** */

static const char sWifiStatusHtml[] PROGMEM = USER_WIFI_STATUS_HTML_STR;

#define WIFI_CONFIG_STATUS_SIZE (sizeof(USER_WIFI_STATUS_HTML_STR) + 512)

// wifi config web interface (/wifi)
static bool ICACHE_FLASH_ATTR sWifiStatusRequestCb(struct espconn *pConn, const HTTPD_REQCB_INFO_t *pkInfo)
{
    char *pResp = memAlloc(WIFI_CONFIG_STATUS_SIZE);
    if (pResp == NULL)
    {
        ERROR("sWifiStatusRequestCb(%p) malloc %u fail", pConn, WIFI_CONFIG_STATUS_SIZE);
        return false;
    }

    strcpy_P(pResp, sWifiStatusHtml);

    USER_CFG_t userCfg;
    cfgGet(&userCfg);

    // station status
    char staStatus[256];
    staStatus[0] = '\0';
    struct station_config staConfig;
    if (wifi_station_get_config(&staConfig))
    {
        uint8_t staMac[6];
        wifi_get_macaddr(STATION_IF, staMac);
        const unsigned int phymode = wifi_get_phy_mode();
        const char *phymodeStr = skPhymodeStrs[phymode < NUMOF(skPhymodeStrs) ? phymode : 0 ];
        const int rssi = wifi_station_get_rssi();
        struct ip_info ipinfo;
        wifi_get_ip_info(STATION_IF, &ipinfo);
        const unsigned int status = wifi_station_get_connect_status();
        const char *statusStr = (status < NUMOF(skStatusStrs)) ? skStatusStrs[status] : PSTR("???");
        sprintf_PP(staStatus,
            PSTR("ssid:   %s\n"),
            userCfg.staSsid);
        sprintf_PP(&staStatus[os_strlen(staStatus)],
            PSTR("mac:    "MACSTR"\n"
                "phy:    %s\n"
                "rssi:   %d\n"
                "status: %s\n"),
            MAC2STR(staMac), phymodeStr, rssi,
            statusStr);
        sprintf_PP(&staStatus[os_strlen(staStatus)],
            PSTR("ip:     "IPSTR"\n"
                "mask:   "IPSTR"\n"
                "gw:     "IPSTR),
            IP2STR(&ipinfo.ip), IP2STR(&ipinfo.netmask), IP2STR(&ipinfo.gw));
    }

    // ap status
    char apStatus[256];
    apStatus[0] = '\0';
#if (USER_WIFI_USE_AP > 0)
    struct softap_config apCfg;
    if (wifi_softap_get_config(&apCfg))
    {
        uint8_t apMac[6];
        wifi_get_macaddr(SOFTAP_IF, apMac);
        const int dhcpsStatus = wifi_softap_dhcps_status();
        const char *dhcpsStatusStr = (dhcpsStatus > 0) && (dhcpsStatus < (int)NUMOF(skDhcpStatusStrs)) ?
            skDhcpStatusStrs[dhcpsStatus] : PSTR("???");
        const char *authModeStr = apCfg.authmode < NUMOF(skAuthModeStrs) ?
            skAuthModeStrs[apCfg.authmode] : PSTR("???");
        sprintf_PP(apStatus,
            PSTR("ssid:   %s%s\n"
                "mac:    "MACSTR"\n"
                "auth:   %s\n"
                "ch:     %d\n"
                "dhcp:   %s\n"
                "ip:     "IPSTR"\n"
                "gw:     "IPSTR"\n"
                "mask:   "IPSTR),
            apCfg.ssid, apCfg.ssid_hidden ? " (cloaked)" : "",
            MAC2STR(apMac), authModeStr, apCfg.channel, dhcpsStatusStr,
            IP2STR(&sWifiDhcpsIp.ip), IP2STR(&sWifiDhcpsIp.gw), IP2STR(&sWifiDhcpsIp.netmask));
        const struct station_info *pkSta = wifi_softap_get_station_info();
        int staNo = 1;
        while (pkSta)
        {
            sprintf_PP(&apStatus[os_strlen(apStatus)],
                PSTR("\n"
                    "sta[%d]: mac:  "MACSTR"\n"
                    "        ip:   "IPSTR"\n"
                    "        rssi: %d"),
                staNo++, MAC2STR(pkSta->bssid), IP2STR(&pkSta->ip),
                sWifiGetRssi(pkSta->bssid));
            pkSta = STAILQ_NEXT(pkSta, next);
        }
        wifi_softap_free_station_info();
    }
#endif


    // render html
    const char *keys[] = { PSTR("STASTATUS"), PSTR("APSTATUS") };
    const char *vals[] = {       staStatus,         apStatus  };

    /*const int htmlLen = */htmlRender(
        pResp, pResp, WIFI_CONFIG_STATUS_SIZE, keys, vals, (int)NUMOF(keys), true);

    const bool res = httpSendHtmlPage(pConn, pResp, false);

    memFree(pResp);

    return res;
}


/* ********************************************************************************************** */
//@}
// eof
