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
#include "jenkins.h"
#include "status.h"
#include "tone.h"
#include "config.h"
#include "json.h"
#include "backend.h"


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
    const uint32_t now = osTime();
    if ( (now - sLastHello) > (1000 * BACKEND_STABLE_CONN_THRS) )
    {
        jenkinsUnknownAll();
    }
    else
    {
        jenkinsClearAll();
    }
    sLastHeartbeat = 0;
    sLastHello = 0;
    sBytesReceived = 0;
}

void backendMonStatus(void)
{
    const uint32_t now = osTime();
    DEBUG("mon: backend: count=%u uptime=%u (%s) heartbeat=%u bytes=%u",
        sConnCount, sLastHello ? now - sLastHello : 0,
        sLastHello ? ((now - sLastHello) > (1000 * BACKEND_STABLE_CONN_THRS) ? "stable" : "unstable" ) : "n/a",
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


static void sBackendHandleSetTime(const char *timestamp)
{
    const uint32_t ts = (uint32_t)atoi(timestamp);
    if (ts != 0)
    {
        sLastHeartbeat = osTime();
        osSetPosixTime(ts);
    }
}


// process response from backend
BACKEND_STATUS_t backendHandle(char *resp, const int len)
{
    BACKEND_STATUS_t res = BACKEND_STATUS_OKAY;
    sBytesReceived += len;

    //DEBUG("backendHandle() [%d] %s", len, resp);

    char *pConfig    = strstr(resp, "\r\n""config ");
    char *pStatus    = strstr(resp, "\r\n""status ");
    char *pHeartbeat = strstr(resp, "\r\n""heartbeat ");
    char *pError     = strstr(resp, "\r\n""error ");
    char *pReconnect = strstr(resp, "\r\n""reconnect ");
    char *pCommand   = strstr(resp, "\r\n""command ");

    // "\r\nerror 1491146601 WTF?\r\n"
    if (pError != NULL)
    {
        pError += 2;
        char *endOfLine = strstr(pError, "\r\n");
        if (endOfLine != NULL)
        {
            *endOfLine = '\0';
            sBackendHandleSetTime(&pError[6]);
            const char *pMsg = &pError[6 + 10 + 1];
            ERROR("backend: error: %s", pMsg);
        }
    }

    // "\r\nreconnect 1491146601\r\n"
    if (pReconnect != NULL)
    {
        pReconnect += 2;
        char *endOfLine = strstr(pReconnect, "\r\n");
        if (endOfLine != NULL)
        {
            *endOfLine = '\0';
            sBackendHandleSetTime(&pReconnect[10]);
            WARNING("backend: reconnect");
            res = BACKEND_STATUS_RECONNECT;
        }
    }

    // "\r\nheartbeat 1491146601 25\r\n"
    if (pHeartbeat != NULL)
    {
        pHeartbeat += 2;
        char *endOfLine = strstr(pHeartbeat, "\r\n");
        if (endOfLine != NULL)
        {
            *endOfLine = '\0';
            sBackendHandleSetTime(&pHeartbeat[10]);
            const char *pCnt = &pHeartbeat[10 + 10 + 1];
            DEBUG("backend: heartbeat %s", pCnt);
        }
    }

    // "\r\nconfig 1491146576 json={"key":"value", ... }\r\n"
    if (pConfig != NULL)
    {
        pConfig += 2;
        char *endOfLine = strstr(pConfig, "\r\n");
        if (endOfLine != NULL)
        {
            *endOfLine = '\0';
            sBackendHandleSetTime(&pConfig[7]);
            char *pJson = &pConfig[7 + 10 + 1];
            const int jsonLen = strlen(pJson);
            DEBUG("backend: config");
            if (configParseJson(pJson, jsonLen))
            {
                statusNoise(STATUS_NOISE_OTHER);
            }
            else
            {
                statusNoise(STATUS_NOISE_ERROR);
            }
        }
    }

    // "\r\nstatus 1491146576 json={"jobs": ... }\r\n"
    if (pStatus != NULL)
    {
        pStatus += 2;
        char *endOfLine = strstr(pStatus, "\r\n");
        if (endOfLine != NULL)
        {
            *endOfLine = '\0';
            sBackendHandleSetTime(&pStatus[7]);
            DEBUG("backend: status");
            char *pJson = &pStatus[7 + 10 + 1];
            const int jsonLen = strlen(pJson);
            sBackendProcessStatus(pJson, jsonLen);
        }
    }

    // "\r\ncommand 1491146601 reset\r\n"
    if (pCommand != NULL)
    {
        pCommand += 2;
        char *endOfLine = strstr(pCommand, "\r\n");
        if (endOfLine != NULL)
        {
            *endOfLine = '\0';
            sBackendHandleSetTime(&pCommand[8]);
            const char *pCmd = &pCommand[8 + 10 + 1];
            if (strcmp("reconnect", pCmd) == 0)
            {
                PRINT("backend: command reconnect");
                statusNoise(STATUS_NOISE_OTHER);
                res = BACKEND_STATUS_RECONNECT;
            }
            else if (strcmp("reset", pCmd) == 0)
            {
                WARNING("backend: command restart");
                statusNoise(STATUS_NOISE_OTHER);
                osSleep(250);
                sdk_system_restart();
            }
            else if (strcmp("identify", pCmd) == 0)
            {
                PRINT("backend: command identify");
                toneStop();
                toneBuiltinMelody("PacMan"); // ignore noise config
            }
            else if (strcmp("random", pCmd) == 0)
            {
                PRINT("backend: command random");
                toneStop();
                toneBuiltinMelodyRandom();
            }
            else
            {
                WARNING("backend: command %s ???", pCmd);
                toneStop();
                statusNoise(STATUS_NOISE_ERROR);
            }
        }
    }

    return res;
}


void sBackendProcessStatus(char *resp, const int respLen)
{
    DEBUG("backend: [%d] %s", respLen, resp);

    const int maxTokens = (6 * JENKINS_MAX_CH) + 20;
    jsmntok_t *pTokens = jsmnAllocTokens(maxTokens);
    if (pTokens == NULL)
    {
        ERROR("backend: json malloc fail");
        return;
    }

    // parse JSON response
    const int numTokens = jsmnParse(resp, respLen, pTokens, maxTokens);
    bool okay = numTokens > 0 ? true : false;

    //jsmnDumpTokens(resp, pTokens, numTokens);
    /*
      004.974 D: json 00: 2 arr   000..218 10 <-1
      004.984 D: json 01: 2 arr   001..057 6 < 0 [0,"PROJECT03","server-abc","idle","unknown",1522616716]
      004.985 D: json 02: 4 prim  002..003 0 < 1 0
      004.985 D: json 03: 3 str   005..014 0 < 1 PROJECT03
      004.995 D: json 04: 3 str   017..027 0 < 1 server-abc
      004.995 D: json 05: 3 str   030..034 0 < 1 idle
      004.995 D: json 06: 3 str   037..044 0 < 1 unknown
      005.006 D: json 07: 4 prim  046..056 0 < 1 1522616716
      005.016 D: json 08: 2 arr   058..121 6 < 0 [1,"CI_Project_Foo1","server-abc","idle","unstable",1522616736]
      005.017 D: json 09: 4 prim  059..060 0 < 8 1
      005.017 D: json 10: 3 str   062..077 0 < 8 CI_Project_Foo1
      005.027 D: json 11: 3 str   080..090 0 < 8 server-abc
      005.027 D: json 12: 3 str   093..097 0 < 8 idle
      005.027 D: json 13: 3 str   100..108 0 < 8 unstable
      005.038 D: json 14: 4 prim  110..120 0 < 8 1522616736
      005.048 D: json 15: 2 arr   122..189 6 < 0 [2,"CI_Project1_Foo02","server-abc","unknown","unknown",1505942566]
      005.048 D: json 16: 4 prim  123..124 0 <15 2
      005.048 D: json 17: 3 str   126..143 0 <15 CI_Project1_Foo02
      005.059 D: json 18: 3 str   146..156 0 <15 server-abc
      005.059 D: json 19: 3 str   159..166 0 <15 unknown
      005.059 D: json 20: 3 str   169..176 0 <15 unknown
      005.069 D: json 21: 4 prim  178..188 0 <15 1505942566
      005.069 D: json 22: 2 arr   190..193 1 < 0 [3]
      005.080 D: json 23: 4 prim  191..192 0 <22 3
      005.080 D: json 24: 2 arr   194..197 1 < 0 [4]
      005.080 D: json 25: 4 prim  195..196 0 <24 4
    */

    // process JSON data

    if (okay && (pTokens[0].type != JSMN_ARRAY))
    {
        WARNING("backend: json not array");
        okay = false;
    }

    // look for results
    if (okay)
    {
        // look for response result
        for (int ix = 0; ix < numTokens; ix++)
        {
            const jsmntok_t *pkTok = &pTokens[ix];
            // top-level array, size 1 or 6
            if ( (pkTok->parent == 0) && (pkTok->type == JSMN_ARRAY) &&
                ( (pkTok->size == 1) || (pkTok->size == 6) ) )
            {
                // create message to Jenkins task
                JENKINS_INFO_t jInfo;
                memset(&jInfo, 0, sizeof(jInfo));

                // first element is the channel index
                const jsmntok_t *pkTokCh = &pTokens[ix + 1];
                if ( (pkTokCh->type == JSMN_STRING) || (pkTokCh->type == JSMN_PRIMITIVE) )
                {
                    resp[ pkTokCh->end ] = '\0';
                    const char *chStr = &resp[ pkTokCh->start ];
                    jInfo.chIx = atoi(chStr);
                    jInfo.active = false;
                    ix++;
                }
                else
                {
                    WARNING("backend: json jobs format at %d (%d)", ix, pkTokCh->type);
                    okay = false;
                    break;
                }

                // the next five elements are the job info
                const jsmntok_t *pkTokName   = &pTokens[ix + 1];
                const jsmntok_t *pkTokServer = &pTokens[ix + 2];
                const jsmntok_t *pkTokState  = &pTokens[ix + 3];
                const jsmntok_t *pkTokResult = &pTokens[ix + 4];
                const jsmntok_t *pkTokTime   = &pTokens[ix + 5];
                if ( (pkTok->size == 6) &&
                     (pkTokName->type == JSMN_STRING) &&  (pkTokServer->type == JSMN_STRING) &&
                     (pkTokState->type == JSMN_STRING) &&  (pkTokResult->type == JSMN_STRING) &&
                     ( (pkTokTime->type == JSMN_STRING) || (pkTokTime->type == JSMN_PRIMITIVE) ) )
                {

                    resp[ pkTokName->end   ] = '\0'; const char *nameStr   = &resp[ pkTokName->start ];
                    resp[ pkTokServer->end ] = '\0'; const char *serverStr = &resp[ pkTokServer->start ];
                    resp[ pkTokState->end  ] = '\0'; const char *stateStr  = &resp[ pkTokState->start ];
                    resp[ pkTokResult->end ] = '\0'; const char *resultStr = &resp[ pkTokResult->start ];
                    resp[ pkTokTime->end   ] = '\0'; const char *timeStr   = &resp[ pkTokTime->start ];
                    ix += 5;

                    jInfo.active = true;
                    jInfo.state  = jenkinsStrToState(stateStr);
                    jInfo.result = jenkinsStrToResult(resultStr);
                    strncpy(jInfo.job, nameStr, sizeof(jInfo.job));
                    strncpy(jInfo.server, serverStr, sizeof(jInfo.server));
                    jInfo.time = (uint32_t)atoi(timeStr);
                }
                else if (pkTok->size == 6)
                {
                    WARNING("backend: json jobs format at %d (%d, %d, %d, %d, %d)", ix,
                        pkTokName->type, pkTokServer->type, pkTokState->type,
                        pkTokResult->type, pkTokTime->type);
                    okay = false;
                    break;
                }

                // send info the Jenkins task
                jenkinsSetInfo(&jInfo);
            }
        }
    }

    // are we happy?
    if (okay)
    {
        statusNoise(STATUS_NOISE_OTHER);
        DEBUG("backend: json parse okay");
    }
    else
    {
        statusNoise(STATUS_NOISE_ERROR);
        ERROR("backend: json parse fail");
    }

    // cleanup
    free(pTokens);
}



void backendInit(void)
{
    DEBUG("backend: init");
}

/* ********************************************************************************************** */

// eof
