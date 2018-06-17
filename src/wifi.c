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

#include <bearssl.h>

#include "jsmn.h"

#include "stuff.h"
#include "debug.h"
#include "wifi.h"
#include "status.h"
#include "backend.h"
#include "jenkins.h"
#include "cfg_gen.h"
#include "version_gen.h"
#include "crt_gen.h"


#if (!defined FF_CFG_STASSID || !defined FF_CFG_STAPASS || !defined FF_CFG_BACKENDURL)
#  define HAVE_CONFIG 0
#  warning incomplete or missing configuration
#else
#  define HAVE_CONFIG 1
#endif

/* ********************************************************************************************** */

#if (HAVE_CONFIG > 0)

// the state of the wifi (network) connection
typedef enum WIFI_STATE_e
{
    WIFI_STATE_OFFLINE = 0, // offline --> wait for station connect
    WIFI_STATE_ONLINE,      // station online --> connect to backend
    WIFI_STATE_CONNECTED,   // backend connected
    WIFI_STATE_FAIL,        // failure (e.g. connection lost) --> initialise
} WIFI_STATE_t;

static const char *sWifiStateStr(const WIFI_STATE_t state)
{
    switch (state)
    {
        case WIFI_STATE_OFFLINE:    return "OFFLINE";
        case WIFI_STATE_ONLINE:     return "ONLINE";
        case WIFI_STATE_CONNECTED:  return "CONNECTED";
        case WIFI_STATE_FAIL:       return "FAIL";
    }
    return "???";
}

// query parameters for the backend
#define BACKEND_QUERY "cmd=realtime;ascii=1;client=%s;name=%s;stassid="FF_CFG_STASSID";staip="IPSTR";version="FF_BUILDVER";maxch="STRINGIFY(JENKINS_MAX_CH)

// wifi (network) state data
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
    bool            backendReady;
#if (HAVE_CRT)
    // buffer for SSL tx/rx and state, should be BR_SSL_BUFSIZE_MONO, but seems to work fine if smaller
    uint8_t                 bearSslBuf[BR_SSL_BUFSIZE_MONO/4];
    br_ssl_client_context   bearSslClientCtx;
    br_x509_minimal_context bearSslCertCtx;
    br_sslio_context        bearSslIoCtx;
#endif
} WIFI_DATA_t;

// -------------------------------------------------------------------------------------------------

static WIFI_STATE_t sWifiState;
static WIFI_DATA_t sWifiData;

#define WIFI_CONNECT_TIMEOUT 30

// wait for wifi station connect
static bool sWifiWaitConnect(void)
{
    // wait for connection to come up
    bool connected = false;
    const uint32_t now = osTime();
    const uint32_t timeout = now + (WIFI_CONNECT_TIMEOUT * 1000);
    uint8_t lastStatus = 0xff;
    int n = 0;
    while (osTime() < timeout)
    {
        const uint8_t status = sdk_wifi_station_get_connect_status();
        struct ip_info ipinfo;
        sdk_wifi_get_ip_info(STATION_IF, &ipinfo);
        const bool statusChanged = (status != lastStatus);
        const bool printStatus = ((n % 50) ==  0);
        if (statusChanged || printStatus)
        {
            switch (status)
            {
                case STATION_WRONG_PASSWORD:
                case STATION_NO_AP_FOUND:
                case STATION_CONNECT_FAIL:
                    WARNING("wifi: status=%s ip="IPSTR" mask="IPSTR" gw="IPSTR" (%s, %us left)",
                        sdkStationConnectStatusStr(status),
                        IP2STR(&ipinfo.ip), IP2STR(&ipinfo.netmask), IP2STR(&ipinfo.gw),
                        statusChanged ? "changed" : "still trying", (timeout - osTime() + 500) / 1000);
                    break;
                default:
                    DEBUG("wifi: status=%s ip="IPSTR" mask="IPSTR" gw="IPSTR" (%s, %us left)",
                        sdkStationConnectStatusStr(status),
                        IP2STR(&ipinfo.ip), IP2STR(&ipinfo.netmask), IP2STR(&ipinfo.gw),
                    statusChanged ? "changed" : "still trying", (timeout - osTime() + 500) / 1000);
            }
            lastStatus = status;
        }
        if ( (status == STATION_GOT_IP) && (ipinfo.ip.addr != 0) )
        {
            sWifiData.staIp = ipinfo.ip;
            connected = true;
            PRINT("wifi: online after %.3fs", (double)(osTime() - now) * 1e-3);
            break;
        }
        osSleep(100);
        n++;
    }

    return connected;
}


