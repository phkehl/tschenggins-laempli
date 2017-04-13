// by Philippe Kehl <flipflip at oinkzwurgl dot org>

#include "user_stuff.h"
#include "user_app.h"
#include "user_wifi.h"
#include "user_ws2801.h"
#include "user_status.h"
#include "user_httpd.h"
#include "user_html.h"
#include "user_wget.h"
#include "user_cfg.h"
#include "jsmn.h"
#include "version_gen.h"
#include "html_gen.h"


/* ***** Jenkins status ************************************************************************** */

// possible job states
typedef enum JENKINS_STATE_e
{
    JENKINS_STATE_OFF      = 0,
    JENKINS_STATE_UNKNOWN  = 1,
    JENKINS_STATE_RUNNING  = 2,
    JENKINS_STATE_IDLE     = 3,
} JENKINS_STATE_t;

static const char skJenkinsStateStrs[][8] PROGMEM =
{
    { "off\0" }, { "unknown\0" }, { "running\0" }, { "idle\0" }
};

// possible job results
typedef enum JENKINS_RESULT_e
{
    JENKINS_RESULT_OFF      = 0,
    JENKINS_RESULT_UNKNOWN  = 1,
    JENKINS_RESULT_SUCCESS  = 2,
    JENKINS_RESULT_UNSTABLE = 3,
    JENKINS_RESULT_FAILURE  = 4,
} JENKINS_RESULT_t;

static const char skJenkinsResultStrs[][12] PROGMEM =
{
    { "off\0" }, { "unknown\0" }, { "success\0" }, { "unstable\0" }, { "failure\0" }
};

#define APP_LED_JOBNAME_LEN 48
#define APP_LED_SERVER_LEN  32

typedef struct LEDS_s
{
    JENKINS_RESULT_t jResult;
    JENKINS_STATE_t  jState;
    char             jobName[APP_LED_JOBNAME_LEN];
    char             serverName[APP_LED_SERVER_LEN];
    int32_t          time;
} LEDS_t;

// storage for jenkins state/result
static LEDS_t sLeds[APP_NUM_LEDS];

JENKINS_RESULT_t sWorstResult = JENKINS_RESULT_OFF;

static void ICACHE_FLASH_ATTR sSetLedsUnknown(void)
{
    for (uint16_t ledIx = 0; ledIx < NUMOF(sLeds); ledIx++)
    {
        sLeds[ledIx].jState  = JENKINS_STATE_UNKNOWN;
        sLeds[ledIx].jResult = JENKINS_RESULT_UNKNOWN;
        strcat_P(sLeds[ledIx].jobName, PSTR("unknown"));
        strcat_P(sLeds[ledIx].serverName, PSTR("unknown"));
        sLeds[ledIx].time = 0;
    }
}

// --------------------------------------------------------------------------------------------------

// convert state string to state enum
static JENKINS_STATE_t ICACHE_FLASH_ATTR sStrToState(const char *str)
{
    JENKINS_STATE_t state = JENKINS_STATE_UNKNOWN;
    if (strcmp_PP(str, skJenkinsStateStrs[JENKINS_STATE_IDLE]) == 0)
    {
        state = JENKINS_STATE_IDLE;
    }
    else if (strcmp_PP(str, skJenkinsStateStrs[JENKINS_STATE_RUNNING]) == 0)
    {
        state = JENKINS_STATE_RUNNING;
    }
    else if (strcmp_PP(str, skJenkinsStateStrs[JENKINS_STATE_OFF]) == 0)
    {
        state = JENKINS_STATE_OFF;
    }
    return state;
}

// convert result string to result enum
static JENKINS_RESULT_t ICACHE_FLASH_ATTR sStrToResult(const char *str)
{
    JENKINS_RESULT_t result = JENKINS_RESULT_UNKNOWN;
    if (strcmp_PP(str, skJenkinsResultStrs[JENKINS_RESULT_FAILURE]) == 0)
    {
        result = JENKINS_RESULT_FAILURE;
    }
    else if (strcmp_PP(str, skJenkinsResultStrs[JENKINS_RESULT_UNSTABLE]) == 0)
    {
        result = JENKINS_RESULT_UNSTABLE;
    }
    else if (strcmp_PP(str, skJenkinsResultStrs[JENKINS_RESULT_SUCCESS]) == 0)
    {
        result = JENKINS_RESULT_SUCCESS;
    }
    else if (strcmp_PP(str, skJenkinsResultStrs[JENKINS_RESULT_OFF]) == 0)
    {
        result = JENKINS_RESULT_OFF;
    }
    return result;
}


/* ***** LED animations ************************************************************************** */

// colours for different results
#define UNKNOWN_HUE     0
#define UNKNOWN_SAT     0
#define UNKNOWN_VAL   150
#define SUCCESS_HUE    85
#define SUCCESS_SAT   255
#define SUCCESS_VAL   200
#define UNSTABLE_HUE   38
#define UNSTABLE_SAT  255
#define UNSTABLE_VAL  200
#define FAILURE_HUE     0
#define FAILURE_SAT   255
#define FAILURE_VAL   200

// minimal brightness, also brightness for unknown state
#define MIN_VAL        30

#define NUM_STEPS     100

// calculate LED brightness given the state
static uint8_t ICACHE_FLASH_ATTR sStateVal(const JENKINS_STATE_t jState, const uint8_t ampl,
    const uint8_t minVal, const uint8_t maxVal)
{
    uint8_t val = 0;
    switch (jState)
    {
        case JENKINS_STATE_OFF:
            break;
        case JENKINS_STATE_UNKNOWN:
            val = minVal;
            break;
        case JENKINS_STATE_IDLE:
            val = maxVal;
            break;
        case JENKINS_STATE_RUNNING:
            val = minVal +  (int)( (float)(maxVal - minVal) * ((float)ampl / (float)NUM_STEPS) );
            //val = minVal + ampl;
            break;
    }
    return val;
}

#define LED_TIMER_INTERVAL 10

#if (USER_WS2801_NUMLEDS < APP_NUM_LEDS)
#  error USER_WS2801_NUMLEDS < APP_NUM_LEDS
#endif

