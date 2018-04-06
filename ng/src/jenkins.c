/*!
    \file
    \brief flipflip's Tschenggins Lämpli: Jenkins (see \ref FF_JENKINS)

    - Copyright (c) 2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli
*/

#include "stdinc.h"

#include "debug.h"
#include "stuff.h"
#include "leds.h"
#include "config.h"
#include "tone.h"
#include "jenkins.h"

static const char * const skJenkinsStateStrs[] =
{
    [JENKINS_STATE_UNKNOWN] = "unknown", [JENKINS_STATE_OFF] = "off",
    [JENKINS_STATE_RUNNING] = "running", [JENKINS_STATE_IDLE] = "idle",
};

static const char * const  skJenkinsResultStrs[] =
{
    [JENKINS_RESULT_UNKNOWN] = "unknown", [JENKINS_RESULT_UNSTABLE] = "unstable",
    [JENKINS_RESULT_SUCCESS] = "success", [JENKINS_RESULT_FAILURE] "failure",
};

JENKINS_STATE_t jenkinsStrToState(const char *str)
{
    JENKINS_STATE_t state = JENKINS_STATE_UNKNOWN;
    if (strcmp(str, skJenkinsStateStrs[JENKINS_STATE_IDLE]) == 0)
    {
        state = JENKINS_STATE_IDLE;
    }
    else if (strcmp(str, skJenkinsStateStrs[JENKINS_STATE_RUNNING]) == 0)
    {
        state = JENKINS_STATE_RUNNING;
    }
    else if (strcmp(str, skJenkinsStateStrs[JENKINS_STATE_OFF]) == 0)
    {
        state = JENKINS_STATE_OFF;
    }
    return state;
}

JENKINS_RESULT_t jenkinsStrToResult(const char *str)
{
    JENKINS_RESULT_t result = JENKINS_RESULT_UNKNOWN;
    if (strcmp(str, skJenkinsResultStrs[JENKINS_RESULT_FAILURE]) == 0)
    {
        result = JENKINS_RESULT_FAILURE;
    }
    else if (strcmp(str, skJenkinsResultStrs[JENKINS_RESULT_UNSTABLE]) == 0)
    {
        result = JENKINS_RESULT_UNSTABLE;
    }
    else if (strcmp(str, skJenkinsResultStrs[JENKINS_RESULT_SUCCESS]) == 0)
    {
        result = JENKINS_RESULT_SUCCESS;
    }
    return result;
}

const char *jenkinsStateToStr(const JENKINS_STATE_t state)
{
    switch (state)
    {
        case JENKINS_STATE_UNKNOWN: return "unknown";
        case JENKINS_STATE_OFF:     return "off";
        case JENKINS_STATE_RUNNING: return "running";
        case JENKINS_STATE_IDLE:    return "idle";
    }
    return "???";
}

const char *jenkinsResultToStr(const JENKINS_RESULT_t result)
{
    switch (result)
    {
        case JENKINS_RESULT_UNKNOWN:  return "unknown";
        case JENKINS_RESULT_SUCCESS:  return "success";
        case JENKINS_RESULT_UNSTABLE: return "unstable";
        case JENKINS_RESULT_FAILURE:  return "failure";
    }
    return "???";
}