#if (HAVE_CRT)
static int sWifiBearSslWriteFunc(void *ctx, const unsigned char *buf, size_t len)
{
    struct netconn *conn = (struct netconn *)ctx;
    //DEBUG("wifi: ssl write %u", len);
    const err_t err = netconn_write(conn, buf, len, NETCONN_COPY);
    if (err != ERR_OK)
    {
        ERROR("wifi: ssl write %u: %s", len, lwipErrStr(err));
        return -1;
    }
    return (int)len;
}

static int sWifiBearSslReadTimeout = 0;

static uint8_t sWifiBearSslRxBuf[1024]; // buffer for application

static int sWifiBearSslReadFunc(void *ctx, unsigned char *outBuf, size_t outLen)
{
    struct netconn *conn = (struct netconn *)ctx;

    // we may receive more data (from LWIP) than is requested (by BearSSL), so keep track of that
    // and only receive more when needed, and only return (up to) as much as is requested
    static struct netbuf *sReadBuf      = NULL;
    static uint8_t       *sReadData     = NULL;
    static uint16_t       sReadDataLen  = 0;
    static uint16_t       sReadDataOffs = 0;

    // try to receive more
    if (sReadBuf == NULL)
    {
        const err_t errRecv = netconn_recv(conn, &sReadBuf);
        if (errRecv == ERR_WOULDBLOCK)
        {
            if (sWifiBearSslReadTimeout > 0)
            {
                sWifiBearSslReadTimeout--;
                if (sWifiBearSslReadTimeout <= 0)
                {
                    ERROR("wifi: ssl receive timeout");
                    return -1;
                }
                osSleep(100);
            }
            return 0;
        }
        if (errRecv != ERR_OK)
        {
            ERROR("wifi: ssl receive: %s", lwipErrStr(errRecv));
            return -1;
        }

        void *data;
        const err_t errData = netbuf_data(sReadBuf, &data, &sReadDataLen);
        if (errData != ERR_OK)
        {
            ERROR("wifi: ssl receive buf: %s", lwipErrStr(errData));
            netbuf_free(sReadBuf);
            netbuf_delete(sReadBuf);
            sReadBuf = NULL;
            return -1;
        }

        sReadData = (uint8_t *)data;
        sReadDataOffs = 0;
        //DEBUG("wifi: ssl receive %u", sReadDataLen);
    }

    // have anything in buffer to return?
    if ( (sReadData != NULL) && (sReadDataOffs < sReadDataLen) )
    {
        const uint16_t haveLen = sReadDataLen - sReadDataOffs;
        const uint16_t copyLen = MIN(outLen, haveLen);

        //DEBUG("wifi: ssl read %u/%u (%u left)", copyLen, outLen, haveLen - copyLen);

        memcpy(outBuf, &sReadData[sReadDataOffs], copyLen);
        sReadDataOffs += copyLen;
        //HEXDUMP(outBuf, MIN(copyLen, 32 * 10);

        // clean up if there's no more data in the buffer
        if (sReadDataOffs >= sReadDataLen)
        {
            //DEBUG("wifi: ssl read done");
            netbuf_free(sReadBuf);
            netbuf_delete(sReadBuf);
            sReadBuf = NULL;
            sReadData = NULL;
            sReadDataLen = 0;
            sReadDataOffs = 0;
        }

        return (int)copyLen;
    }

    // we shouldn't end up here
    WARNING("wifi: ssl read nothing");
    return -1;
}

static void sWifiBearSslAddEntropy(void)
{
    // BearSSL wants "at least 80 bits, preferably 128 bit or more")
    for (int i = 0; i < 10; i++)
    {
        uint32_t rand = hwrand();
        br_ssl_engine_inject_entropy(&sWifiData.bearSslClientCtx.eng, &rand, sizeof(rand));
    }
}
#endif // HAVE_CRT