// also using the regular LED timer to measure time
//static uint32_t sAppTick;

// update LEDs according to the led states and results
static void ICACHE_FLASH_ATTR sLedTimerFunc(void *arg)
{
    UNUSED(arg);
    //sAppTick++;

    static const uint8_t skLedIxMap[APP_NUM_LEDS] = APP_LED_MAP;

    static int16_t phase;

    phase++;
    phase %= (2 * NUM_STEPS);

    for (uint16_t ledIx = 0; ledIx < NUMOF(sLeds); ledIx++)
    {
        const int8_t ph = phase - NUM_STEPS;
        const uint8_t ampl = ABS(ph);
        switch (sLeds[ledIx].jResult)
        {
            case JENKINS_RESULT_OFF:
                ws2801SetHSV(skLedIxMap[ledIx], 0, 0, 0);
                break;
            case JENKINS_RESULT_UNKNOWN:
                ws2801SetHSV(skLedIxMap[ledIx], UNKNOWN_HUE, UNKNOWN_SAT,
                    sStateVal(sLeds[ledIx].jState, ampl, MIN_VAL, UNKNOWN_VAL));
                break;
            case JENKINS_RESULT_SUCCESS:
                ws2801SetHSV(skLedIxMap[ledIx], SUCCESS_HUE, SUCCESS_SAT,
                    sStateVal(sLeds[ledIx].jState, ampl, MIN_VAL, SUCCESS_VAL));
                break;
            case JENKINS_RESULT_UNSTABLE:
                ws2801SetHSV(skLedIxMap[ledIx], UNSTABLE_HUE, UNSTABLE_SAT,
                    sStateVal(sLeds[ledIx].jState, ampl, MIN_VAL, UNSTABLE_VAL));
                break;
            case JENKINS_RESULT_FAILURE:
                ws2801SetHSV(skLedIxMap[ledIx], FAILURE_HUE, FAILURE_SAT,
                    sStateVal(sLeds[ledIx].jState, ampl, MIN_VAL, FAILURE_VAL));
                break;
        }
    }
    ws2801Flush();
}


/* ***** Chewbacca ******************************************************************************* */

static void ICACHE_FLASH_ATTR sChewieTimerFunc(void *pArg)
{
    UNUSED(pArg);
    GPIO_OUT_SET(PIN_D1);
    DEBUG("chewie 2");
}

static void ICACHE_FLASH_ATTR sAppTriggerChewie(const bool happy)
{
    DEBUG("sAppTriggerChewie() %s", happy ? PSTR("Indy!") : PSTR("Roar!"));

    if (happy)
    {
        toneBuiltinMelody("IndianaShort");
    }
    else
    {
        const USER_CFG_t *pkUserCfg = cfgGetPtr();
        if (pkUserCfg->haveChewie)
        {
            DEBUG("chewie 1");
            // pull trigger input on sound module low for a bit
            GPIO_OUT_CLR(PIN_D1);
            static os_timer_t sChewieTimer;
            os_timer_disarm(&sChewieTimer);
            os_timer_setfn(&sChewieTimer, (os_timer_func_t *)sChewieTimerFunc, NULL);
            //os_timer_arm(&sChewieTimer, 250, 0); // 250ms, once
            os_timer_arm(&sChewieTimer, 500, 0); // 250ms, once
        }
        else
        {
            toneBuiltinMelody("ImperialShort");
        }
    }
}


/* ***** status update stuff (real-time) ********************************************************* */

typedef enum UPDATE_STATE_e
{
    UPDATE_CHECK, UPDATE_LOOKUP, UPDATE_CONNECT, UPDATE_ONLINE, UPDATE_ABORT, UPDATE_FAIL
} UPDATE_STATE_t;

static const char skUpdateStateStrs[][8] PROGMEM =
{
    { "CHECK\0" }, { "LOOKUP\0" }, { "CONNECT\0" }, { "ONLINE\0" }, { "ABORT\0" }, { "FAIL\0" }
};

static os_timer_t sUpdateTimer;
static UPDATE_STATE_t sUpdateState;
static UPDATE_STATE_t sUpdateStateNext;
#define TRIGGER_UPDATE(state, dt) do { sUpdateStateNext = state; \
        os_timer_disarm(&sUpdateTimer); \
        os_timer_arm(&sUpdateTimer, (dt), 0); } while (0)

static uint32_t sAppPosixTime;
static uint32_t sAppLocalTime;

#define UPDATE_CHECK_INTERVAL 250

//! maximum length for each string (server and job name)
#if (APP_LED_SERVER_LEN > APP_LED_JOBNAME_LEN)
#  define APP_STATUS_REQ_STRLEN APP_LED_SERVER_LEN
#else
#  define APP_STATUS_REQ_STRLEN APP_LED_JOBNAME_LEN
#endif

#define APP_STATUS_RETRY_INTERVAL    10000
#define APP_STATUS_HELLO_TIMEOUT     10000
#define APP_STATUS_HEARTBEAT_TIMEOUT 10000

static bool sForceUpdate;

bool ICACHE_FLASH_ATTR appForceUpdate(void)
{
    switch (sUpdateState)
    {
        case UPDATE_CHECK:
        case UPDATE_ONLINE:
            sForceUpdate = true;
            TRIGGER_UPDATE(UPDATE_ABORT, 10);
            break;

        case UPDATE_LOOKUP:
        case UPDATE_CONNECT:
        case UPDATE_ABORT:
        case UPDATE_FAIL:
            WARNING("app: ignoring manual update (state=%s)", skUpdateStateStrs[sUpdateState]);
            break;
    }
    return sForceUpdate;
}

typedef struct STATUS_HELP_s
{
    struct espconn  conn; // must be first!
    struct _esp_tcp tcp;
    ip_addr_t       hostip;
    const char     *pkHost;
    const char     *pkPath;
    const char     *pkQuery;
    const char     *pkAuth;
    int             urlLen;
    uint16_t        port;
    bool            https;
    __PAD(1);
} STATUS_HELP_t;