static const LEDS_STATE_t *sJenkinsLedStateFromJenkins(const JENKINS_STATE_t state, const JENKINS_RESULT_t result)
{
    static const LEDS_STATE_t skLedStateOff = { 0 };
    const LEDS_STATE_t *pRes = &skLedStateOff;

    switch (state)
    {
        case JENKINS_STATE_RUNNING:
        {
            switch (result)
            {
                case JENKINS_RESULT_SUCCESS:
                {
                    static const LEDS_STATE_t skLedState = { .hue = 85, .sat = 255, .val = 255, .minVal = 30, .maxVal = 255, .dVal = 2 };
                    pRes = &skLedState;
                    break;
                }
                case JENKINS_RESULT_UNSTABLE:
                {
                    static const LEDS_STATE_t skLedState = { .hue = 38, .sat = 255, .val = 255, .minVal = 30, .maxVal = 255, .dVal = 2 };
                    pRes = &skLedState;
                    break;
                }
                case JENKINS_RESULT_FAILURE:
                {
                    static const LEDS_STATE_t skLedState = { .hue = 0, .sat = 255, .val = 255, .minVal = 30, .maxVal = 255, .dVal = 2 };
                    pRes = &skLedState;
                    break;
                }
                case JENKINS_RESULT_UNKNOWN:
                {
                    static const LEDS_STATE_t skLedState = { .hue = 0, .sat = 0, .val = 255, .minVal = 30, .maxVal = 255, .dVal = 2 };
                    pRes = &skLedState;
                    break;
                }
            }
            break;
        }
        case JENKINS_STATE_IDLE:
        {
            switch (result)
            {
                case JENKINS_RESULT_SUCCESS:
                {
                    static const LEDS_STATE_t skLedState = { .hue = 85, .sat = 255, .val = 255 };
                    pRes = &skLedState;
                    break;
                }
                case JENKINS_RESULT_UNSTABLE:
                {
                    static const LEDS_STATE_t skLedState = { .hue = 38, .sat = 255, .val = 255 };
                    pRes = &skLedState;
                    break;
                }
                case JENKINS_RESULT_FAILURE:
                {
                    static const LEDS_STATE_t skLedState = { .hue = 0, .sat = 255, .val = 255 };
                    pRes = &skLedState;
                    break;
                }
                case JENKINS_RESULT_UNKNOWN:
                {
                    static const LEDS_STATE_t skLedState = { .hue = 0, .sat = 0, .val = 255 };
                    pRes = &skLedState;
                    break;
                }
            }
            break;
        }
        case JENKINS_STATE_UNKNOWN:
        {
            static const LEDS_STATE_t skLedState = { .hue = 0, .sat = 0, .val = 100 };
            pRes = &skLedState;
            break;
        }
        default:
            break;
    }

    return pRes;
}


static JENKINS_INFO_t sJenkinsInfo[JENKINS_MAX_CH];
static JENKINS_RESULT_t sJenkinsWorstResult;

static QueueHandle_t sJenkinsInfoQueue;

void jenkinsSetInfo(const JENKINS_INFO_t *pkInfo)
{
    if (pkInfo != NULL)
    {
        if (xQueueSend(sJenkinsInfoQueue, pkInfo, 10) != pdTRUE)
        {
            ERROR("jenkins: queue full");
        }
    }
}

void jenkinsClearInfo(void)
{
    sJenkinsWorstResult = JENKINS_RESULT_UNKNOWN;
    for (uint16_t ix = 0; ix < JENKINS_MAX_CH; ix++)
    {
        JENKINS_INFO_t info = { .chIx = ix, .active = false };
        jenkinsSetInfo(&info);
    }
}

static void sJenkinsStoreInfo(const JENKINS_INFO_t *pkInfo)
{
    // store new info
    JENKINS_INFO_t *pInfo = NULL;
    if (pkInfo->chIx < NUMOF(sJenkinsInfo))
    {
        pInfo = &sJenkinsInfo[pkInfo->chIx];
        memcpy(pInfo, pkInfo, sizeof(*pInfo));
        if (!pInfo->active)
        {
            const int ix = pInfo->chIx;
            memset(pInfo, 0, sizeof(*pInfo));
            pInfo->chIx = ix;
        }
    }

    // inform
    if (pInfo != NULL)
    {
        if (pInfo->active)
        {
            const char *state  = jenkinsStateToStr(pInfo->state);
            const char *result = jenkinsResultToStr(pInfo->result);
            const uint32_t now = osGetPosixTime();
            const uint32_t age = now - pInfo->time;
            PRINT("jenkins: info: #%02d %-"STRINGIFY(JENKINS_JOBNAME_LEN)"s %-"STRINGIFY(JENKINS_SERVER_LEN)"s %-7s %-8s %6.1fh",
                pInfo->chIx, pInfo->job, pInfo->server, state, result, (double)age / 3600.0);
        }
        else
        {
            PRINT("jenkins: info: #%02d <unused>", pInfo->chIx);
        }
    }
}

