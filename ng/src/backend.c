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

#include <jsmn.h>

#include "stuff.h"
#include "debug.h"
#include "wifi.h"
#include "jenkins.h"
#include "status.h"
#include "backend.h"
#include "cfg_gen.h"
#include "version_gen.h"

#define BACKEND_HEARTBEAT_INTERVAL 5000
#define BACKEND_HEARTBEAT_TIMEOUT (3 * BACKEND_HEARTBEAT_INTERVAL)

static uint32_t sLastHello;
static uint32_t sLastHeartbeat;
static uint32_t sBytesReceived;
static uint32_t sConnCount;

bool backendConnect(char *resp, const int len)
{
    DEBUG("backend: connect");
    // update statistics
    sLastHello = 0;
    sLastHeartbeat = 0;
    sBytesReceived = 0;
    sConnCount++;

    // look for "hello"
    char *pHello = strstr(resp, "\r\nhello ");
    if (pHello == NULL)
    {
        ERROR("backend: no 'hello' :-(");
        return false;
    }
    pHello += 2;
    char *endOfHello = strstr(pHello, "\r\n");
    if (endOfHello != NULL)
    {
        *endOfHello = '\0';
    }
    const uint32_t now = osTime();
    sLastHello = now;
    sLastHeartbeat = now;

    // handle remaining data
    char *pParse = endOfHello + 2;
    const int len2 = len - (pParse - resp);
    sBytesReceived += len - len2;
    backendHandle(pParse, len2);

    return true;
}

void backendDisconnect(void)
{
    DEBUG("backend: disconnect");
    sLastHeartbeat = 0;
    sLastHello = 0;
    sBytesReceived = 0;
}

void backendMonStatus(void)
{
    const uint32_t now = osTime();
    DEBUG("mon: backend: count=%u uptime=%u heartbeat=%u bytes=%u",
        sConnCount, sLastHello ? now - sLastHello : 0,
        sLastHeartbeat ? now - sLastHeartbeat : 0, sBytesReceived);
}

bool backendIsOkay(void)
{
    // check heartbeat
    const uint32_t now = osTime();
    if ( (now - sLastHeartbeat) > BACKEND_HEARTBEAT_TIMEOUT )
    {
        ERROR("backend: lost heartbeat");
        return false;
    }
    else
    {
        //DEBUG("backend: heartbeat %u < %u", now - sLastHeartbeat, BACKEND_HEARTBEAT_TIMEOUT);
        return true;
    }
}


// forward declarations
void sBackendProcessStatus(char *resp, const int respLen);

// process response from backend
BACKEND_STATUS_t backendHandle(char *resp, const int len)
{
    BACKEND_STATUS_t res = BACKEND_STATUS_OKAY;
    sBytesReceived += len;

    //DEBUG("backendHandle() [%d] %s", len, resp);

    char *pStatus    = strstr(resp, "\r\n""status ");
    char *pHeartbeat = strstr(resp, "\r\n""heartbeat ");
    char *pError     = strstr(resp, "\r\n""error ");
    char *pReconnect = strstr(resp, "\r\n""reconnect ");

    // "\r\nerror 1491146601 WTF?\r\n"
    if (pError != NULL)
    {
        pError += 2;
        char *endOfError = strstr(pError, "\r\n");
        if (endOfError != NULL)
        {
            *endOfError = '\0';
        }
        char *pErrorMsg = &pError[17];
        DEBUG("backend: error (%s)", pErrorMsg < endOfError ? pErrorMsg : "???");
        sLastHeartbeat = osTime();
        osSetPosixTime((uint32_t)atoi(&pError[6]));
    }

    // "\r\nreconnect 1491146601\r\n"
    if (pReconnect != NULL)
    {
        pReconnect += 2;
        char *endOfError = strstr(pReconnect, "\r\n");
        if (endOfError != NULL)
        {
            *endOfError = '\0';
        }
        WARNING("backend: reconnect");
        sLastHeartbeat = osTime();
        osSetPosixTime((uint32_t)atoi(&pReconnect[6]));
        res = BACKEND_STATUS_RECONNECT;
    }

    // "\r\nheartbeat 1491146601 25\r\n"
    if (pHeartbeat != NULL)
    {
        pHeartbeat += 2;
        char *endOfHeartbeat = strstr(pHeartbeat, "\r\n");
        if (endOfHeartbeat != NULL)
        {
            *endOfHeartbeat = '\0';
        }
        DEBUG("backend: heartbeat");
        sLastHeartbeat = osTime();
        osSetPosixTime((uint32_t)atoi(&pHeartbeat[10]));
    }

    // "\r\nstatus 1491146576 json={"jobs": ... }\r\n"
    if (pStatus != NULL)
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
            sLastHeartbeat = osTime();
            osSetPosixTime((uint32_t)atoi(&pStatus[7]));
            const int jsonLen = strlen(pJson);
            DEBUG("backend: status");
            sBackendProcessStatus(pJson, jsonLen);
        }
        else
        {
            WARNING("backend: ignoring fishy status");
        }
    }

    return res;
}

#define JSON_STREQ(json, pkTok, str) (    \
        ((pkTok)->type == JSMN_STRING) && \
        (strlen(str) == ( (pkTok)->end - (pkTok)->start ) ) && \
        (strncmp(&json[(pkTok)->start], str, (pkTok)->end - (pkTok)->start) == 0) )