static void sAppHostlookupCb(const char *name, ip_addr_t *pAddr, void *arg);
static void sAppConnectCb(void *arg);
static void sAppDisconnectCb(void *arg);
static void sAppReconnectCb(void *arg, sint8 err);
static void sAppTcpSentCb(void *arg);
static void sAppTcpRecvCb(void *arg, char *data, uint16_t size);
static void sAppJsonResponseToState(char *resp, const int respLen);


static void ICACHE_FLASH_ATTR sUpdateTimerFunc(void *pArg)
{
    UNUSED(pArg);
    const UPDATE_STATE_t state = *((UPDATE_STATE_t *)pArg);

    static USER_CFG_t sUserCfg;
    static STATUS_HELP_t sStatusHelper;
    static char sStatusUrl[
        sizeof(sUserCfg.statusUrl) +
        sizeof(sUserCfg.staName) +
        64 + 8 +
        (NUMOF(sUserCfg.leds) * 15) +
        64 +
        64];

    switch (state)
    {
        // 1. check if an update is due and possible
        case UPDATE_CHECK:
        {
            // update config
            cfgGet(&sUserCfg);

            bool doUpdate = true;
            if (!wifiIsOnline())
            {
                //DEBUG("sUpdateTimerFunc() offline");
                statusSet(USER_STATUS_OFFLINE);
                doUpdate = false;
            }
            else if (!sUserCfg.statusUrl[0])
            {
                //DEBUG("sUpdateTimerFunc() no config");
                statusSet(USER_STATUS_OFFLINE);
                doUpdate = false;
            }

            if (doUpdate)
            {
                statusSet(USER_STATUS_UPDATE);
                TRIGGER_UPDATE(UPDATE_LOOKUP, 1000);
            }
            else
            {
                TRIGGER_UPDATE(UPDATE_CHECK, UPDATE_CHECK_INTERVAL);
            }

            break;
        }

        // 2. hostlookup webserver
        case UPDATE_LOOKUP:
        {
            STATUS_HELP_t *pSH = &sStatusHelper;
            struct espconn *pConn = &pSH->conn;

            // reset status helper (states, connection)
            espconn_disconnect(pConn);
            os_memset(&sStatusHelper, 0, sizeof(sStatusHelper));

            // make update URL
            struct ip_info ipinfo;
            wifi_get_ip_info(STATION_IF, &ipinfo);
            sprintf_PP(sStatusUrl,
                PSTR("%s?cmd=realtime;ascii=1;client=%s;name=%s;staip="IPSTR";version="FF_BUILDVER";strlen=" STRINGIFY(APP_STATUS_REQ_STRLEN)),
                sUserCfg.statusUrl, getSystemId(), sUserCfg.staName,
                IP2STR(&ipinfo.ip));
            for (int ledIx = 0; ledIx < (int)NUMOF(sUserCfg.leds); ledIx++)
            {
                char tmp[16];
                sprintf_PP(tmp, PSTR(";ch=%08x"), sUserCfg.leds[ledIx]);
                os_strcat(sStatusUrl, tmp);
            }
            DEBUG("sUpdateTimerFunc() connect %s", sStatusUrl);

            // check update URL
            const int urlLen = wgetReqParamsFromUrl(sStatusUrl, sStatusUrl, sizeof(sStatusUrl),
                &pSH->pkHost, &pSH->pkPath, &pSH->pkQuery, &pSH->pkAuth, &pSH->https, &pSH->port);
            DEBUG("sUpdateTimerFunc() connect %d / %s / %s ? %s / %s / %d / %u",
                urlLen, pSH->pkHost, pSH->pkPath, pSH->pkQuery, pSH->pkAuth, pSH->https, pSH->port);
            if (urlLen <= 0)
            {
                ERROR("app: fishy status url configured");
                TRIGGER_UPDATE(UPDATE_CHECK, APP_STATUS_RETRY_INTERVAL);
            }
            pSH->urlLen = urlLen;

            // connection for hostlookup and status http request
            pSH->conn.type      = ESPCONN_TCP;
            pSH->conn.state     = ESPCONN_NONE;
            pSH->conn.proto.tcp = &pSH->tcp;

            // initiate hostlookup
            if (urlLen > 0)
            {
                PRINT("app: lookup %s", pSH->pkHost);
                const int8_t res = espconn_gethostbyname(
                    &pSH->conn, pSH->pkHost, &pSH->hostip, sAppHostlookupCb);
                // need to call the callback ourselves it seems, compare comment in sWgetHostlookupCb()
                if (res == ESPCONN_OK)
                {
                    sAppHostlookupCb(pSH->pkHost, &pSH->hostip, &pSH->conn);
                }
                // give up
                else if (res != ESPCONN_INPROGRESS)
                {
                    WARNING("app: hostlookup failed: %s", espconnErrStr(res));
                    TRIGGER_UPDATE(UPDATE_FAIL, 20);
                }
            }
            break;
        }

        // 3. hostlookup webserver
        case UPDATE_CONNECT:
        {
            STATUS_HELP_t *pSH = &sStatusHelper;
            struct espconn *pConn = &pSH->conn;

            PRINT("app: connect %s://"IPSTR":%u",
                pSH->https ? PSTR("https") : PSTR("http"), IP2STR(&pSH->hostip), pSH->port);

            // prepare connection handle
            espconn_disconnect(pConn);
            os_memset(&pSH->conn, 0, sizeof(pSH->conn));
            os_memset(&pSH->tcp, 0, sizeof(pSH->tcp));
            pConn->type      = ESPCONN_TCP;
            pConn->state     = ESPCONN_NONE;
            pConn->proto.tcp = &pSH->tcp;
            pConn->proto.tcp->local_port = espconn_port();
            pConn->proto.tcp->remote_port = pSH->port;
            os_memcpy(pConn->proto.tcp->remote_ip, &pSH->hostip, sizeof(pConn->proto.tcp->remote_ip));

            // register connection callbacks
            espconn_regist_connectcb(pConn, sAppConnectCb);
            espconn_regist_reconcb(pConn, sAppReconnectCb);
            espconn_regist_disconcb(pConn, sAppDisconnectCb);
            espconn_regist_recvcb(pConn, sAppTcpRecvCb);
            espconn_regist_sentcb(pConn, sAppTcpSentCb);

            // establish connection
            const int8_t res = pSH->https ?
                espconn_secure_connect(pConn) :
                espconn_connect(pConn);
            if (res != ESPCONN_OK)
            {
                WARNING("app: connect fail: %s", espconnErrStr(res));
                TRIGGER_UPDATE(UPDATE_FAIL, 20);
            }

            // fail if we don't receive the "hello" in reasonable time
            TRIGGER_UPDATE(UPDATE_FAIL, APP_STATUS_HELLO_TIMEOUT);

            // next step: sAppConnectCb() or sAppReconnectCb() or sAppDisconnectCb()
            break;
        }

        // 4. online, wait for connection to break or forced refresh
        case UPDATE_ONLINE:

            statusSet(USER_STATUS_HEARTBEAT);
            if (sUpdateState != UPDATE_ONLINE)
            {
                PRINT("app: status online");
            }

            // fail if we don't receive another "heartbeat" in reasonable time
            TRIGGER_UPDATE(UPDATE_FAIL, APP_STATUS_HEARTBEAT_TIMEOUT);

            break;

        // something failed, retry later
        case UPDATE_FAIL:
        {
            STATUS_HELP_t *pSH = &sStatusHelper;
            struct espconn *pConn = &pSH->conn;

            statusSet(USER_STATUS_FAIL);
            sSetLedsUnknown();

            espconn_disconnect(pConn);

            ERROR("app: status fail");
            TRIGGER_UPDATE(UPDATE_CHECK, APP_STATUS_RETRY_INTERVAL);

            break;
        }

        // aborted, re-connect
        case UPDATE_ABORT:
        {
            STATUS_HELP_t *pSH = &sStatusHelper;
            struct espconn *pConn = &pSH->conn;

            statusSet(USER_STATUS_FAIL);
            sSetLedsUnknown();

            espconn_disconnect(pConn);

            if (sForceUpdate)
            {
                PRINT("app: force update");
                TRIGGER_UPDATE(UPDATE_CHECK, 250);
            }
            else if (sUpdateState == UPDATE_FAIL)
            {
                // the espconn_disconnect() -> sAppDisconnectCb() triggered us
            }
            else
            {
                WARNING("app: status abort");
                TRIGGER_UPDATE(UPDATE_CHECK, 250);
            }
            break;
        }

    } // switch (state)

    sUpdateState = state;
}