void sJenkinsUpdate(void)
{
    DEBUG("jenkins: update");

    // update LEDs
    for (int ix = 0; ix < NUMOF(sJenkinsInfo); ix++)
    {
        const JENKINS_INFO_t *pkInfo = &sJenkinsInfo[ix];
        if (pkInfo->active)
        {
            ledsSetState(pkInfo->chIx, sJenkinsLedStateFromJenkins(pkInfo->state, pkInfo->result));
        }
        else
        {
            ledsSetState(pkInfo->chIx, sJenkinsLedStateFromJenkins(JENKINS_STATE_UNKNOWN, JENKINS_RESULT_UNKNOWN));
        }
    }


    // find worst result
    JENKINS_RESULT_t worstResult = JENKINS_RESULT_UNKNOWN;
    for (int ix = 0; ix < NUMOF(sJenkinsInfo); ix++)
    {
        const JENKINS_INFO_t *pkInfo = &sJenkinsInfo[ix];
        if (pkInfo->result >= worstResult)
        {
            worstResult = pkInfo->result;
        }
    }
    DEBUG("jenkins: worst is now %s", jenkinsResultToStr(worstResult));

    // play sound if we changed from failure/warning to success or from success/warning to failure
    // TODO: play more sounds if CONFIG_NOISE_MORE
    if (sJenkinsWorstResult != JENKINS_RESULT_UNKNOWN)
    {
        if (worstResult != sJenkinsWorstResult)
        {
            switch (worstResult)
            {
                case JENKINS_RESULT_FAILURE:
                    PRINT("jenkins: failure!");
                    if (configGetNoise() >= CONFIG_NOISE_MORE)
                    {
                        toneStop();
                        toneBuiltinMelody("ImperialShort");
                    }
                    break;
                case JENKINS_RESULT_SUCCESS:
                    PRINT("jenkins: success!");
                    if (configGetNoise() >= CONFIG_NOISE_MORE)
                    {
                        toneStop();
                        toneBuiltinMelody("IndianaShort");
                    }
                    break;
                case JENKINS_RESULT_UNSTABLE:
                    PRINT("jenkins: unstable!");
                    break;
                default:
                    break;
            }
        }
    }
    sJenkinsWorstResult = worstResult;
}


static void sJenkinsTask(void *pArg)
{
    while (true)
    {
        static JENKINS_INFO_t info;
        static bool dirty;
        if (xQueueReceive(sJenkinsInfoQueue, &info, 100))
        {
            sJenkinsStoreInfo(&info);
            dirty = true;
        }
        else
        {
            if (dirty)
            {
                sJenkinsUpdate();
                dirty = false;
            }
        }
    }
}


void jenkinsMonStatus(void)
{
    static char str[10 * 6 + 2];
    char *pStr = str;
    int len = sizeof(str) - 1;
    bool last = false;
    int ix = 0;
    while (!last)
    {
        const JENKINS_INFO_t *pkInfo = &sJenkinsInfo[ix];
        const char *stateStr  = jenkinsStateToStr(pkInfo->state);
        const char *resultStr = jenkinsResultToStr(pkInfo->result);
        const char stateChar  = pkInfo->state  == JENKINS_STATE_UNKNOWN  ? '?' : stateStr[0];
        const char resultChar = pkInfo->result == JENKINS_RESULT_UNKNOWN ? '?' : resultStr[0];
        const int n = snprintf(pStr, len, " %02i=%c%c", ix, toupper((int)stateChar), toupper((int)resultChar));
        len -= n;
        pStr += n;
        ix++;
        last = (ix >= NUMOF(sJenkinsInfo)) || (len < 1);
        if ( ((ix % 10) == 0) || last )
        {
            DEBUG("mon: jenkins:%s", str);
            pStr = str;
            len = sizeof(str) - 1;
        }
    }
    DEBUG("mon: jenkins: worst=%s", jenkinsResultToStr(sJenkinsWorstResult));
}

#define JENKINS_INFO_QUEUE_LEN 5

void jenkinsInit(void)
{
    DEBUG("jenkins: init");

    static StaticQueue_t sQueue;
    static uint8_t sQueueBuf[sizeof(JENKINS_INFO_t) * JENKINS_INFO_QUEUE_LEN];
    sJenkinsInfoQueue = xQueueCreateStatic(JENKINS_INFO_QUEUE_LEN, sizeof(JENKINS_INFO_t), sQueueBuf, &sQueue);
}

void jenkinsStart(void)
{
    DEBUG("jenkins: start");

    static StackType_t sJenkinsTaskStack[512];
    static StaticTask_t sJenkinsTaskTCB;
    xTaskCreateStatic(sJenkinsTask, "ff_jenkins", NUMOF(sJenkinsTaskStack), NULL, 2, sJenkinsTaskStack, &sJenkinsTaskTCB);
}

// eof