#define JSON_ANYEQ(json, pkTok, str) (    \
        ( ((pkTok)->type == JSMN_STRING) || ((pkTok)->type == JSMN_PRIMITIVE) ) && \
        (strlen(str) == ( (pkTok)->end - (pkTok)->start ) ) && \
        (strncmp(&json[(pkTok)->start], str, (pkTok)->end - (pkTok)->start) == 0) )

void sBackendProcessStatus(char *resp, const int respLen)
{
    // memory for JSON parser
    const int maxTokens = (6 * JENKINS_MAX_CH) + 20; // FIXME: ???
    const int tokensSize = maxTokens * sizeof(jsmntok_t);
    jsmntok_t *pTokens = malloc(tokensSize);
    if (pTokens == NULL)
    {
        ERROR("backend: json malloc fail");
        return;
    }
    memset(pTokens, 0, tokensSize);

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
    DEBUG("backend: json %d/%d tokens, alloc %d",
        numTokens, maxTokens, tokensSize);

#if 0
    // debug json tokens
    for (int ix = 0; ix < numTokens; ix++)
    {
        static const char * const skTypeStrs[] = { "undef", "obj", "arr", "str", "prim" };
        const jsmntok_t *pkTok = &pTokens[ix];
        char buf[200];
        int sz = pkTok->end - pkTok->start;
        if ( (sz > 0) && (sz < (int)sizeof(buf)))
        {
            memcpy(buf, &resp[pkTok->start], sz);
            buf[sz] = '\0';
        }
        else
        {
            buf[0] = '\0';
        }
        char str[10];
        strcpy(str, pkTok->type < NUMOF(skTypeStrs) ? skTypeStrs[pkTok->type] : "???");
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
            WARNING("backend: json not obj");
            okay = false;
            break;
        }

        // look for response result
        for (int ix = 0; ix < numTokens; ix++)
        {
            const jsmntok_t *pkTok = &pTokens[ix];
            // top-level "res" key
            if ( (pkTok->parent == 0) && JSON_STREQ(resp, pkTok, "res") )
            {
                // so the next token must point back to this (key : value pair)
                if (pTokens[ix + 1].parent == ix)
                {
                    // and we want the result 1 (or "1")
                    if (!JSON_ANYEQ(resp, &pTokens[ix+1], "1"))
                    {
                        resp[ pTokens[ix+1].end ] = '\0';
                        WARNING("backend: json res=%s", &resp[ pTokens[ix+1].start ]);
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

        // look for "jobs" data
        int chArrTokIx = -1;
        for (int ix = 0; ix < numTokens; ix++)
        {
            const jsmntok_t *pkTok = &pTokens[ix];
            // top-level "jobs" key
            if ( (pkTok->parent == 0) && JSON_STREQ(resp, pkTok, "jobs") )
            {
                //DEBUG("jobs at %d", ix);
                // so the next token must be an array and point back to this token
                if ( (pTokens[ix + 1].type == JSMN_ARRAY) &&
                     (pTokens[ix + 1].parent == ix) )
                {
                    chArrTokIx = ix + 1;
                }
                else
                {
                    WARNING("backend: status json no jobs");
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
        if (pTokens[chArrTokIx].size > JENKINS_MAX_CH)
        {
            WARNING("backend: json jobs %d > %d", pTokens[chArrTokIx].size, JENKINS_MAX_CH);
            okay = false;
            break;
        }

        // parse array
        for (int ledIx = 0; (ledIx < JENKINS_MAX_CH) && (ledIx <pTokens[chArrTokIx].size) ; ledIx++)
        {
            const int numFields = 5;
            // expected start of fife element array with state and result
            // ["CI_foo_master","devpos-thl","idle","success",9199]
            const int arrIx = chArrTokIx + 1 + (ledIx * (numFields + 1));
            //DEBUG("ledIx=%d arrIx=%d", ledIx, arrIx);
            if ( (pTokens[arrIx].type != JSMN_ARRAY) || (pTokens[arrIx].size != numFields) )
            {
                WARNING("backend: json jobs format (arrIx=%d, type=%d, size=%d)",
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
                WARNING("backend: json jobs format (%d!=%d, %d!=%d, %d!=%d, %d!=%d, %d!=%d)",
                    pTokens[nameIx].type, JSMN_STRING, pTokens[serverIx].type, JSMN_STRING,
                    pTokens[stateIx].type, JSMN_STRING, pTokens[resultIx].type, JSMN_STRING,
                    pTokens[timeIx].type, JSMN_PRIMITIVE);
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

            //DEBUG("backend: json arrIx=%02d ledIx=%02d name=%s server=%s state=%s result=%s time=%s",
            //    arrIx, ledIx, nameStr, serverStr, stateStr, resultStr, timeStr);

            // create message to Jenkins task
            JENKINS_INFO_t jInfo;
            memset(&jInfo, 0, sizeof(jInfo));
            jInfo.state  = jenkinsStrToState(stateStr);
            jInfo.result = jenkinsStrToResult(resultStr);
            strncpy(jInfo.job, nameStr, sizeof(jInfo.job));
            strncpy(jInfo.server, serverStr, sizeof(jInfo.server));
            jInfo.time = (uint32_t)atoi(timeStr);
            jenkinsAddInfo(&jInfo);
        }

        break;
    }

    // are we happy?
    if (okay)
    {
        statusMakeNoise(STATUS_NOISE_OTHER);
        DEBUG("backend: json parse okay");
    }
    else
    {
        statusMakeNoise(STATUS_NOISE_ERROR);
        ERROR("backend: json parse fail");
    }

    // cleanup
    free(pTokens);
}



void backendInit(void)
{
    DEBUG("backendInit()");
}

/* ********************************************************************************************** */

// eof