static void ICACHE_FLASH_ATTR sAppHostlookupCb(const char *name, ip_addr_t *pAddr, void *arg)
{
    struct espconn *pConn = (struct espconn *)arg;
    STATUS_HELP_t *pSH = (STATUS_HELP_t *)pConn;

    // give up, hostname lookup failed
    if (pAddr == NULL)
    {
        WARNING("app: hostname lookup for %s failed", pSH->pkHost);
        TRIGGER_UPDATE(UPDATE_FAIL, 20);
        return;
    }

    // pAddr is NOT what third argument we passed to espconn_gethostbyname()
    // http://bbs.espressif.com/viewtopic.php?t=2095: «The third parameter can be used to store the
    // IP address which got by DNS, so that if users call espconn_gethostbyname again, it won't run
    // DNS again, but just use the IP address it already got.»

    pSH->hostip.addr = pAddr->addr;
    DEBUG("sAppHostlookupCb() %s -> "IPSTR, pSH->pkHost, IP2STR(&pSH->hostip));

    // next step: connect
    TRIGGER_UPDATE(UPDATE_CONNECT, 20);
}

static void ICACHE_FLASH_ATTR sAppConnectCb(void *arg)
{
    struct espconn *pConn = (struct espconn *)arg;
    STATUS_HELP_t *pSH = (STATUS_HELP_t *)pConn;

    DEBUG("sAppConnectCb(%p)", pConn);

    PRINT("app: request /%s", pSH->pkPath);
#if 0
    // make HTTP GET request
    char req[pSH->urlLen + 128];
    sprintf_PP(req,
        PSTR("GET /%s?%s HTTP/1.1\r\n"     // HTTP GET request
            "Host: %s\r\n"              // provide host name for virtual host setups
            "Authorization: Basic %s\r\n" // okay to provide empty one?
            "User-Agent: %s/%s\r\n"     // be nice
            "\r\n"),                    // end of request headers
        pSH->pkPath, pSH->pkQuery, pSH->pkHost,
        pSH->pkAuth != NULL ? pSH->pkAuth : PSTR(""),
        PSTR(FF_PROJECT), PSTR(FF_BUILDVER));
#else
    // make HTTP GET request
    char req[pSH->urlLen + 128];
    sprintf_PP(req,
        PSTR("POST /%s HTTP/1.1\r\n"      // HTTP POST request
            "Host: %s\r\n"                // provide host name for virtual host setups
            "Authorization: Basic %s\r\n" // okay to provide empty one?
            "User-Agent: %s/%s\r\n"       // be nice
            "Content-Length: %d\r\n"      // length of query parameters
            "\r\n"                        // end of request headers
            "%s"),                        // query parameters (FIXME: urlencode!)
        pSH->pkPath,
        pSH->pkHost,
        pSH->pkAuth != NULL ? pSH->pkAuth : PSTR(""),
        PSTR(FF_PROJECT), PSTR(FF_BUILDVER),
        os_strlen(pSH->pkQuery),
        pSH->pkQuery);
#endif

    // send request
    const uint16_t reqLen = os_strlen(req);
    const int8_t res = pSH->https ?
        espconn_secure_send(pConn, (uint8_t *)req, reqLen) :
        espconn_send(pConn, (uint8_t *)req, reqLen);

    if (res != ESPCONN_OK)
    {
        WARNING("app: request fail: %s", espconnErrStr(res));
        TRIGGER_UPDATE(UPDATE_FAIL, 1000);
    }

    // next step: sAppTcpSentCb()
}

