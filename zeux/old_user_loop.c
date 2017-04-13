// by Philippe Kehl <flipflip at oinkzwurgl dot org>

#include "user_stuff.h"
#include "user_loop.h"
#include "user_wifi.h"
#include "user_ws2801.h"
#include "user_status.h"
#include "user_httpd.h"
#include "user_html.h"
#include "user_cfg.h"
#include "jsmn.h"
#include "version_gen.h"
#include "html_gen.h"


// --------------------------------------------------------------------------------------------------

#define STATUS_USE_JSON 1
#define STATUS_HOST "things.oinkzwurgl.org"
#if (STATUS_USE_JSON > 0)
#  define STATUS_PATH "/tschenggins-laempli/?cmd=channels"
#else
#  define STATUS_PATH "/tschenggins-laempli/?cmd=channelstxt"
#endif
#define UPDATE_INTERVAL_RETRY  10000
#define UPDATE_INTERVAL_NORMAL 60000
#define UPDATE_EXPIRE          (5 * UPDATE_INTERVAL_NORMAL)


/* ***** LED animations ************************************************************************** */

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

#if (STATUS_USE_JSON == 0)
static JENKINS_STATE_t ICACHE_FLASH_ATTR sStrNumToState(const char *str)
{
    const int st = atoi(str);
    switch (st)
    {
        case JENKINS_STATE_UNKNOWN: return JENKINS_STATE_UNKNOWN;
        case JENKINS_STATE_RUNNING: return JENKINS_STATE_RUNNING;
        case JENKINS_STATE_IDLE:    return JENKINS_STATE_IDLE;
        default:
        case JENKINS_STATE_OFF:     return JENKINS_STATE_OFF;
    }
}
static JENKINS_RESULT_t ICACHE_FLASH_ATTR sStrNumToResult(const char *str)
{
    const int st = atoi(str);
    switch (st)
    {
        case JENKINS_RESULT_UNKNOWN:  return JENKINS_RESULT_UNKNOWN;
        case JENKINS_RESULT_SUCCESS:  return JENKINS_RESULT_SUCCESS;
        case JENKINS_RESULT_UNSTABLE: return JENKINS_RESULT_UNSTABLE;
        case JENKINS_RESULT_FAILURE:  return JENKINS_RESULT_FAILURE;
        default:
        case JENKINS_RESULT_OFF:      return JENKINS_RESULT_OFF;
    }
}
#else
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
#endif

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

#define MIN_VAL        30

#define NUM_STEPS     100

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


// --------------------------------------------------------------------------------------------------


#define USER_LOOP_NUMLEDS 8
#if USER_WS2801_NUMLEDS < USER_LOOP_NUMLEDS
#  error USER_WS2801_NUMLEDS < USER_LOOP_NUMLEDS
#endif

typedef struct CHANNELS_s
{
    JENKINS_RESULT_t jResult;
    JENKINS_STATE_t  jState;
} CHANNELS_t;

static CHANNELS_t sChannels[USER_LOOP_NUMLEDS];

JENKINS_RESULT_t sWorstResult = JENKINS_RESULT_OFF;

static uint32_t sChannelsUpdateTick;

static void ICACHE_FLASH_ATTR sSetChannelsUnknown(void)
{
    for (uint16_t chIx = 0; chIx < NUMOF(sChannels); chIx++)
    {
        sChannels[chIx].jState  = JENKINS_STATE_UNKNOWN;
        sChannels[chIx].jResult = JENKINS_RESULT_UNKNOWN;
    }
}

static os_timer_t sChewieTimer;
static void ICACHE_FLASH_ATTR sChewieTimerFunc(void *pArg)
{
    UNUSED(pArg);
    GPIO_OUT_SET(PIN_D1);
}
static void ICACHE_FLASH_ATTR sLoopTriggerChewie(void)
{
    DEBUG("sLoopTriggerChewie()");
    // pull trigger input on sound module low for a bit
    GPIO_OUT_CLR(PIN_D1);
    os_timer_arm(&sChewieTimer, 250, 0); // 250ms, once
}


// --------------------------------------------------------------------------------------------------

#define JSON_STREQ(json, pkTok, str) (    \
        ((pkTok)->type == JSMN_STRING) && \
        (strlen_P(str) == ( (pkTok)->end - (pkTok)->start ) ) && \
        (strncmp_PP(&json[(pkTok)->start], str, (pkTok)->end - (pkTok)->start) == 0) )

#define JSON_ANYEQ(json, pkTok, str) (    \
        ( ((pkTok)->type == JSMN_STRING) || ((pkTok)->type == JSMN_PRIMITIVE) ) && \
        (strlen_P(str) == ( (pkTok)->end - (pkTok)->start ) ) && \
        (strncmp_PP(&json[(pkTok)->start], str, (pkTok)->end - (pkTok)->start) == 0) )