// connect to backend
static bool sWifiConnectBackend(void)
{
    // check and decompose backend URL
    {
        strcpy(sWifiData.url, FF_CFG_BACKENDURL);
        const int urlLen = strlen(sWifiData.url);

        snprintf(&sWifiData.url[urlLen], sizeof(sWifiData.url) - urlLen - 1, "?"BACKEND_QUERY,
            getSystemId(), sWifiData.staName, IP2STR(&sWifiData.staIp));
        DEBUG("wifi: backend url=%s", sWifiData.url);

        const int res = reqParamsFromUrl(sWifiData.url, sWifiData.url, sizeof(sWifiData.url),
            &sWifiData.host, &sWifiData.path, &sWifiData.query, &sWifiData.auth, &sWifiData.https, &sWifiData.port);
        if (res)
        {
            DEBUG("wifi: host=%s path=%s query=%s auth=%s https=%s, port=%u",
                sWifiData.host, sWifiData.path, sWifiData.query, sWifiData.auth, sWifiData.https ? "yes" : "no", sWifiData.port);
        }
        else
        {
            ERROR("wifi: fishy backend url!");
            return false;
        }
#if (!HAVE_CRT)
        if (sWifiData.https)
        {
            ERROR("wifi: https (ssl) support not compiled-in!");
            return false;
        }
#endif
    }

#if (HAVE_CRT)
    // initialise BearSSL engine (à la esp-open-rtos/examples/http_get_bearssl/http_get_bearssl.c)
    DEBUG("wifi: ssl init");
    memset(sWifiData.bearSslBuf,        0, sizeof(sWifiData.bearSslBuf));
    memset(&sWifiData.bearSslClientCtx, 0, sizeof(sWifiData.bearSslClientCtx));
    memset(&sWifiData.bearSslCertCtx,   0, sizeof(sWifiData.bearSslCertCtx));
    memset(&sWifiData.bearSslIoCtx,     0, sizeof(sWifiData.bearSslIoCtx));
    br_ssl_client_init_full(&sWifiData.bearSslClientCtx, &sWifiData.bearSslCertCtx, TAs, TAs_NUM);
    br_ssl_engine_set_buffer(&sWifiData.bearSslClientCtx.eng, sWifiData.bearSslBuf, sizeof(sWifiData.bearSslBuf), 0);
    sWifiBearSslAddEntropy();
    if (br_ssl_client_reset(&sWifiData.bearSslClientCtx, sWifiData.host, 0) == 0)
    {
        ERROR("wifi: ssl init");
        return false;
    }
    // use compile time as "now" (for certificate expiration check)
    sWifiData.bearSslCertCtx.days = (CRT_TODAY / 86400) + 719528;
    sWifiData.bearSslCertCtx.seconds = CRT_TODAY % 86400;
#endif

    // get IP of backend server
    {
        DEBUG("wifi: DNS lookup %s", sWifiData.host);
        const err_t err = netconn_gethostbyname(sWifiData.host, &sWifiData.hostIp);
        if (err != ERR_OK)
        {
            ERROR("wifi: DNS query for %s failed: %s",
                sWifiData.host, lwipErrStr(err));
            return false;
        }
    }

    // connect to backend server
    {
        sWifiData.conn = netconn_new(NETCONN_TCP);
        DEBUG("wifi: connect "IPSTR":%u", IP2STR(&sWifiData.hostIp), sWifiData.port);
        const err_t err = netconn_connect(sWifiData.conn, &sWifiData.hostIp, sWifiData.port);
        if (err != ERR_OK)
        {
            ERROR("wifi: connect to "IPSTR":%u failed: %s",
                IP2STR(&sWifiData.hostIp), sWifiData.port, lwipErrStr(err));
            return false;
        }
    }

#if (HAVE_CRT)
    // hook connection into ssl engine
    br_sslio_init(&sWifiData.bearSslIoCtx, &sWifiData.bearSslClientCtx.eng,
        sWifiBearSslReadFunc, sWifiData.conn, sWifiBearSslWriteFunc, sWifiData.conn);
#endif

    // make HTTP POST request
    {
        char req[sizeof(sWifiData.url) + 128];
        snprintf(req, sizeof(req),
            "POST /%s HTTP/1.1\r\n"           // HTTP POST request
                "Host: %s\r\n"                // provide host name for virtual host setups
                "Authorization: Basic %s\r\n" // okay to provide empty one?
                "User-Agent: "FF_PROGRAM"/"FF_BUILDVER"\r\n"  // be nice
                "Content-Length: %d\r\n"      // length of query parameters
                "\r\n"                        // end of request headers
                "%s",                         // query parameters (FIXME: urlencode!)
            sWifiData.path,
            sWifiData.host,
            sWifiData.auth != NULL ? sWifiData.auth : "",
            strlen(sWifiData.query),
            sWifiData.query);
        DEBUG("wifi: request POST /%s: %s", sWifiData.path, sWifiData.query);

#if (HAVE_CRT)
        if (sWifiData.https)
        {
            if ( (br_sslio_write_all(&sWifiData.bearSslIoCtx, req, strlen(req)) != BR_ERR_OK) ||
                 (br_sslio_flush(&sWifiData.bearSslIoCtx)                       != BR_ERR_OK) )
            {
                ERROR("wifi: ssl POST /%s: %s", sWifiData.path,
                    bearSslErrStr(br_ssl_engine_last_error(&sWifiData.bearSslClientCtx.eng), NULL));
                netconn_delete(sWifiData.conn);
                sWifiData.conn = NULL;
                // FIXME: shutdown BearSSL engine?
                return false;
            }
        }
        else
#endif
        {
            const err_t err = netconn_write(sWifiData.conn, req, strlen(req), NETCONN_COPY);
            if (err != ERR_OK)
            {
                ERROR("wifi: POST /%s: %s", sWifiData.path, lwipErrStr(err));
                netconn_delete(sWifiData.conn);
                sWifiData.conn = NULL;
                return false;
            }
        }
    }

    // from now on make the receive non-blocking
    netconn_set_nonblocking(sWifiData.conn, true);
    // note that the BearSSL stuff still behaves blocking, see sWifiBearSslReadFunc() and sWifiBearSslReadTimeout

    // receive header
    sWifiData.backendReady = false;
    struct netbuf *buf = NULL;
    int helloTimeout = 10000 / 100;
    while (true)
    {
        uint16_t rxLen = 0;
        uint8_t *rxBuf = NULL;
#if (HAVE_CRT)
        if (sWifiData.https)
        {
            sWifiBearSslReadTimeout = helloTimeout;
            const int len = br_sslio_read(&sWifiData.bearSslIoCtx, sWifiBearSslRxBuf, sizeof(sWifiBearSslRxBuf) - 1);

            // maybe received something
            const int brErr = br_ssl_engine_last_error(&sWifiData.bearSslClientCtx.eng);
            if ( (brErr != BR_ERR_OK) || (len < 0) )
            {
                ERROR("wifi: ssl read: %s", bearSslErrStr(brErr, NULL));
                break;
            }

            // got data
            rxBuf = sWifiBearSslRxBuf;
            rxLen = len;
        }
        else
#endif
        {
            const err_t errRecv = netconn_recv(sWifiData.conn, &buf);
            // no more data at the moment
            if (errRecv == ERR_WOULDBLOCK)
            {
                helloTimeout--;
                // give up
                if (helloTimeout < 0)
                {
                    ERROR("wifi: response timeout");
                    break;
                }
                // wait for more data
                else
                {
                    osSleep(100);
                    continue;
                }
            }
            if (errRecv != ERR_OK)
            {
                ERROR("wifi: read: %s", lwipErrStr(errRecv));
                break;
            }

            // (note: not handling multiple netbufs -- should not be necessary)
            void *data;
            uint16_t len;
            const err_t errData = netbuf_data(buf, &data, &len);
            if (errData != ERR_OK)
            {
                ERROR("wifi: netbuf_data(): %s", lwipErrStr(errData));
                break;
            }
            rxBuf = (uint8_t *)data;
            rxLen = len;
        }

        // check data for HTTP response
        char *pParse = (char *)rxBuf;
        pParse[rxLen] = '\0'; // make sure it's nul terminated
        //DEBUG("wifi: recv [%u] %s", len, pParse);

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

        sWifiData.backendReady = backendConnect(pBody, (int)rxLen - (pBody - (char *)rxBuf));
        break;
    }
    if (buf != NULL)
    {
        netbuf_free(buf);
        netbuf_delete(buf);
        buf = NULL;
    }

    if (sWifiData.backendReady)
    {
        // don't close tx for SSL connections (renegotiations and such) FIXME: required?
        if (!sWifiData.https)
        {
            netconn_shutdown(sWifiData.conn, false, true); // no more tx
        }
        return true;
    }
    else
    {
        ERROR("wifi: no or illegal response from backend");
        netconn_close(sWifiData.conn);
        netconn_delete(sWifiData.conn);
        sWifiData.conn = NULL;
        return false;
    }
}