static void ICACHE_FLASH_ATTR sAppReconnectCb(void *arg, sint8 err)
{
    struct espconn *pConn = (struct espconn *)arg;
    STATUS_HELP_t *pSH = (STATUS_HELP_t *)pConn;

    WARNING("app: connect to "IPSTR":%u error %d", IP2STR(&pSH->hostip), pSH->port, err);

    TRIGGER_UPDATE(UPDATE_FAIL, 1000);
}

static void ICACHE_FLASH_ATTR sAppTcpSentCb(void *arg)
{
    struct espconn *pConn = (struct espconn *)arg;
    STATUS_HELP_t *pSH = (STATUS_HELP_t *)pConn;
    UNUSED(pSH);

    DEBUG("sAppTcpSentCb(%p)", pConn);

    // next step: one or more sAppTcpRecvCb()
}

static void ICACHE_FLASH_ATTR sAppSetTime(const char *tsStr)
{
    sAppPosixTime = atoi(tsStr);
    sAppLocalTime = system_get_time() / 1000;
}

static void ICACHE_FLASH_ATTR sAppTcpRecvCb(void *arg, char *data, uint16_t size)
{
    struct espconn *pConn = (struct espconn *)arg;
    STATUS_HELP_t *pSH = (STATUS_HELP_t *)pConn;
    UNUSED(pSH);

    char *pStatus    = strstr_P(data, PSTR("\r\nstatus "));
    char *pHeartbeat = strstr_P(data, PSTR("\r\nheartbeat "));

    // "\r\nheartbeat 1491146601 25\r\n"
    if (pHeartbeat != NULL)
    {
        pHeartbeat += 2;
        char *endOfHeartbeat = strstr_P(pHeartbeat, PSTR("\r\n"));
        if (endOfHeartbeat != NULL)
        {
            *endOfHeartbeat = '\0';
        }
        sAppSetTime(&pHeartbeat[10]);
        DEBUG("sAppTcpRecvCb(%p) %s", pConn, pHeartbeat);

        TRIGGER_UPDATE(UPDATE_ONLINE, 20);
    }
    // "\r\nstatus 1491146576 json={"leds": ... }\r\n"
    else if (pStatus != NULL)
    {
        pStatus += 2;
        char *endOfStatus = strstr_P(pStatus, PSTR("\r\n"));
        if (endOfStatus != NULL)
        {
            *endOfStatus = '\0';
        }
        char *pJson = os_strstr(&pStatus[7], " ");
        if (pJson != NULL)
        {
            *pJson = '\0';
            pJson += 1;
            sAppSetTime(&pStatus[7]);
            const int jsonLen = os_strlen(pJson);
            DEBUG("sAppTcpRecvCb(%p) %s json=%s", pConn, pStatus, &pJson[1]);
            PRINT("app: status update received");
            sAppJsonResponseToState(pJson, jsonLen);
        }
        else
        {
            WARNING("app: ignoring fishy status");
        }
    }
    else
    {
        char *pHello = strstr_P(data, PSTR("\r\nhello "));
        if (pHello != NULL)
        {
            pHello += 2;
            char *endOfHello = strstr_P(pHello, PSTR("\r\n"));
            if (endOfHello != NULL)
            {
                *endOfHello = '\0';
            }
            DEBUG("sAppTcpRecvCb(%p) %s", pConn, pHello);
            TRIGGER_UPDATE(UPDATE_ONLINE, 20);
        }
        else
        {
            char *pRedirect = strstr_P(data, PSTR("\r\nLocation: "));
            if (pRedirect != NULL)
            {
                WARNING("app: cannot handle redirect, check statusUrl configuration");
                TRIGGER_UPDATE(UPDATE_FAIL, 5);
            }
            else
            {
                DEBUG("sAppTcpRecvCb(%p) size=%u data=%s", pConn, size, data);
                WARNING("app: ignoring fishy response");
            }
        }
    }

    // next step: more data in sAppTcpRecvCb(), or sAppDisconnectCb()
}

static void ICACHE_FLASH_ATTR sAppDisconnectCb(void *arg)
{
    struct espconn *pConn = (struct espconn *)arg;
    STATUS_HELP_t *pSH = (STATUS_HELP_t *)pConn;
    UNUSED(pSH);

    DEBUG("sAppDisconnectCb(%p)", pConn);

    TRIGGER_UPDATE(UPDATE_ABORT, 20);
}


/* ***** button ********************************************************************************** */

#define BUTTON_TIMER_INTERVAL 50

static void ICACHE_FLASH_ATTR sButtonTimerFunc(void *pArg)
{
    UNUSED(pArg);
    static uint32_t pressedTime;
    // button press pulls signal low
    if ( !GPIO_IN_READ(PIN_D3) )
    {
        if (pressedTime++ == (500/BUTTON_TIMER_INTERVAL))
        {
            appForceUpdate();
        }
    }
    // reset on button release
    else
    {
        pressedTime = 0;
    }
    //TRIGGER_UPDATE(UPDATE_CHECK, UPDATE_INTERVAL_NORMAL);
}


/* ***** json status to state ******************************************************************** */

#define JSON_STREQ(json, pkTok, str) (    \
        ((pkTok)->type == JSMN_STRING) && \
        (strlen_P(str) == ( (pkTok)->end - (pkTok)->start ) ) && \
        (strncmp_PP(&json[(pkTok)->start], str, (pkTok)->end - (pkTok)->start) == 0) )

#define JSON_ANYEQ(json, pkTok, str) (    \
        ( ((pkTok)->type == JSMN_STRING) || ((pkTok)->type == JSMN_PRIMITIVE) ) && \
        (strlen_P(str) == ( (pkTok)->end - (pkTok)->start ) ) && \
        (strncmp_PP(&json[(pkTok)->start], str, (pkTok)->end - (pkTok)->start) == 0) )