static bool ICACHE_FLASH_ATTR sSetChannelsFromResp(/* in, out */ char *resp)
{
    bool done = false;

#if (STATUS_USE_JSON > 0)

    jsmn_parser parser;
    jsmn_init(&parser);

    const int resplen = os_strlen(resp);
    jsmntok_t tokens[ (3 * NUMOF(sChannels)) + 10 ];
    os_memset(tokens, 0, sizeof(tokens));
    const int res = jsmn_parse(&parser, resp, resplen, tokens, NUMOF(tokens));
    bool okay = true;
    if (res < 1)
    {
        switch (res)
        {
            case JSMN_ERROR_NOMEM: WARNING("json: no mem");              break;
            case JSMN_ERROR_INVAL: WARNING("json: invalid");             break;
            case JSMN_ERROR_PART:  WARNING("json: partial");             break;
            default:               WARNING("json: too short (%d)", res); break;
        }
        okay = false;
    }

#if 0
    // debug json tokens
    for (int ix = 0; ix < 10; ix++)
    {
        static const char skTypeStrs[][8] PROGMEM =
        {
            { "undef\0" }, { "obj\0" }, { "arr\0" }, { "str\0" }, { "prim\0" }
        };
        const jsmntok_t *pkTok = &tokens[ix];
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
        DEBUG("json %02u: %-5s %03d..%03d %d <%2d %s",
            ix, pkTok->type < NUMOF(skTypeStrs) ? skTypeStrs[pkTok->type] : "???",
            pkTok->start, pkTok->end, pkTok->size, pkTok->parent, buf);
    }
#endif

    while (okay)
    {
        DEBUG("json parse: %d/%d tokens, %d/%d chars",
            parser.toknext - 1, (int)NUMOF(tokens), parser.pos, resplen);

        if (tokens[0].type != JSMN_OBJECT)
        {
            WARNING("json: not obj");
            okay = false;
            break;
        }

        // look for response result
        for (int ix = 0; ix < (int)NUMOF(tokens); ix++)
        {
            const jsmntok_t *pkTok = &tokens[ix];
            // top-level "res" key
            if ( (pkTok->parent == 0) && JSON_STREQ(resp, pkTok, PSTR("res")) )
            {
                // so the next token must point back to this (key : value pair)
                if (tokens[ix + 1].parent == ix)
                {
                    // and we want the result 1 (or "1")
                    if (!JSON_ANYEQ(resp, &tokens[ix+1], "1"))
                    {
                        resp[ tokens[ix+1].end ] = '\0';
                        WARNING("json: res=%s", &resp[ tokens[ix+1].start ]);
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

        // look for "channels" data
        int chArrTokIx = -1;
        for (int ix = 0; ix < (int)NUMOF(tokens); ix++)
        {
            const jsmntok_t *pkTok = &tokens[ix];
            // top-level "channels" key
            if ( (pkTok->parent == 0) && JSON_STREQ(resp, pkTok, PSTR("channels")) )
            {
                //DEBUG("channels at %d", ix);
                // so the next token must be an array and point back to this token
                if ( (tokens[ix + 1].type == JSMN_ARRAY) &&
                     (tokens[ix + 1].parent == ix) )
                {
                    chArrTokIx = ix + 1;
                }
                else
                {
                    WARNING("json: no channels?");
                    okay = false;
                }
                break;
            }
        }
        if (!okay || (chArrTokIx < 0))
        {
            break;
        }
        //DEBUG("chArrTokIx=%d", chArrTokIx);

        // check number of array elements
        if (tokens[chArrTokIx].size != NUMOF(sChannels))
        {
            WARNING("json: channels %d!=%d", tokens[chArrTokIx].size, (int)NUMOF(sChannels));
            okay = false;
            break;
        }

        // parse array
        for (int chIx = 0; chIx < (int)NUMOF(sChannels); chIx++)
        {
            // expected start of two element array with state and result
            const int srIx = chArrTokIx + 1 + (chIx * 3);
            //DEBUG("chIx=%d srIx=%d", chIx, srIx);
            if ( (tokens[srIx].type != JSMN_ARRAY) ||(tokens[srIx].size != 2) )
            {
                WARNING("json: channels format (srIx=%d, type=%d, size=%d)",
                    srIx, tokens[srIx].type, tokens[srIx].size);
                okay = false;
                break;
            }
            const int sIx = srIx + 1;
            const int rIx = srIx + 2;
            if ( (tokens[sIx].type != JSMN_STRING) || (tokens[rIx].type != JSMN_STRING) )
            {
                WARNING("json: channels format (sIx=%d, type=%d / rIx=%d, type=%d",
                    sIx, tokens[sIx].type, rIx, tokens[rIx].type);
                okay = false;
                break;
            }

            resp[ tokens[sIx].end ] = '\0';
            const char *stateStr    = &resp[ tokens[sIx].start ];
            sChannels[chIx].jState  = sStrToState(stateStr);

            resp[ tokens[rIx].end ] = '\0';
            const char *resultStr   = &resp[ tokens[rIx].start ];
            sChannels[chIx].jResult = sStrToResult(resultStr);

            DEBUG("chIx=%d: %s %d, %s %d", chIx, stateStr,
                sChannels[chIx].jState, resultStr, sChannels[chIx].jResult);
        }

        break;

    }
    if (okay)
    {
        DEBUG("json: parse okay");
        done = true;
    }

#else // (STATUS_USE_JSON > 0)

    // decode response
    {
        int chIx = 0;
        char *saveptr = NULL;
        while ( !done && (chIx < (int)NUMOF(sChannels)) )
        {
            const char *stateStr  = strtok_r(saveptr == NULL ? resp : NULL, " ", &saveptr);
            const char *resultStr = stateStr != NULL ? strtok_r(NULL, " ", &saveptr) : NULL;
            if ( (stateStr != NULL) && (resultStr != NULL) )
            {
                //DEBUG("%u/%u: pair: %s %s", chIx, NUMOF(sChannels), stateStr, resultStr);
                CS_ENTER;
                sChannels[chIx].jState  = sStrNumToState(stateStr);
                sChannels[chIx].jResult = sStrNumToResult(resultStr);
                CS_LEAVE;
                chIx++;
            }
            else
            {
                done = true;
            }
        }
        if (chIx == NUMOF(sChannels))
        {
            done = true;
        }
        else
        {
            WARNING("weird response");
            done = false;
        }
    }

#endif // (STATUS_USE_JSON > 0)

    if (done)
    {
        JENKINS_RESULT_t worstResult = JENKINS_RESULT_OFF;
        for (int chIx = 0; chIx < (int)NUMOF(sChannels); chIx++)
        {
            if (sChannels[chIx].jResult > worstResult)
            {
                worstResult = sChannels[chIx].jResult;
            }
        }
        const char *whatnow = "nothing";
        // Chewbacca?
        if ( (worstResult > sWorstResult) && (worstResult == JENKINS_RESULT_FAILURE) )
        {
            whatnow = "Chewbacca";
            sLoopTriggerChewie();
        }
        // Indiana?
        else if ( (worstResult < sWorstResult) && (worstResult == JENKINS_RESULT_SUCCESS) )
        {
            whatnow = "Indiana";
            toneBuiltinMelody("IndianaShort");
        }
        PRINT("worst result: %s (was: %s) --> %s",
            skJenkinsResultStrs[worstResult], skJenkinsResultStrs[sWorstResult], whatnow);
        sWorstResult = worstResult;
    }


    return done;
}


// --------------------------------------------------------------------------------------------------

static os_timer_t sLoopTimer;

static uint32_t sLoopTimerTick = 0;

static void ICACHE_FLASH_ATTR sLoopTimerFunc(void *arg)
{
    UNUSED(arg);
    static const uint8_t ledIxMap[USER_LOOP_NUMLEDS] = { 7, 0, 6, 1, 5, 2, 4, 3 }; // vertical
    //static const uint8_t ledIxMap[USER_LOOP_NUMLEDS] = { 0, 1, 2, 3, 7, 6, 5, 4 }; // horizonral

    if ( (sLoopTimerTick - sChannelsUpdateTick) == UPDATE_EXPIRE )
    {
        WARNING("update info expired%s", "!");
        sSetChannelsUnknown();
    }

    // update LEDs according to the channel states and results
    static int16_t phase;
    if ( (sLoopTimerTick % 10) == 0 )
    {
        phase++;
        phase %= (2 * NUM_STEPS);

        for (uint16_t chIx = 0; chIx < NUMOF(sChannels); chIx++)
        {
            const int8_t ph = phase - NUM_STEPS;
            const uint8_t ampl = ABS(ph);
            switch (sChannels[chIx].jResult)
            {
                case JENKINS_RESULT_OFF:
                    ws2801SetHSV(ledIxMap[chIx], 0, 0, 0);
                    break;
                case JENKINS_RESULT_UNKNOWN:
                    ws2801SetHSV(ledIxMap[chIx], UNKNOWN_HUE, UNKNOWN_SAT,
                        sStateVal(sChannels[chIx].jState, ampl, MIN_VAL, UNKNOWN_VAL));
                    break;
                case JENKINS_RESULT_SUCCESS:
                    ws2801SetHSV(ledIxMap[chIx], SUCCESS_HUE, SUCCESS_SAT,
                        sStateVal(sChannels[chIx].jState, ampl, MIN_VAL, SUCCESS_VAL));
                    break;
                case JENKINS_RESULT_UNSTABLE:
                    ws2801SetHSV(ledIxMap[chIx], UNSTABLE_HUE, UNSTABLE_SAT,
                        sStateVal(sChannels[chIx].jState, ampl, MIN_VAL, UNSTABLE_VAL));
                    break;
                case JENKINS_RESULT_FAILURE:
                    ws2801SetHSV(ledIxMap[chIx], FAILURE_HUE, FAILURE_SAT,
                        sStateVal(sChannels[chIx].jState, ampl, MIN_VAL, FAILURE_VAL));
                    break;
            }
        }
        ws2801Flush();
    }

    sLoopTimerTick++;
}


/* ***** status update stuff ********************************************************************* */

typedef enum UPDATE_STATE_e
{
    UPDATE_INIT, UPDATE_LOOKUP, UPDATE_CONNECT, UPDATE_GET, UPDATE_RESP, UPDATE_DONE, UPDATE_FAIL,
} UPDATE_STATE_t;
static const char skUpdateStateStrs[][8] PROGMEM =
{
    { "INIT\0" }, { "LOOKUP\0" }, { "CONNECT\0" }, { "GET\0" }, { "RESP\0" }, { "DONE\0" }, { "FAIL\0" }
};

static os_timer_t sUpdateTimer;
static UPDATE_STATE_t sUpdateState;
#define ARM_UPDATE(state, dt) do { sUpdateState = state; \
        os_timer_disarm(&sUpdateTimer); \
        os_timer_arm(&sUpdateTimer, (dt), 0); } while (0)

//XXX static ip_addr_t sHostAddr;
//XXX static char      sHostResponse[1024];
//XXX static uint16_t  sHostRespLen;
static uint32_t  sLatUpdateTick;

// --------------------------------------------------------------------------------------------------

// forward declarations
//XXX static void sHostlookupCb(const char *name, ip_addr_t *ipaddr, void *arg);
//XXX static void sHostConnectCb(void *arg);
//XXX static void sHostDisconnectCb(void *arg);
//XXX static void sHostReconnectCb(void *arg, sint8 err);
//XXX static void sHostTcpSentCb(void *arg);
//XXX static void sHostTcpRecvCb(void *arg, char *data, uint16_t size);

// --------------------------------------------------------------------------------------------------

static void ICACHE_FLASH_ATTR sUpdateTimerFunc(void *arg)
{
//XXX    static struct espconn   sHostConn;
//XXX    static struct _esp_tcp  sHostTcp;

    const UPDATE_STATE_t state = *((UPDATE_STATE_t *)arg);

    DEBUG("sUpdateTimerFunc() UPDATE_%s", skUpdateStateStrs[state]);

    static uint32_t updateStart;
    USER_CFG_t userCfg;
    cfgGet(&userCfg);
//XXX     char *pStatusHost = NULL, *pStatusPath = NULL;

    switch (state)
    {
        // 1. check if we're online or try again later
        case UPDATE_INIT:
        {
//XXX #if 0
//XXX             // update hostname and status URL
//XXX             cfgGet(&userCfg);
//XXX             if (userCfg.statusUrl[0])
//XXX             {
//XXX                 const char *pProtSep = os_strstr(userCfg.updateHost, "://");
//XXX             }
//XXX             else
//XXX             {
//XXX                 pStatusHost = NULL;
//XXX                 pStatusPath = NULL;
//XXX             }
//XXX #endif
//XXX             if ( (pStatusHost == NULL) || (pStatusPath == NULL) )
//XXX             {
//XXX                 WARNING("update: no config");
//XXX                 statusSet(USER_STATUS_OFFLINE);
//XXX                 ARM_UPDATE(UPDATE_INIT, UPDATE_INTERVAL_NORMAL); // try again later
//XXX             }
//XXX             else if (wifiOnline())
//XXX             {
//XXX                 DEBUG("update: online");
//XXX                 updateStart = system_get_time() / 1000; // ms
//XXX                statusSet(USER_STATUS_HEARTBEAT);
//XXX                ARM_UPDATE(UPDATE_LOOKUP, 50); // proceed to status host lookup
//XXX            }
//XXX            else
//XXX            {
//XXX                WARNING("update: offline");
//XXX                statusSet(USER_STATUS_OFFLINE);
//XXX                ARM_UPDATE(UPDATE_INIT, UPDATE_INTERVAL_RETRY); // try again later
//XXX                return;
//XXX            }
            ARM_UPDATE(UPDATE_DONE, 50); // try again later
            break;
        }

        // 2. lookup hostname
        case UPDATE_LOOKUP:
        {
//XXX            // get hostname
//XXX
//XXX
//XXX            statusSet(USER_STATUS_UPDATE);
//XXX            PRINT("Update from https://%s%s", STATUS_HOST, STATUS_PATH);
//XXX            os_memset(&sHostConn, 0, sizeof(sHostConn));
//XXX            os_memset(&sHostTcp, 0, sizeof(sHostTcp));
//XXX            sHostConn.proto.tcp = &sHostTcp;
//XXX            sHostConn.type      = ESPCONN_TCP;
//XXX            sHostConn.state     = ESPCONN_NONE;
//XXX            sHostAddr.addr      = 0;
//XXX            // install callback that, on success, proceeds to step connect and get
//XXX            const int8_t res = espconn_gethostbyname(&sHostConn, STATUS_HOST, &sHostAddr, sHostlookupCb);
//XXX            if ( (res != ESPCONN_OK) && (res != ESPCONN_INPROGRESS) )
//XXX            {
//XXX                WARNING("update: hostlookup error %d", res);
//XXX                ARM_UPDATE(UPDATE_FAIL, 50); // proceed to fail tep
//XXX            }
//XXX            //UNUSED(sHostConn); UNUSED(sHostTcp); UNUSED(sHostAddr); UNUSED(sHostlookupCb);
            break;
        }

        // 3. connect to web server
        case UPDATE_CONNECT:
        {
//XXX            DEBUG("connect " IPSTR " (%p)", IP2STR((struct ip_addr *)&sHostAddr), &sHostConn);
//XXX
//XXX            espconn_disconnect(&sHostConn);
//XXX            os_memset(&sHostConn, 0, sizeof(sHostConn));
//XXX            os_memset(&sHostTcp, 0, sizeof(sHostTcp));
//XXX            sHostConn.type      = ESPCONN_TCP;
//XXX            sHostConn.state     = ESPCONN_NONE;
//XXX            sHostConn.proto.tcp = &sHostTcp;
//XXX
//XXX            sHostConn.proto.tcp->local_port = espconn_port();
//XXX            sHostConn.proto.tcp->remote_port = 443;
//XXX            os_memcpy(sHostConn.proto.tcp->remote_ip, &sHostAddr.addr, 4 /*sizeof(sHostConn.proto.tcp->remote_ip)*/);
//XXX
//XXX            espconn_regist_connectcb(&sHostConn, sHostConnectCb);
//XXX            espconn_regist_reconcb(&sHostConn, sHostReconnectCb);
//XXX            espconn_regist_disconcb(&sHostConn, sHostDisconnectCb);
//XXX            espconn_regist_recvcb(&sHostConn, sHostTcpRecvCb);
//XXX            espconn_regist_sentcb(&sHostConn, sHostTcpSentCb);
//XXX            espconn_secure_set_size(ESPCONN_CLIENT, 8192);
//XXX
//XXX            const int8_t res = espconn_secure_connect(&sHostConn);
//XXX            if (res != ESPCONN_OK)
//XXX            {
//XXX                WARNING("connect error %d", res);
//XXX                ARM_UPDATE(UPDATE_FAIL, 50);
//XXX            }
            break;
        }

        // 4. get channels status from webserver
        case UPDATE_GET:
        {
//XXX            static char *buf[256];
//XXX            //#define STATUS_HTTP_GET "GET " STATUS_PATH " HTTP/1.1\r\nHost: " STATUS_HOST "\r\nConnection: close\r\nUser-Agent: " PROJECT "/1.0\r\n\r\n"
//XXX            //os_strcpy(buf, STATUS_HTTP_GET, 256);
//XXX            os_sprintf(buf,
//XXX                "GET %s HTTP/1.1\r\n"       // HTTP GET request
//XXX                "Host: %s\r\n"              // provide host name for virtual hosts
//XXX                "Connection: close\r\n"     // ask server to close the connection after it sent the data
//XXX                "User-Agent: %s/%s\r\n"     // be nice
//XXX                "\r\n",                     // end of request headers
//XXX                STATUS_PATH, STATUS_HOST, FF_PROJECT, FF_BUILDVER);
//XXX            const int len = os_strlen(buf);
//XXX            //DEBUG("get %s [%d]", buf, len);
//XXX            //DEBUG("request headers %d bytes", len);
//XXX
//XXX            sHostRespLen = 0;
//XXX            os_memset(sHostResponse, 0, sizeof(sHostResponse));
//XXX
//XXX            const int8_t res = espconn_secure_sent(&sHostConn, (uint8_t *)buf, len);
//XXX            if (res != ESPCONN_OK)
//XXX            {
//XXX                WARNING("connect error %d", res);
//XXX                ARM_UPDATE(UPDATE_FAIL, 50);
//XXX            }
//XXX
            break;
        }

        // 5. parse web server response (collected by sHostTcpRecvCb() configured in UPDATE_CONNECT above)
        case UPDATE_RESP:
        {
//XXX            char *body = os_strstr(sHostResponse, "\r\n\r\n");
//XXX            if ( (sHostRespLen == 0) || (body == NULL) )
//XXX            {
//XXX                WARNING("no response (size=%u, body=%p)", sHostRespLen, body);
//XXX                ARM_UPDATE(UPDATE_FAIL, 50);
//XXX                break;
//XXX            }
//XXX            body += 4; // strip leading \r\n\r\n
//XXX
//XXX            // make sure that we have a Content-Length header because we won't handle
//XXX            // https://en.wikipedia.org/wiki/Chunked_transfer_encoding markers
//XXX            if (strcasestr(sHostResponse, "Transfer-Encoding") != NULL)
//XXX            {
//XXX                WARNING("cannot handle Transfer-Encoding in response");
//XXX                ARM_UPDATE(UPDATE_FAIL, 50);
//XXX                break;
//XXX            }
//XXX
//XXX            //DEBUG("reponse body: size=%u, offset=%u, data=%s", sHostRespLen, body - sHostResponse, body);
//XXX            const uint16_t bodyLen = sHostRespLen - (body - sHostResponse);
//XXX            DEBUG("reponse: size=%u, body: size=%u, offset=%u, body=%s",
//XXX                sHostRespLen, bodyLen, body - sHostResponse, body);
//XXX
//XXX            // update status
//XXX            if (sSetChannelsFromResp(body))
//XXX            {
//XXX                sChannelsUpdateTick = sLoopTimerTick;
//XXX                ARM_UPDATE(UPDATE_DONE, 50);
//XXX            }
//XXX            else
//XXX            {
//XXX                ARM_UPDATE(UPDATE_FAIL, 50);
//XXX            }
//XXX
//XXX            break;
        }

        // update failed, try again in 10s
        case UPDATE_FAIL:
        {
//XXX            ERROR("update fail (%ums)", (system_get_time() / 1000) - updateStart);
//XXX            espconn_disconnect(&sHostConn);
//XXX
//XXX            statusSet(USER_STATUS_FAIL);
//XXX
//XXX            ARM_UPDATE(UPDATE_INIT, UPDATE_INTERVAL_RETRY);
            break;
        }

        // 6. done, schedule next update
        case UPDATE_DONE:
        {
            PRINT("update done (%ums)", (system_get_time() / 1000) - updateStart);

            sLatUpdateTick = sLoopTimerTick;

            statusSet(USER_STATUS_HEARTBEAT);

            ARM_UPDATE(UPDATE_INIT, UPDATE_INTERVAL_NORMAL);
            break;
        }

    }
}

// --------------------------------------------------------------------------------------------------

//XXXstatic void ICACHE_FLASH_ATTR sHostlookupCb(const char *name, ip_addr_t *pAddr, void *arg)
//XXX{
//XXX    struct espconn *pHostConn = (struct espconn *)arg; // same as &sHostConn
//XXX
//XXX    if (pAddr == NULL)
//XXX    {
//XXX        WARNING("sHostlookupCb() %s", "pAddr null");
//XXX        ARM_UPDATE(UPDATE_FAIL, 50);
//XXX        return;
//XXX    }
//XXX
//XXX    DEBUG("sHostlookupCb(%p) name=%s ip=" IPSTR,
//XXX        pHostConn, name, IP2STR((struct ip_addr *)pAddr));
//XXX    sHostAddr.addr = pAddr->addr;
//XXX
//XXX    // hostname lookup successful, connect to host to get Jenkins status
//XXX    ARM_UPDATE(UPDATE_CONNECT, 50); // proceed to step 3
//XXX}

// --------------------------------------------------------------------------------------------------

//XXXstatic void ICACHE_FLASH_ATTR sHostConnectCb(void *arg)
//XXX{
//XXX    struct espconn *pHostConn = (struct espconn *)arg; // same as &sHostConn
//XXX
//XXX    DEBUG("sHostConnectCb(%p) connected", pHostConn);
//XXX
//XXX    // proceed with getting data from server
//XXX    ARM_UPDATE(UPDATE_GET, 50);
//XXX}

// --------------------------------------------------------------------------------------------------

//XXXstatic void ICACHE_FLASH_ATTR sHostReconnectCb(void *arg, int8_t err)
//XXX{
//XXX    struct espconn *pHostConn = (struct espconn *)arg; // same as &sHostConn
//XXX
//XXX    WARNING("sHostReconnectCb(%p) error %d", pHostConn, err);
//XXX
//XXX    // give up, we'll try again later
//XXX    ARM_UPDATE(UPDATE_FAIL, 50);
//XXX}

// --------------------------------------------------------------------------------------------------

//XXXstatic void ICACHE_FLASH_ATTR sHostDisconnectCb(void *arg)
//XXX{
//XXX    struct espconn *pHostConn = (struct espconn *)arg; // same as &sHostConn
//XXX
//XXX    DEBUG("sHostDisconnectCb(%p) disconnect", pHostConn);
//XXX
//XXX    // proceed with processing server response
//XXX    ARM_UPDATE(UPDATE_RESP, 50);
//XXX}

// --------------------------------------------------------------------------------------------------

//XXXstatic void ICACHE_FLASH_ATTR sHostTcpRecvCb(void *arg, char *data, uint16_t size)
//XXX{
//XXX    struct espconn *pHostConn = (struct espconn *)arg; // same as &sHostConn
//XXX
//XXX    //static char *buf[1024];
//XXX    //os_memset(buf, 0, sizeof(buf));
//XXX    //os_memcpy(buf, data, size > 1024 ? 1024 - 1 : size);
//XXX    //DEBUG("sHostTcpRecvCb() size=%u, data=%s", size, data /*buf*/);
//XXX
//XXX    // copy received data to response buffer
//XXX    const uint16_t tail = sHostRespLen + size;
//XXX    if (tail > sizeof(sHostResponse))
//XXX    {
//XXX        WARNING("sHostTcpRecvCb(%p) buf full %u > %u",
//XXX            pHostConn, tail, sizeof(sHostResponse));
//XXX        ARM_UPDATE(UPDATE_FAIL, 50);
//XXX    }
//XXX    else
//XXX    {
//XXX        os_memcpy(&sHostResponse[sHostRespLen], data, size);
//XXX        sHostRespLen += size;
//XXX        DEBUG("sHostTcpRecvCb(%p) size=%u, total=%u",
//XXX            pHostConn, size, sHostRespLen);
//XXX    }
//XXX}

// --------------------------------------------------------------------------------------------------

//XXXstatic void ICACHE_FLASH_ATTR sHostTcpSentCb(void *arg)
//XXX{
//XXX    struct espconn *pHostConn = (struct espconn *)arg; // same as &sHostConn
//XXX
//XXX    DEBUG("sHostTcpSentCb(%p) sent", pHostConn);
//XXX}

// --------------------------------------------------------------------------------------------------

#define BUTTON_TIMER_INTERVAL 50
static os_timer_t sButtonTimer;

static void ICACHE_FLASH_ATTR sButtonTimerFunc(void *arg)
{
    static uint32_t pressedTime;
    // button press pulls signal low
    if ( !GPIO_IN_READ(PIN_D3) )
    {
        if (pressedTime++ == (500/BUTTON_TIMER_INTERVAL))
        {
            DEBUG("pressedTime=%u", pressedTime);
            if (sUpdateState == UPDATE_INIT)
            {
                sSetChannelsUnknown();
                PRINT("force update");
                ARM_UPDATE(UPDATE_INIT, 50);
            }
            else
            {
                WARNING("ignoring manual update (state=%s)", skUpdateStateStrs[sUpdateState]);
            }
        }
    }
    // reset on button release
    else
    {
        pressedTime = 0;
    }
    //ARM_UPDATE(UPDATE_INIT, UPDATE_INTERVAL_NORMAL);
}


/* ***** web interface *************************************************************************** */

static const char skLoopStatusStyles[] PROGMEM = USER_LOOP_STATUS_HTML_STR;

#define LOOP_STATUS_HTML_SIZE (6 * 1024)

static bool ICACHE_FLASH_ATTR sLoopStatusRequestCb(struct espconn *pConn, const HTTPD_REQCB_INFO_t *pkInfo)
{
    char *pContent = memAlloc(LOOP_STATUS_HTML_SIZE);
    if (pContent == NULL)
    {
        ERROR("sLoopStatusRequestCb(%p) malloc %u fail", pConn, LOOP_STATUS_HTML_SIZE);
        return false;
    }

    UNUSED(pkInfo);

    strcpy_P(pContent, skLoopStatusStyles);

    strcat_P(pContent, PSTR("<table class=\"status\">"));

    for (int chIx = 0; chIx < (int)NUMOF(sChannels); chIx++)
    {
        strcat_P(pContent, "<tr><td>");
        const char *colour;
        switch (sChannels[chIx].jResult)
        {
            case JENKINS_RESULT_SUCCESS:  colour = PSTR("green");   break;
            case JENKINS_RESULT_UNSTABLE: colour = PSTR("yellow");  break;
            case JENKINS_RESULT_FAILURE:  colour = PSTR("red");     break;
            case JENKINS_RESULT_UNKNOWN:  colour = PSTR("unknown"); break;
            default:
            case JENKINS_RESULT_OFF:      colour = PSTR("off");     break;
        }
        sprintf_PP(&pContent[os_strlen(pContent)], PSTR("<div class=\"led led-%s"), colour);
        if (sChannels[chIx].jState == JENKINS_STATE_RUNNING)
        {
            sprintf_PP(&pContent[os_strlen(pContent)], PSTR(" led-%s-ani"), colour);
        }
        sprintf_PP(&pContent[os_strlen(pContent)], PSTR("\"></div></td><td>%s</td><td>%s</td></tr>"),
            skJenkinsStateStrs[ sChannels[chIx].jState ],
            skJenkinsResultStrs[ sChannels[chIx].jResult ]);
    }
    strcat_P(pContent, PSTR("</table>"));
    /*
    os_strcat(pContent, "<pre>");
    os_sprintf(&pContent[os_strlen(pContent)],
        "state=%s tick=%u age=%u/%u worst=%s\n",
        skUpdateStateStrs[sUpdateState], sLoopTimerTick,
        sLoopTimerTick - sLatUpdateTick, UPDATE_INTERVAL_NORMAL,
        skJenkinsResultStrs[sWorstResult]);

    char chStr[1024];
    chStr[0] = '\0';
    for (uint16_t chIx = 0; chIx < NUMOF(sChannels); chIx++)
    {
        os_sprintf(&chStr[os_strlen(chStr)],
            "ch%02d=%s/%s ",
            chIx,
            skJenkinsStateStrs[ sChannels[chIx].jState ],
            skJenkinsResultStrs[ sChannels[chIx].jResult ]);
        if ( (((chIx+1) % 4) == 0) || (chIx == (NUMOF(sChannels)-1)) )
        {
            os_strcat(chStr, "\n");
            os_strcat(pContent, chStr);
            chStr[0] = '\0';
        }
    }
    os_strcat(pContent, "</pre>");
    */

    // send response
    const bool res = httpSendHtmlPage(pConn, pContent, false);

    // clean up
    memFree(pContent);

    return res;
}


static const char skLoopSoundHtml[] PROGMEM = USER_LOOP_SOUND_HTML_STR;

static bool ICACHE_FLASH_ATTR sLoopSoundRequestCb(struct espconn *pConn, const HTTPD_REQCB_INFO_t *pkInfo)
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
            toneStop();
            toneBuiltinMelody(pkInfo->vals[0]);
            redirect = true;
        }
        else if (strcmp_PP(pkInfo->keys[0], PSTR("rtttl")) == 0)
        {
            toneStop();
            toneMelodyRTTTL(pkInfo->vals[0]);
            redirect = true;
        }
        else if (strcmp_PP(pkInfo->keys[0], PSTR("chewie")) == 0)
        {
            toneStop();
            sLoopTriggerChewie();
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
    return httpSendHtmlPage(pConn, skLoopSoundHtml, false);
}


/* ***** initialisation ************************************************************************** */

void ICACHE_FLASH_ATTR loopInit(void)
{
    DEBUG("loop: init");

    // setup LED timer
    os_timer_disarm(&sLoopTimer);
    os_timer_setfn(&sLoopTimer, (os_timer_func_t *)sLoopTimerFunc, NULL);

    // setup update timer
    os_timer_disarm(&sUpdateTimer);
    os_timer_setfn(&sUpdateTimer, (os_timer_func_t *)sUpdateTimerFunc, &sUpdateState);

    // button
    GPIO_ENA_PIN_D3();
    GPIO_DIR_CLR(PIN_D3); // input
    PIN_PULLUP_EN(PERIPHS_IO_MUX_GPIO0_U);

    os_timer_disarm(&sButtonTimer);
    os_timer_setfn(&sButtonTimer, (os_timer_func_t *)sButtonTimerFunc, NULL);

    // Chewie trigger
    GPIO_ENA_PIN_D1();
    GPIO_DIR_SET(PIN_D1);
    GPIO_OUT_SET(PIN_D1);

    os_timer_disarm(&sChewieTimer);
    os_timer_setfn(&sChewieTimer, (os_timer_func_t *)sChewieTimerFunc, NULL);

    sSetChannelsUnknown();

    // setup web interface menu entries
    htmlRegisterMenu(PSTR("home"),   PSTR("/"));
    htmlRegisterMenu(PSTR("status"), PSTR("/status"));
    htmlRegisterMenu(PSTR("sound"),  PSTR("/sound"));
    htmlRegisterMenu(PSTR("wifi"),   PSTR("/wifi"));
    htmlRegisterMenu(PSTR("config"), PSTR("/cfg"));

    httpdRegisterRequestCb(PSTR("/status"), HTTPD_AUTH_USER, sLoopStatusRequestCb);
    httpdRegisterRequestCb(PSTR("/sound"),  HTTPD_AUTH_USER, sLoopSoundRequestCb);
}


// --------------------------------------------------------------------------------------------------


void ICACHE_FLASH_ATTR loopStart(void)
{
    // start "loop timer" (for the LEDs)
    os_timer_arm(&sLoopTimer, 1, 1); // 1ms interval, repeated

    ARM_UPDATE(UPDATE_INIT, UPDATE_INTERVAL_RETRY); // fire update timer

    os_timer_arm(&sButtonTimer, BUTTON_TIMER_INTERVAL, 1); // 1ms interval, repeated
}


// --------------------------------------------------------------------------------------------------

void ICACHE_FLASH_ATTR loopStatus(void)
{
    DEBUG("mon: loop: state=%s tick=%u age=%u/%u worst=%s",
        skUpdateStateStrs[sUpdateState], sLoopTimerTick,
        sLoopTimerTick - sLatUpdateTick, UPDATE_INTERVAL_NORMAL,
        skJenkinsResultStrs[sWorstResult]);

    char chStr[1024];
    chStr[0] = '\0';
    for (uint16_t chIx = 0; chIx < NUMOF(sChannels); chIx++)
    {
        sprintf_PP(&chStr[os_strlen(chStr)], PSTR(" ch%02d=%s/%s"),
            chIx,
            skJenkinsStateStrs[ sChannels[chIx].jState ],
            skJenkinsResultStrs[ sChannels[chIx].jResult ]);
        if ( (((chIx+1) % 4) == 0) || (chIx == (NUMOF(sChannels)-1)) )
        {
            DEBUG("mon: loop:%s", chStr);
            chStr[0] = '\0';
        }
    }
}

// eof