// handle backend connection (wait for more data)
// return true to force immediate reconnect, false for reconnecting later
static bool sWifiHandleConnection(void)
{
    bool res = true;
    bool keepGoing = true;
    while (keepGoing)
    {
        // check if backend is okay
        if (!backendIsOkay())
        {
            res = false;
            break;
        }

        // read more data from the connection
        struct netbuf *buf = NULL;
        uint16_t rxLen = 0;
        uint8_t *rxBuf = NULL;

#if (HAVE_CRT)
        if (sWifiData.https)
        {
            sWifiBearSslReadTimeout = 15000 / 100;
            const int len = br_sslio_read(&sWifiData.bearSslIoCtx, sWifiBearSslRxBuf, sizeof(sWifiBearSslRxBuf) - 1);

            // maybe received something
            const int brErr = br_ssl_engine_last_error(&sWifiData.bearSslClientCtx.eng);
            if ( (brErr != BR_ERR_OK) || (len < 0) )
            {
                ERROR("wifi: ssl read: %s", bearSslErrStr(brErr, NULL));
                res = false;
                break;
            }

            // got data
            rxBuf = sWifiBearSslRxBuf;
            rxLen = len;
        }
        else
#endif
        {
            const err_t errRecv = netconn_recv(sWifiData.conn, &buf);

            // no more data at the moment
            if (errRecv == ERR_WOULDBLOCK)
            {
                osSleep(23);
                continue;
            }

            if (errRecv != ERR_OK)
            {
                ERROR("wifi: read: %s", lwipErrStr(errRecv));
                res = false;
                break;
            }

            // check data
            // (note: not handling multiple netbufs -- should not be necessary)
            void *data;
            uint16_t len;
            const err_t errData = netbuf_data(buf, &data, &len);
            if (errData != ERR_OK)
            {
                ERROR("wifi: netbuf_data(): %s", lwipErrStr(errData));
                res = false;
                break;
            }

            // got data
            rxBuf = (uint8_t *)data;
            rxLen = len;
        }

        if (rxLen > 0)
        {
            // convert response to string (nul-terminate it)
            char *respStr = (char *)rxBuf;
            respStr[rxLen] = '\0';
            //DEBUG("wifi: recv [%u] %s", len, respStr);
            const BACKEND_STATUS_t status = backendHandle(respStr, (int)rxLen);
            switch (status)
            {
                case BACKEND_STATUS_OKAY:                                      break;
                case BACKEND_STATUS_FAIL:      keepGoing = false; res = false; break;
                case BACKEND_STATUS_RECONNECT: keepGoing = false; res = true;  break;
            }
        }

        if (buf != NULL)
        {
            netbuf_free(buf);
            netbuf_delete(buf);
            buf = NULL;
        }
    }

    netconn_close(sWifiData.conn);
    netconn_delete(sWifiData.conn);
    sWifiData.conn = NULL;

    sWifiData.backendReady = false;
    backendDisconnect();

    return res;
}