static void ICACHE_FLASH_ATTR sAppJsonResponseToState(char *resp, const int respLen)
{
    // memory for JSON parser
    const int maxTokens = (6 * NUMOF(sLeds)) + 20;
    const int tokensSize = maxTokens * sizeof(jsmntok_t);
    jsmntok_t *pTokens = memAlloc(tokensSize);
    if (pTokens == NULL)
    {
        ERROR("app: json malloc fail");
        //TRIGGER_UPDATE(UPDATE_FAIL, 20);
        return;
    }
    os_memset(pTokens, 0, tokensSize);

    // parse JSON response
    jsmn_parser parser;
    jsmn_init(&parser);
    const int numTokens = jsmn_parse(&parser, resp, respLen, pTokens, maxTokens);
    bool okay = true;
    if (numTokens < 1)
    {
        switch (numTokens)
        {
            case JSMN_ERROR_NOMEM: WARNING("json: no mem");                    break;
            case JSMN_ERROR_INVAL: WARNING("json: invalid");                   break;
            case JSMN_ERROR_PART:  WARNING("json: partial");                   break;
            default:               WARNING("json: too short (%d)", numTokens); break;
        }
        okay = false;
    }
    DEBUG("sAppWgetResponse() %d/%d tokens, alloc %d",
        numTokens, maxTokens, tokensSize);

#if 0
    // debug json tokens
    for (int ix = 0; ix < numTokens; ix++)
    {
        static const char skTypeStrs[][8] PROGMEM =
        {
            { "undef\0" }, { "obj\0" }, { "arr\0" }, { "str\0" }, { "prim\0" }
        };
        const jsmntok_t *pkTok = &pTokens[ix];
        char buf[200];
        int sz = pkTok->end - pkTok->start;
        if ( (sz > 0) && (sz < (int)sizeof(buf)))
        {
            os_memcpy(buf, &resp[pkTok->start], sz);
            buf[sz] = '\0';
        }
        else
        {
            buf[0] = '\0';
        }
        char str[10];
        strcpy_P(str, pkTok->type < NUMOF(skTypeStrs) ? skTypeStrs[pkTok->type] : PSTR("???"));
        DEBUG("json %02u: %d %-5s %03d..%03d %d <%2d %s",
            ix, pkTok->type, str,
            pkTok->start, pkTok->end, pkTok->size, pkTok->parent, buf);
    }
#endif


    // process JSON data
    while (okay)
    {
        if (pTokens[0].type != JSMN_OBJECT)
        {
            WARNING("app: json not obj");
            okay = false;
            break;
        }

        // look for response result
        for (int ix = 0; ix < numTokens; ix++)
        {
            const jsmntok_t *pkTok = &pTokens[ix];
            // top-level "res" key
            if ( (pkTok->parent == 0) && JSON_STREQ(resp, pkTok, PSTR("res")) )
            {
                // so the next token must point back to this (key : value pair)
                if (pTokens[ix + 1].parent == ix)
                {
                    // and we want the result 1 (or "1")
                    if (!JSON_ANYEQ(resp, &pTokens[ix+1], "1"))
                    {
                        resp[ pTokens[ix+1].end ] = '\0';
                        WARNING("app: json res=%s", &resp[ pTokens[ix+1].start ]);
                        okay = false;
                    }
                    break;
                }
            }
        }
        if (!okay)
        {
            break;
        }

        // look for "leds" data
        int chArrTokIx = -1;
        for (int ix = 0; ix < numTokens; ix++)
        {
            const jsmntok_t *pkTok = &pTokens[ix];
            // top-level "leds" key
            if ( (pkTok->parent == 0) && JSON_STREQ(resp, pkTok, PSTR("leds")) )
            {
                //DEBUG("leds at %d", ix);
                // so the next token must be an array and point back to this token
                if ( (pTokens[ix + 1].type == JSMN_ARRAY) &&
                     (pTokens[ix + 1].parent == ix) )
                {
                    chArrTokIx = ix + 1;
                }
                else
                {
                    WARNING("app: json no leds");
                    okay = false;
                }
                break;
            }
        }
        if (chArrTokIx < 0)
        {
            okay = false;
        }
        if (!okay)
        {
            break;
        }
        //DEBUG("chArrTokIx=%d", chArrTokIx);

        // check number of array elements
        if (pTokens[chArrTokIx].size != NUMOF(sLeds))
        {
            WARNING("app: json leds %d!=%d", pTokens[chArrTokIx].size, (int)NUMOF(sLeds));
            okay = false;
            break;
        }

        // parse array
        for (int ledIx = 0; ledIx < (int)NUMOF(sLeds); ledIx++)
        {
            const int numFields = 5;
            // expected start of fife element array with state and result
            // ["CI_foo_master","devpos-thl","idle","success",9199]
            const int arrIx = chArrTokIx + 1 + (ledIx * (numFields + 1));
            //DEBUG("ledIx=%d arrIx=%d", ledIx, arrIx);
            if ( (pTokens[arrIx].type != JSMN_ARRAY) || (pTokens[arrIx].size != numFields) )
            {
                WARNING("app: json leds format (arrIx=%d, type=%d, size=%d)",
                    arrIx, pTokens[arrIx].type, pTokens[arrIx].size);
                okay = false;
                break;
            }
            const int nameIx   = arrIx + 1;
            const int serverIx = arrIx + 2;
            const int stateIx  = arrIx + 3;
            const int resultIx = arrIx + 4;
            const int timeIx   = arrIx + 5;
            if ( (pTokens[nameIx].type   != JSMN_STRING) ||
                 (pTokens[serverIx].type != JSMN_STRING) ||
                 (pTokens[stateIx].type  != JSMN_STRING) ||
                 (pTokens[resultIx].type != JSMN_STRING) ||
                 (pTokens[timeIx].type   != JSMN_PRIMITIVE) )
            {
                WARNING("app: json leds format (%d, %d, %d, %d, %d)",
                    pTokens[nameIx].type, pTokens[serverIx].type,
                    pTokens[stateIx].type, pTokens[resultIx].type, pTokens[timeIx].type);
                okay = false;
                break;
            }

            // get data
            resp[ pTokens[nameIx].end ] = '\0';
            const char *nameStr    = &resp[ pTokens[nameIx].start ];

            resp[ pTokens[serverIx].end ] = '\0';
            const char *serverStr  = &resp[ pTokens[serverIx].start ];

            resp[ pTokens[stateIx].end ] = '\0';
            const char *stateStr  = &resp[ pTokens[stateIx].start ];

            resp[ pTokens[resultIx].end ] = '\0';
            const char *resultStr   = &resp[ pTokens[resultIx].start ];

            resp[ pTokens[timeIx].end ] = '\0';
            const char *timeStr     = &resp[ pTokens[timeIx].start ];

            DEBUG("sAppWgetResponse() arrIx=%02d ledIx=%02d name=%s server=%s state=%s result=%s time=%s",
                arrIx, ledIx, nameStr, serverStr, stateStr, resultStr, timeStr);

            sLeds[ledIx].jState  = sStrToState(stateStr);
            sLeds[ledIx].jResult = sStrToResult(resultStr);
            os_strncpy(sLeds[ledIx].jobName,    nameStr,   sizeof(sLeds[ledIx].jobName) - 1);
            os_strncpy(sLeds[ledIx].serverName, serverStr, sizeof(sLeds[ledIx].serverName) - 1);
            sLeds[ledIx].time = atoi(timeStr);
        }

        break;
    }

    // are we happy?
    if (okay)
    {
        DEBUG("sAppWgetResponse() json parse okay");

        JENKINS_RESULT_t worstResult = JENKINS_RESULT_OFF;
        for (int ledIx = 0; ledIx < (int)NUMOF(sLeds); ledIx++)
        {
            if (sLeds[ledIx].jResult > worstResult)
            {
                worstResult = sLeds[ledIx].jResult;
            }
        }

        // Chewbacca?
        if ( (worstResult > sWorstResult) && (worstResult == JENKINS_RESULT_FAILURE) )
        {
            sAppTriggerChewie(false);
        }
        // Indiana?
        else if ( (worstResult < sWorstResult) && (worstResult == JENKINS_RESULT_SUCCESS) )
        {
            sAppTriggerChewie(true);
        }
        PRINT("app: %s --> %s",
            skJenkinsResultStrs[sWorstResult], skJenkinsResultStrs[worstResult]);
        sWorstResult = worstResult;
    }
    else
    {
        ERROR("app: json parse fail");
    }

    // cleanup
    memFree(pTokens);

    //if (!okay)
    //{
    //    TRIGGER_UPDATE(UPDATE_FAIL, 20);
    //}
    //else
    //{
    //    TRIGGER_UPDATE(UPDATE_DONE, 20);
    //}
}


