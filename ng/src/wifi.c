/*!
    \file
    \brief flipflip's Tschenggins Lämpli: wifi and network things (see \ref FF_WIFI)

    - Copyright (c) 2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

    \todo SSL connection using mbedtls, see examples/http_get_bearssl
*/

#include "stdinc.h"

#include <lwip/api.h>
#include <lwip/netif.h>

#include "stuff.h"
#include "debug.h"
#include "wifi.h"
#include "cfg_gen.h"
#include "version_gen.h"

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

#define BACKEND_QUERY "cmd=realtime;ascii=1;client=%s;name=%s;stassid="FF_CFG_STASSID";staip="IPSTR";version="FF_BUILDVER

typedef struct WIFI_DATA_s
{
    char            url[ (2 * sizeof(FF_CFG_BACKENDURL)) + (2 * sizeof(BACKEND_QUERY)) ];
    const char     *host;
    const char     *path;
    const char     *query;
    const char     *auth;
    bool            https;
    uint16_t        port;
    ip_addr_t       hostIp;
    ip_addr_t       staIp;
    char            staName[32];
    struct netconn *conn;
    uint32_t        lastHello;
    uint32_t        lastHeartbeat;
    uint32_t        bytesReceived;
} WIFI_DATA_t;

static bool sWifiInit(WIFI_DATA_t *pData)
{
    // initialise data
    memset(pData, 0, sizeof(*pData));

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
    if (sdk_wifi_get_opmode() != STATION_MODE)
    {
        if (!sdk_wifi_set_opmode(STATION_MODE))
        {
            ERROR("wifi: sdk_wifi_set_opmode() fail!");
            return false;
        }
        if (!sdk_wifi_set_opmode_current(STATION_MODE))
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

static bool sWifiConnect(WIFI_DATA_t *pData)
{
    struct sdk_station_config config =
    {
        .ssid = FF_CFG_STASSID, .password = FF_CFG_STAPASS, .bssid_set = false, .bssid = { 0 }
    };
    sdk_wifi_station_set_config(&config);

    //sdk_wifi_station_set_hostname(pData->staName);
    getSystemName(pData->staName, sizeof(pData->staName));
#if LWIP_NETIF_HOSTNAME
    struct netif *netif = sdk_system_get_netif(STATION_IF);
    netif_set_hostname(netif, pData->staName);
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
        uint8_t lastStatus = 0xff;
        while (osTime() < timeout)
        {
            const uint8_t status = sdk_wifi_station_get_connect_status();
            struct ip_info ipinfo;
            sdk_wifi_get_ip_info(STATION_IF, &ipinfo);
            if (status != lastStatus)
            {
                switch (status)
                {
                    case STATION_WRONG_PASSWORD:
                    case STATION_NO_AP_FOUND:
                    case STATION_CONNECT_FAIL:
                        WARNING("wifi: status=%s ip="IPSTR" mask="IPSTR" gw="IPSTR,
                            sdkStationConnectStatusStr(status),
                            IP2STR(&ipinfo.ip), IP2STR(&ipinfo.netmask), IP2STR(&ipinfo.gw));
                        break;
                    default:
                        DEBUG("wifi: status=%s ip="IPSTR" mask="IPSTR" gw="IPSTR,
                            sdkStationConnectStatusStr(status),
                            IP2STR(&ipinfo.ip), IP2STR(&ipinfo.netmask), IP2STR(&ipinfo.gw));
                }
                lastStatus = status;
            }
            if ( (status == STATION_GOT_IP) && (ipinfo.ip.addr != 0) )
            {
                pData->staIp = ipinfo.ip;
                connected = true;
                break;
            }
            osSleep(100);
        }
    }

    return connected;
}

// forward declaration
static void sBackendHandleData(WIFI_DATA_t *pData, char *body);

static bool sWifiConnectBackend(WIFI_DATA_t *pData)
{
    // check and decompose backend URL
    {
        strcpy(pData->url, FF_CFG_BACKENDURL);
        const int urlLen = strlen(pData->url);

        snprintf(&pData->url[urlLen], sizeof(pData->url) - urlLen - 1, "?"BACKEND_QUERY,
            getSystemId(), pData->staName, IP2STR(&pData->staIp));
        DEBUG("wifi: backend url=%s", pData->url);

        const int res = reqParamsFromUrl(pData->url, pData->url, sizeof(pData->url),
            &pData->host, &pData->path, &pData->query, &pData->auth, &pData->https, &pData->port);
        if (res)
        {
            DEBUG("wifi: host=%s path=%s query=%s auth=%s https=%s, port=%u",
                pData->host, pData->path, pData->query, pData->auth, pData->https ? "yes" : "no", pData->port);
        }
        else
        {
            ERROR("wifi: fishy backend url!");
            return false;
        }
    }

    // get IP of backend server
    {
        DEBUG("wifi: DNS lookup %s", pData->host);
        const err_t err = netconn_gethostbyname(pData->host, &pData->hostIp);
        if (err != ERR_OK)
        {
            ERROR("wifi: DNS query for %s failed: %s",
                pData->host, lwipErrStr(err));
            return false;
        }
    }

    // connect to backend server
    {
        pData->conn = netconn_new(NETCONN_TCP);
        DEBUG("wifi: connect "IPSTR, IP2STR(&pData->hostIp));
        const err_t err = netconn_connect(pData->conn, &pData->hostIp, pData->port);
        if (err != ERR_OK)
        {
            ERROR("wifi: connect to "IPSTR":%u failed: %s",
                IP2STR(&pData->hostIp), pData->port, lwipErrStr(err));
            return false;
        }
    }

    // make HTTP POST request
    {
        char req[sizeof(pData->url) + 128];
        snprintf(req, sizeof(req),
            "POST /%s HTTP/1.1\r\n"           // HTTP POST request
                "Host: %s\r\n"                // provide host name for virtual host setups
                "Authorization: Basic %s\r\n" // okay to provide empty one?
                "User-Agent: "FF_PROGRAM"/"FF_BUILDVER"\r\n"  // be nice
                "Content-Length: %d\r\n"      // length of query parameters
                "\r\n"                        // end of request headers
                "%s",                         // query parameters (FIXME: urlencode!)
            pData->path,
            pData->host,
            pData->auth != NULL ? pData->auth : "",
            strlen(pData->query),
            pData->query);
        DEBUG("wifi: request POST /%s: %s", pData->path, pData->query);
        const err_t err = netconn_write(pData->conn, req, strlen(req), NETCONN_COPY);
        if (err != ERR_OK)
        {
            ERROR("wifi: POST /%s failed: %s", pData->path, lwipErrStr(err));
            netconn_delete(pData->conn);
            return false;
        }
    }

    // receive header
    bool backendReady = false;
    struct netbuf *buf = NULL;
    while (true)
    {
        const err_t errRecv = netconn_recv(pData->conn, &buf);
        if (errRecv != ERR_OK)
        {
            ERROR("wifi: read failed: %s", lwipErrStr(errRecv));
            break;
        }
        // check data for HTTP response
        // (note: not handling multiple netbufs -- should not be necessary)
        void *data;
        uint16_t len;
        const err_t errData = netbuf_data(buf, &data, &len);
        if (errData != ERR_OK)
        {
            ERROR("wifi: netbuf_data() failed: %s", lwipErrStr(errData));
            break;
        }
        char *pParse = (char *)data;
        //DEBUG("wifi: recv [%u] %s", len, pParse);
        pData->bytesReceived += len;

        // first line: "HTTP/1.1 200 OK\r\n"
        char *firstLineStart = pParse;
        char *statusCode = &pParse[9]; // "200 OK\r\n"
        char *firstLineEnd = strstr(firstLineStart, "\r\n");
        if ( (firstLineEnd == NULL) || (strncmp(firstLineStart, "HTTP/1.1 ", 8) != 0) )
        {
            ERROR("wifi: response is not HTTP/1.1");
            break;
        }
        const int status = atoi(statusCode);
        *firstLineEnd = '\0';
        DEBUG("wifi: %s (code %d)", firstLineStart, status);
        if (status != 200)
        {
            ERROR("wifi: illegal response: %s", statusCode);
            break;
        }
        pParse = firstLineEnd + 2;

        // seek to end of header
        char *pBody = strstr(pParse, "\r\n\r\n");
        if ( (pBody == NULL) || (strlen(pBody) < 10) )
        {
            ERROR("wifi: no response (maybe redirect?)");
            break;
        }

        // look for "hello"
        char *pHello = strstr(pBody, "\r\nhello ");
        if (pHello == NULL)
        {
            ERROR("wifi: no hello from backend");
            break;
        }
        pHello += 2;
        char *endOfHello = strstr(pHello, "\r\n");
        if (endOfHello)
        {
            *endOfHello = '\0';
        }
        DEBUG("wifi: %s", pHello);
        pData->lastHello = osTime();

        // handle remaining data
        pParse = endOfHello + 2;
        sBackendHandleData(pData, pParse);

        // we should be fine...
        backendReady = true;
        break;
    }
    if (buf != NULL)
    {
        netbuf_free(buf);
        netbuf_delete(buf);
    }

    if (backendReady)
    {
        netconn_shutdown(pData->conn, false, true); // no more tx
        return true;
    }
    else
    {
        ERROR("wifi: no or illegal response from backend");
        netconn_close(pData->conn);
        netconn_delete(pData->conn);
        return false;
    }
}

static void sBackendHandleData(WIFI_DATA_t *pData, char *resp)
{
    //DEBUG("sBackendHandleData() %s", resp);

    char *pStatus    = strstr(resp, "\r\nstatus ");
    char *pHeartbeat = strstr(resp, "\r\nheartbeat ");

    // "\r\nheartbeat 1491146601 25\r\n"
    if (pHeartbeat != NULL)
    {
        pHeartbeat += 2;
        char *endOfHeartbeat = strstr(pHeartbeat, "\r\n");
        if (endOfHeartbeat != NULL)
        {
            *endOfHeartbeat = '\0';
        }
        DEBUG("wifi: heartbeat (%s)", pHeartbeat);
        pData->lastHeartbeat = osTime(); // POSIX time: (uint32_t)atoi(&pHeartbeat[10]);
    }
    // "\r\nstatus 1491146576 json={"leds": ... }\r\n"
    else if (pStatus != NULL)
    {
        pStatus += 2;
        char *endOfStatus = strstr(pStatus, "\r\n");
        if (endOfStatus != NULL)
        {
            *endOfStatus = '\0';
        }
        char *pJson = strstr(&pStatus[7], " ");
        if (pJson != NULL)
        {
            *pJson = '\0';
            pJson += 1;
            pData->lastHeartbeat = osTime(); // POSIX: (uint32_t)atoi(&pStatus[7]);
            const int jsonLen = strlen(pJson);
            DEBUG("wifi: status [%d] %s", jsonLen, pJson);
        }
        else
        {
            WARNING("wifi: ignoring fishy status");
        }
    }

}


static bool sWifiHandleConnection(WIFI_DATA_t *pData)
{
    bool okay = true;
    while (okay)
    {
        struct netbuf *buf = NULL;
        const err_t errRecv = netconn_recv(pData->conn, &buf);
        if (errRecv != ERR_OK)
        {
            ERROR("wifi: read failed: %s", lwipErrStr(errRecv));
            okay = false;
            break;
        }
        // check data for HTTP response
        // (note: not handling multiple netbufs -- should not be necessary)
        void *resp;
        uint16_t len;
        const err_t errData = netbuf_data(buf, &resp, &len);
        if (errData != ERR_OK)
        {
            ERROR("wifi: netbuf_data() failed: %s", lwipErrStr(errData));
            okay = false;
        }
        else
        {
            //DEBUG("wifi: recv [%u] %s", len, (const char *)resp);
            pData->bytesReceived += len;

            sBackendHandleData(pData, (char *)resp);
        }
        netbuf_free(buf);
        netbuf_delete(buf);

        // check heartbeat
        if ( (osTime() - pData->lastHeartbeat) > 30000 )
        {
            ERROR("wifi: lost heartbeat");
            okay = false;
        }

        if (okay)
        {
            osSleep(100);
        }
    }

    return okay;
}

static bool sWifiIsOnline(void)
{
    const uint8_t status = sdk_wifi_station_get_connect_status();
    struct ip_info ipinfo;
    sdk_wifi_get_ip_info(STATION_IF, &ipinfo);
    return (status == STATION_GOT_IP) && (ipinfo.ip.addr != 0) ? true : false;
}


// forward declaration
static void sWifiMonStatusSet(const WIFI_STATE_t *pkState, const WIFI_DATA_t *pkData);

static void sWifiTask(void *pArg)
{

    static WIFI_STATE_t sWifiState = WIFI_STATE_UNKNOWN;
    static WIFI_DATA_t sWifiData;
    sWifiMonStatusSet(&sWifiState, &sWifiData);
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
                PRINT("wifi: state unknown, initialising...");
                if (!sWifiInit(&sWifiData))
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
                if (sWifiConnect(&sWifiData))
                {
                    sWifiState = WIFI_STATE_ONLINE;
                }
                else
                {
                    sWifiState = WIFI_STATE_FAIL;
                }

                break;
            }

            // we're connected to the AP --> connect to the backend
            case WIFI_STATE_ONLINE:
            {
                PRINT("wifi: state online, connecting backend...!");
                if (sWifiConnectBackend(&sWifiData))
                {
                    sWifiState = WIFI_STATE_CONNECTED;
                }
                else
                {
                    sWifiState = WIFI_STATE_FAIL;
                }
                break;
            }

            case WIFI_STATE_CONNECTED:
            {
                PRINT("wifi: state connected");
                if (sWifiHandleConnection(&sWifiData))
                {
                    sWifiState = sWifiIsOnline() ? WIFI_STATE_ONLINE : WIFI_STATE_UNKNOWN;
                }
                else
                {
                    sWifiState = WIFI_STATE_FAIL;
                }
                break;
            }

            case WIFI_STATE_FAIL:
            {
                static uint32_t lastFail;
                const uint32_t now = osTime();
                const uint32_t waitTime = (now - lastFail) > 300000 ? 5000 : 60000;
                lastFail = now;
                PRINT("wifi: failure... waiting %ums", waitTime);
                osSleep(waitTime);

                sWifiState = sWifiIsOnline() ? WIFI_STATE_ONLINE : WIFI_STATE_UNKNOWN;

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

static const WIFI_STATE_t *spkState;
static const WIFI_DATA_t *spkData;

static void sWifiMonStatusSet(const WIFI_STATE_t *pkState, const WIFI_DATA_t *pkData)
{
    spkState = pkState;
    spkData = pkData;
}

void wifiMonStatus(void)
{
    if ( (spkState != NULL) && (spkData != NULL) )
    {
        const uint32_t now = osTime();
        DEBUG("mon: wifi: state=%s uptime=%u heartbeat=%u bytes=%u",
            sWifiStateStr(*spkState), now - spkData->lastHello, now - spkData->lastHeartbeat,
            spkData->bytesReceived);
    }

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

    //sdk_wifi_status_led_install(2, PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);

    xTaskCreate(sWifiTask, "ff_wifi", 1024, NULL, 2, NULL);
}

/* ********************************************************************************************** */

// eof