// -------------------------------------------------------------------------------------------------

static bool sWifiIsOnline(void)
{
    const uint8_t status = sdk_wifi_station_get_connect_status();
    struct ip_info ipinfo;
    sdk_wifi_get_ip_info(STATION_IF, &ipinfo);
    return (status == STATION_GOT_IP) && (ipinfo.ip.addr != 0) ? true : false;
}

static void sWifiTask(void *pArg)
{
    // doesn't seem to work in wifiInit() (user_init())
    //sdk_wifi_station_set_hostname(sWifiData.staName);
#if LWIP_NETIF_HOSTNAME
    struct netif *netif = sdk_system_get_netif(STATION_IF);
    sdk_wifi_station_disconnect();
    netif_set_hostname(netif, sWifiData.staName);
    sdk_wifi_station_connect();
#endif

    WIFI_STATE_t oldState = WIFI_STATE_OFFLINE;
    while (true)
    {
        if (oldState != sWifiState)
        {
            DEBUG("wifi: %s -> %s", sWifiStateStr(oldState), sWifiStateStr(sWifiState));
            oldState = sWifiState;
        }

        switch (sWifiState)
        {
            // we're offline --> wait for station connect
            case WIFI_STATE_OFFLINE:
            {
                PRINT("wifi: state offline, waiting for station connect...");
                statusNoise(STATUS_NOISE_ABORT);
                statusLed(STATUS_LED_UPDATE);
                if (sWifiWaitConnect())
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
                PRINT("wifi: state online, connecting backend...");
                if (sWifiConnectBackend())
                {
                    sWifiState = WIFI_STATE_CONNECTED;
                }
                else
                {
                    sWifiState = WIFI_STATE_FAIL;
                }
                break;
            }

            // connected to backend --> handle connection
            case WIFI_STATE_CONNECTED:
            {
                PRINT("wifi: state connected...");
                statusNoise(STATUS_NOISE_ONLINE);
                statusLed(STATUS_LED_HEARTBEAT);
                if (sWifiHandleConnection())
                {
                    sWifiState = sWifiIsOnline() ? WIFI_STATE_ONLINE : WIFI_STATE_OFFLINE;
                }
                else
                {
                    sWifiState = WIFI_STATE_FAIL;
                }
                break;
            }

            // something has failed --> wait a bit
            case WIFI_STATE_FAIL:
            {
                static uint32_t lastFail;
                const uint32_t now = osTime();
                int waitTime = (now - lastFail) > (1000 * BACKEND_STABLE_CONN_THRS) ?
                    BACKEND_RECONNECT_INTERVAL : BACKEND_RECONNECT_INTERVAL_SLOW;
                lastFail = now;
                statusNoise(STATUS_NOISE_FAIL);
                statusLed(STATUS_LED_FAIL);
                PRINT("wifi: failure... waiting %us", waitTime);
                while (waitTime > 0)
                {
                    osSleep(1000);
                    if ( (waitTime < 10) || ((waitTime % 10) == 0) )
                    {
                        DEBUG("wifi: wait... %d", waitTime);
                    }
                    if (waitTime <= 3)
                    {
                        statusNoise(STATUS_NOISE_TICK);
                    }
                    waitTime--;
                }
                sWifiState = sWifiIsOnline() ? WIFI_STATE_ONLINE : WIFI_STATE_OFFLINE;

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
    sdk_wifi_set_opmode_current(STATION_MODE);
    while (true)
    {
        static uint32_t sTick;
        vTaskDelayUntil(&sTick, MS2TICKS(WIFI_SCAN_PERIOD));
        PRINT("wifi: no config -- initiating wifi scan");
        struct sdk_scan_config cfg = { .ssid = NULL, .bssid = NULL, .channel = 0, .show_hidden = true };
        sdk_wifi_station_scan(&cfg, sWifiScanDoneCb);
    }
}

#endif // (HAVE_CONFIG > 0)


/* ********************************************************************************************** */

void wifiMonStatus(void)
{
    const char *mode   = sdkWifiOpmodeStr( sdk_wifi_get_opmode() );
    const char *status = sdkStationConnectStatusStr( sdk_wifi_station_get_connect_status() );
    const char *dhcp   = sdkDhcpStatusStr( sdk_wifi_station_dhcpc_status() );
    const char *phy    = sdkWifiPhyModeStr( sdk_wifi_get_phy_mode() );
    const char *sleep  = sdkWifiSleepTypeStr( sdk_wifi_get_sleep_type() );
    const uint8_t ch   = sdk_wifi_get_channel();
    DEBUG("mon: wifi: state=%s mode=%s status=%s dhcp=%s phy=%s sleep=%s ch=%u",
#if (HAVE_CONFIG > 0)
        sWifiStateStr(sWifiState),
#else
        "n/a",
#endif
        mode, status, dhcp, phy, sleep, ch);

    struct ip_info ipinfo;
    sdk_wifi_get_ip_info(STATION_IF, &ipinfo);
#if LWIP_NETIF_HOSTNAME
    struct netif *netif = sdk_system_get_netif(STATION_IF);
    const char *name = netif_get_hostname(netif);
#else
    const char *name = NULL;
#endif
    if (name == NULL)
    {
        name = "???";
    }
    DEBUG("mon: wifi: name=%s ssid="FF_CFG_STASSID" pass=%d", name, sizeof(FF_CFG_STAPASS) - 1);
    uint8_t mac[6];
    sdk_wifi_get_macaddr(STATION_IF, mac);
    DEBUG("mon: wifi: ip="IPSTR" mask="IPSTR" gw="IPSTR" mac="MACSTR,
        IP2STR(&ipinfo.ip), IP2STR(&ipinfo.netmask), IP2STR(&ipinfo.gw), MAC2STR(mac));
    if ( sWifiData.backendReady && (sWifiData.host != NULL) && (sWifiData.path != NULL) )
    {
        DEBUG("mon: wifi: backend=%s%s:%u/%s", sWifiData.https ? "https://" : "http://",
            sWifiData.host, sWifiData.port, sWifiData.path);
    }
}


//#define PHY_MODE PHY_MODE_11N // doesn't work well
#define PHY_MODE PHY_MODE_11G

#define SLEEP_MODE WIFI_SLEEP_MODEM
//#define SLEEP_MODE WIFI_SLEEP_LIGHT
//#define SLEEP_MODE WIFI_SLEEP_NONE

void wifiInit(void)
{
    DEBUG("wifi: init");

    memset(&sWifiData, 0, sizeof(sWifiData));
    getSystemName(sWifiData.staName, sizeof(sWifiData.staName));

    //sdk_wifi_status_led_install(2, PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2);

    //if (sdk_wifi_station_dhcpc_status() == DHCP_STOPPED)
    //{
    //    if (!sdk_wifi_station_dhcpc_start())
    //    {
    //        ERROR("wifi: sdk_wifi_station_dhcpc_start() fail!");
    //        return false;
    //    }
    //}
    //if (sdk_wifi_get_opmode() != STATION_MODE)
    {
        if (!sdk_wifi_set_opmode(STATION_MODE))
        {
            ERROR("wifi: sdk_wifi_set_opmode() fail!");
            //return false;
        }
        if (!sdk_wifi_set_opmode_current(STATION_MODE))
        {
            ERROR("wifi: sdk_wifi_set_opmode_current() fail!");
            //return false;
        }
    }

#ifdef PHY_MODE
    //if (sdk_wifi_get_opmode() != PHY_MODE)
    {
        if (!sdk_wifi_set_phy_mode(PHY_MODE))
        {
            ERROR("wifi: sdk_wifi_set_phy_mode("STRINGIFY(PHY_MODE)") fail!");
            //return false;
        }
    }
#endif // PHY_MODE
#ifdef SLEEP_MODE
    //if (sdk_wifi_get_sleep_type() != SLEEP_MODE)
    {
        if (!sdk_wifi_set_sleep_type(SLEEP_MODE))
        {
            ERROR("wifi: sdk_wifi_set_sleep_type("STRINGIFY(SLEEP_MODE)") fail!");
            //return false;
        }
    }
#endif // SLEEP_MODE

    struct sdk_station_config config =
    {
        .ssid = FF_CFG_STASSID, .password = FF_CFG_STAPASS, .bssid_set = false, .bssid = { 0 }
    };
    sdk_wifi_station_set_config(&config);

    sdk_wifi_station_set_auto_connect(true);
}

void wifiStart(void)
{
    DEBUG("wifi: start");
#if (HAVE_CRT)
    static StackType_t sWifiTaskStack[768 + 1024];
#else
    static StackType_t sWifiTaskStack[768];
#endif
    static StaticTask_t sWifiTaskTCB;
    xTaskCreateStatic(sWifiTask, "ff_wifi", NUMOF(sWifiTaskStack), NULL, 4, sWifiTaskStack, &sWifiTaskTCB);
}

/* ********************************************************************************************** */

// eof