/* ***** web interface *************************************************************************** */

static const char skAppStatusHtml[] PROGMEM = USER_APP_STATUS_HTML_STR;

#define APP_STATUS_HTML_SIZE (6 * 1024)

#define APP_STATUS_TR_FMT "<tr><td><div class=\"led led-%s led-%s-ani\" title=\"%s %s\"></div></td>" \
        "<td><span class=\"name\">%s</span></br><span class=\"server\">%s</span></td></tr>"

#define APP_STATUS_TR_SIZE (NUMOF(sLeds) * 256)
#define APP_STATUS_RESP_SIZE (sizeof(skAppStatusHtml) + sizeof(sLeds) + APP_STATUS_TR_SIZE)

static bool ICACHE_FLASH_ATTR sAppStatusRequestCb(struct espconn *pConn, const HTTPD_REQCB_INFO_t *pkInfo)
{
    // handle status refresh request
    if (pkInfo->numKV > 0)
    {
        bool redirect = false;
        if (strcmp_PP(pkInfo->keys[0], PSTR("refresh")) == 0)
        {
            appForceUpdate();
            redirect = true;
        }

        if (redirect)
        {
            char location[256];
            sprintf_PP(location, PSTR("Location: http://%s%s\r\n"), pkInfo->host, pkInfo->path);
            return httpdSendError(pConn, pkInfo->path, 302, location, NULL);
        }
    }

    char *pResp = memAlloc(APP_STATUS_RESP_SIZE);
    if (pResp == NULL)
    {
        ERROR("sAppStatusRequestCb(%p) malloc %u fail", pConn, APP_STATUS_RESP_SIZE);
        return false;
    }
    char *pTr = memAlloc(APP_STATUS_TR_SIZE);
    if (pTr == NULL)
    {
        ERROR("sAppStatusRequestCb(%p) malloc %u fail", pConn, APP_STATUS_TR_SIZE);
        memFree(pResp);
        return false;
    }

    strcpy_P(pResp, skAppStatusHtml);

    pTr[0] = '\0';
    int trLen = 0;
    for (int ledIx = 0; ledIx < (int)NUMOF(sLeds); ledIx++)
    {
        const char *colour;
        switch (sLeds[ledIx].jResult)
        {
            case JENKINS_RESULT_SUCCESS:  colour = PSTR("green");   break;
            case JENKINS_RESULT_UNSTABLE: colour = PSTR("yellow");  break;
            case JENKINS_RESULT_FAILURE:  colour = PSTR("red");     break;
            case JENKINS_RESULT_UNKNOWN:  colour = PSTR("unknown"); break;
            default:
            case JENKINS_RESULT_OFF:      colour = PSTR("off");     break;
        }
        const char *stateStr  = skJenkinsStateStrs[ sLeds[ledIx].jState ];
        const char *resultStr = skJenkinsResultStrs[ sLeds[ledIx].jResult ];
        const int reqLen = (int)sizeof(APP_STATUS_TR_FMT) + 8 + (2 * strlen_P(colour)) +
            strlen_P(stateStr) + strlen_P(resultStr);
        const int remSize = APP_STATUS_TR_SIZE - 1 - trLen;
        if (reqLen < remSize)
        {
            sprintf_PP(&pTr[trLen], PSTR(APP_STATUS_TR_FMT),
                colour, sLeds[ledIx].jState == JENKINS_STATE_RUNNING ? colour : PSTR("nope"),
                skJenkinsStateStrs[ sLeds[ledIx].jState ],
                skJenkinsResultStrs[ sLeds[ledIx].jResult ],
                sLeds[ledIx].jobName, sLeds[ledIx].serverName);
            trLen = os_strlen(pTr);
        }
    }

    const char *state = skUpdateStateStrs[sUpdateState];
    char localtime[20];
    sprintf_PP(localtime, PSTR("%u"), (sAppLocalTime + 500) / 1000);
    char age[20];
    sprintf_PP(age, PSTR("%u"), ((system_get_time() / 1000) - sAppLocalTime) / 1000);

    const char *templKeys[] = { PSTR("STATUSTABLE"), PSTR("STATE"), PSTR("LOCALTIME"), PSTR("AGE") };
    const char *templVals[] = {       pTr,                 state,         localtime,         age   };

    const int htmlLen = htmlRender(
        pResp, pResp, APP_STATUS_RESP_SIZE, templKeys, templVals, (int)NUMOF(templKeys), true);

    memFree(pTr);

    DEBUG("sAppStatusRequestCb(%p) use %d/%d %d/%d", pConn,
            trLen, APP_STATUS_TR_SIZE, htmlLen, APP_STATUS_RESP_SIZE);

    const bool res = httpSendHtmlPage(pConn, pResp, false);

    memFree(pResp);

    return res;
}


// --------------------------------------------------------------------------------------------------

static const char skAppSoundHtml[] PROGMEM = USER_APP_SOUND_HTML_STR;

static bool ICACHE_FLASH_ATTR sAppSoundRequestCb(struct espconn *pConn, const HTTPD_REQCB_INFO_t *pkInfo)
{
    if (pkInfo->numKV > 0)
    {
        bool redirect = false;
        if (strcmp_PP(pkInfo->keys[0], PSTR("stop")) == 0)
        {
            toneStop();
            redirect = true;
        }
        else if (strcmp_PP(pkInfo->keys[0], PSTR("builtin")) == 0)
        {
            toneBuiltinMelody(pkInfo->vals[0]);
            redirect = true;
        }
        else if (strcmp_PP(pkInfo->keys[0], PSTR("rtttl")) == 0)
        {
            toneRtttlMelody(pkInfo->vals[0]);
            redirect = true;
        }
        else if (strcmp_PP(pkInfo->keys[0], PSTR("chewie")) == 0)
        {
            toneStop();
            switch (pkInfo->vals[0][0])
            {
                case 'r': sAppTriggerChewie(false); break;
                case 'i': sAppTriggerChewie(true);  break;
            }
            redirect = true;
        }

        if (redirect)
        {
            char location[256];
            sprintf_PP(location, PSTR("Location: http://%s%s\r\n"), pkInfo->host, pkInfo->path);
            return httpdSendError(pConn, pkInfo->path, 302, location, NULL);
        }
    }

    // send response
    return httpSendHtmlPage(pConn, skAppSoundHtml, false);
}


/* ***** initialisation, start, monitor ********************************************************** */

void ICACHE_FLASH_ATTR appInit(void)
{
    DEBUG("app: init");


    // button
    GPIO_ENA_PIN_D3();
    GPIO_DIR_CLR(PIN_D3); // input
    PIN_PULLUP_EN(PERIPHS_IO_MUX_GPIO0_U);

    // Chewie trigger
    GPIO_ENA_PIN_D1();
    GPIO_DIR_SET(PIN_D1);
    GPIO_OUT_SET(PIN_D1);

    sSetLedsUnknown();

    httpdRegisterRequestCb(PSTR("/status"), HTTPD_AUTH_USER, sAppStatusRequestCb);
    httpdRegisterRequestCb(PSTR("/sound"),  HTTPD_AUTH_USER, sAppSoundRequestCb);
}


// --------------------------------------------------------------------------------------------------


void ICACHE_FLASH_ATTR appStart(void)
{
    PRINT("app: start");

    // LED timer
    static os_timer_t sLedTimer;
    os_timer_disarm(&sLedTimer);
    os_timer_setfn(&sLedTimer, (os_timer_func_t *)sLedTimerFunc, NULL);
    os_timer_arm(&sLedTimer, LED_TIMER_INTERVAL, 1); // interval, repeated

    // setup update timer
    os_timer_disarm(&sUpdateTimer);
    os_timer_setfn(&sUpdateTimer, (os_timer_func_t *)sUpdateTimerFunc, &sUpdateStateNext);
    // fire update timer
    TRIGGER_UPDATE(UPDATE_CHECK, UPDATE_CHECK_INTERVAL);

    // button timer
    static os_timer_t sButtonTimer;
    os_timer_disarm(&sButtonTimer);
    os_timer_setfn(&sButtonTimer, (os_timer_func_t *)sButtonTimerFunc, NULL);
    os_timer_arm(&sButtonTimer, BUTTON_TIMER_INTERVAL, 1); // interval, repeated
}


// --------------------------------------------------------------------------------------------------

void ICACHE_FLASH_ATTR appStatus(void)
{
    //const uint32_t msss = system_get_time() / 1000;
    DEBUG("mon: app: state=%s"/* last=%d next=%d*/" localtime=%u worst=%s",
        skUpdateStateStrs[sUpdateState],
        //(int32_t)(msss - sLastUpdate), (int32_t)(sNextUpdate - msss),
        (sAppLocalTime + 500) / 1000,
        skJenkinsResultStrs[sWorstResult]);
#if 0
    char chStr[1024];
    chStr[0] = '\0';
    for (uint16_t ledIx = 0; ledIx < NUMOF(sLeds); ledIx++)
    {
        sprintf_PP(&chStr[os_strlen(chStr)], PSTR(" ch%02d=%s/%s"),
            ledIx,
            skJenkinsStateStrs[ sLeds[ledIx].jState ],
            skJenkinsResultStrs[ sLeds[ledIx].jResult ]);
        if ( (((ledIx+1) % 4) == 0) || (ledIx == (NUMOF(sLeds)-1)) )
        {
            DEBUG("mon: app:%s", chStr);
            chStr[0] = '\0';
        }
    }
#endif
}


/* *********************************************************************************************** */

// eof
