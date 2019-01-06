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
#include "status.h"

/* ***** external interface ********************************************************************* */

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

const char *sJenkinsStateToStr(const JENKINS_STATE_t state)
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

const char *sJenkinsResultToStr(const JENKINS_RESULT_t result)
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


typedef enum JENKINS_MSG_TYPE_e
{
    JENKINS_MSG_TYPE_INFO,
    JENKINS_MSG_TYPE_CLEAR_ALL,
    JENKINS_MSG_TYPE_UNKNOWN_ALL,
} JENKINS_MSG_TYPE_t;

typedef struct JENKINS_MSG_s
{
    JENKINS_MSG_TYPE_t type;
    union
    {
        JENKINS_INFO_t info;
    };
} JENKINS_MSG_t;

static QueueHandle_t sJenkinsMsgQueue;

void jenkinsSetInfo(const JENKINS_INFO_t *pkInfo)
{
    if (pkInfo != NULL)
    {
        JENKINS_MSG_t msg;
        memset(&msg, 0, sizeof(msg));
        msg.type = JENKINS_MSG_TYPE_INFO;
        memcpy(&msg.info, pkInfo, sizeof(msg.info));
        if (xQueueSend(sJenkinsMsgQueue, &msg, 10) != pdTRUE)
        {
            ERROR("jenkins: queue full");
        }
    }
}

void jenkinsClearAll(void)
{
    JENKINS_MSG_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = JENKINS_MSG_TYPE_CLEAR_ALL;
    if (xQueueSend(sJenkinsMsgQueue, &msg, 10) != pdTRUE)
    {
        ERROR("jenkins: queue full");
    }
}

void jenkinsUnknownAll(void)
{
    JENKINS_MSG_t msg;
    memset(&msg, 0, sizeof(msg));
    msg.type = JENKINS_MSG_TYPE_UNKNOWN_ALL;
    if (xQueueSend(sJenkinsMsgQueue, &msg, 10) != pdTRUE)
    {
        ERROR("jenkins: queue full");
    }
}


/* ***** internal things ************************************************************************ */

// convert Jenkins state and result to LED colour and mode
static const LEDS_PARAM_t *sJenkinsLedStateFromJenkins(const JENKINS_STATE_t state, const JENKINS_RESULT_t result)
{
    static const LEDS_PARAM_t skLedStateOff = { 0 };
    const LEDS_PARAM_t *pRes = &skLedStateOff;

    switch (state)
    {
        case JENKINS_STATE_RUNNING:
        {
            switch (result)
            {
                case JENKINS_RESULT_SUCCESS:
                {
                    static const LEDS_PARAM_t skLedState = LEDS_MAKE_PARAM(85, 255, 255, PULSE, 0);
                    pRes = &skLedState;
                    break;
                }
                case JENKINS_RESULT_UNSTABLE:
                {
                    static const LEDS_PARAM_t skLedState = LEDS_MAKE_PARAM(38, 255, 255, PULSE, 0);
                    pRes = &skLedState;
                    break;
                }
                case JENKINS_RESULT_FAILURE:
                {
                    static const LEDS_PARAM_t skLedState = LEDS_MAKE_PARAM(0, 255, 255, PULSE, 0);
                    pRes = &skLedState;
                    break;
                }
                case JENKINS_RESULT_UNKNOWN:
                {
                    static const LEDS_PARAM_t skLedState = LEDS_MAKE_PARAM(0, 0, 255, PULSE, 0);
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
                    static const LEDS_PARAM_t skLedState = LEDS_MAKE_PARAM(85, 255, 255, STILL, 0);
                    pRes = &skLedState;
                    break;
                }
                case JENKINS_RESULT_UNSTABLE:
                {
                    static const LEDS_PARAM_t skLedState = LEDS_MAKE_PARAM(38, 255, 255, STILL, 0);
                    pRes = &skLedState;
                    break;
                }
                case JENKINS_RESULT_FAILURE:
                {
                    static const LEDS_PARAM_t skLedState = LEDS_MAKE_PARAM(0, 255, 255, STILL, 0);
                    pRes = &skLedState;
                    break;
                }
                case JENKINS_RESULT_UNKNOWN:
                {
                    static const LEDS_PARAM_t skLedState = LEDS_MAKE_PARAM(0, 0, 200, STILL, 0);
                    pRes = &skLedState;
                    break;
                }
            }
            break;
        }
        case JENKINS_STATE_UNKNOWN:
        {
            switch (result)
            {
                case JENKINS_RESULT_SUCCESS:
                {
                    static const LEDS_PARAM_t skLedState = LEDS_MAKE_PARAM(85, 180, 128, FLICKER, 0);
                    pRes = &skLedState;
                    break;
                }
                case JENKINS_RESULT_UNSTABLE:
                {
                    static const LEDS_PARAM_t skLedState = LEDS_MAKE_PARAM(38, 220, 128, FLICKER, 0);
                    pRes = &skLedState;
                    break;
                }
                case JENKINS_RESULT_FAILURE:
                {
                    static const LEDS_PARAM_t skLedState = LEDS_MAKE_PARAM(0, 180, 128, FLICKER, 0);
                    pRes = &skLedState;
                    break;
                }
                case JENKINS_RESULT_UNKNOWN:
                {
                    static const LEDS_PARAM_t skLedState = LEDS_MAKE_PARAM(0, 0, 128, FLICKER, 0);
                    pRes = &skLedState;
                    break;
                }
            }
        }
        default:
            break;
    }

    return pRes;
}

// current info for all channels
static JENKINS_INFO_t sJenkinsInfo[JENKINS_MAX_CH];

// current dirty flag for all channels
static bool sJenkinsInfoDirty[NUMOF(sJenkinsInfo)];

// curently worst result
static JENKINS_RESULT_t sJenkinsWorstResult;

// currently most active state
static JENKINS_STATE_t sJenkinsActiveState;

// store info
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
        sJenkinsInfoDirty[pkInfo->chIx] = true;
    }

    // inform
    if (pInfo != NULL)
    {
        if (pInfo->active)
        {
            const char *state  = sJenkinsStateToStr(pInfo->state);
            const char *result = sJenkinsResultToStr(pInfo->result);
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

// clear all info
static void sJenkinsClearAll(void)
{
    PRINT("jenkins: clear all");
    memset(&sJenkinsInfo, 0, sizeof(sJenkinsInfo));
    for (int ix = 0; ix < NUMOF(sJenkinsInfo); ix++)
    {
        sJenkinsInfoDirty[ix] = true;
    }
}

// set all channels to state unknown
static void sJenkinsUnknownAll(void)
{
    PRINT("jenkins: unknown all");
    for (int ix = 0; ix < NUMOF(sJenkinsInfo); ix++)
    {
        if (sJenkinsInfo[ix].active)
        {
            sJenkinsInfo[ix].state = JENKINS_STATE_UNKNOWN;
            sJenkinsInfoDirty[ix] = true;
        }
    }
}

// update all dirty channels, re-calculate worst result, play sounds
void sJenkinsUpdate(void)
{
    DEBUG("jenkins: update");

    // update LEDs (for Lämplis with individual LEDs)
    if (configGetModel() != CONFIG_MODEL_HELLO)
    {
        for (int ix = 0; ix < NUMOF(sJenkinsInfo); ix++)
        {
            if (sJenkinsInfoDirty[ix])
            {
                sJenkinsInfoDirty[ix] = false;
                const JENKINS_INFO_t *pkInfo = &sJenkinsInfo[ix];
                if (pkInfo->active)
                {
                    ledsSetState(ix, sJenkinsLedStateFromJenkins(pkInfo->state, pkInfo->result));
                }
                else
                {
                    ledsSetState(ix, sJenkinsLedStateFromJenkins(JENKINS_STATE_UNKNOWN, JENKINS_RESULT_UNKNOWN));
                }
            }
        }
    }

    // find worst result, state
    JENKINS_RESULT_t worstResult = JENKINS_RESULT_UNKNOWN;
    JENKINS_STATE_t activeState = JENKINS_STATE_UNKNOWN;
    for (int ix = 0; ix < NUMOF(sJenkinsInfo); ix++)
    {
        const JENKINS_INFO_t *pkInfo = &sJenkinsInfo[ix];
        if (pkInfo->result >= worstResult)
        {
            worstResult = pkInfo->result;
        }
        if (pkInfo->state > activeState)
        {
            activeState = pkInfo->state;
        }
    }
    DEBUG("jenkins: worst is now %s (was %s), most active is now %s (was %s)",
        sJenkinsResultToStr(worstResult), sJenkinsResultToStr(sJenkinsWorstResult),
        sJenkinsStateToStr(activeState), sJenkinsStateToStr(sJenkinsActiveState));

    // update LEDs (for the "Hello Jenkins" model)
    if (configGetModel() == CONFIG_MODEL_HELLO)
    {
        const LEDS_PARAM_t *pkState = NULL;
        switch (activeState)
        {
            // R=0, RG=43, G=85, BG=128, B=170,
            case JENKINS_STATE_UNKNOWN:
            {
                static const LEDS_PARAM_t skState = LEDS_MAKE_PARAM(43, 255, 255, BLINK,  10);
                pkState = &skState;
                break;
            }
            case JENKINS_STATE_OFF:
            {
                static const LEDS_PARAM_t skState = LEDS_MAKE_PARAM(43, 255, 255, BLINK, 100);
                pkState = &skState;
                break;
            }
            case JENKINS_STATE_IDLE:
            {
                static const LEDS_PARAM_t skState = LEDS_MAKE_PARAM(43, 255, 255, FLICKER, 0);
                pkState = &skState;
                break;
            }
            case JENKINS_STATE_RUNNING:
            {
                static const LEDS_PARAM_t skState = LEDS_MAKE_PARAM(43, 255, 255, BLINK,  30);
                pkState = &skState;
                break;
            }
        }
        ledsSetStateHello(sJenkinsLedStateFromJenkins(activeState, worstResult), pkState);
    }

    // play sound if we changed from failure/warning to success or from success/warning to failure
    // TODO: play more sounds if CONFIG_NOISE_MORE
    // FIXME: also check for state == idle?
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
                        switch (configGetModel())
                        {
                            case CONFIG_MODEL_CHEWIE:
                                statusChewie();
                                if (configGetNoise() >= CONFIG_NOISE_MOST)
                                {
                                    toneBuiltinMelody("ImperialShort");
                                }
                                break;
                            case CONFIG_MODEL_HELLO:
                                statusHello();
                                if (configGetNoise() >= CONFIG_NOISE_MOST)
                                {
                                    toneBuiltinMelody("ImperialShort");
                                }
                                break;
                            default:
                                toneBuiltinMelody("ImperialShort");
                                break;
                        }
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
    sJenkinsActiveState = activeState;
}

// Jenkins task, waits for messages and updates LEDs accordingly
static void sJenkinsTask(void *pArg)
{
    while (true)
    {
        static JENKINS_MSG_t msg;
        static bool doUpdate;

        while (xQueueReceive(sJenkinsMsgQueue, &msg, 100))
        {
            switch (msg.type)
            {
                case JENKINS_MSG_TYPE_INFO:
                    sJenkinsStoreInfo(&msg.info);
                    doUpdate = true;
                    break;
                case JENKINS_MSG_TYPE_CLEAR_ALL:
                    sJenkinsClearAll();
                    doUpdate = true;
                    break;
                case JENKINS_MSG_TYPE_UNKNOWN_ALL:
                    sJenkinsUnknownAll();
                    doUpdate = true;
                    break;
            }
        }

        if (doUpdate)
        {
            sJenkinsUpdate();
            doUpdate = false;
        }
    }
}


/* ***** init and monitoring stuff ************************************************************** */

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
        const char *stateStr  = sJenkinsStateToStr(pkInfo->state);
        const char *resultStr = sJenkinsResultToStr(pkInfo->result);
        const char stateChar  = pkInfo->state  == JENKINS_STATE_UNKNOWN  ? '?' : stateStr[0];
        const char resultChar = pkInfo->result == JENKINS_RESULT_UNKNOWN ? '?' : resultStr[0];
        const int n = snprintf(pStr, len, " %02i%c%c%c", ix, pkInfo->active ? '=' : '-',
            toupper((int)stateChar), pkInfo->state > JENKINS_STATE_OFF ? toupper((int)resultChar) : resultChar);
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
    DEBUG("mon: jenkins: worst=%s active=%s",
        sJenkinsResultToStr(sJenkinsWorstResult), sJenkinsStateToStr(sJenkinsActiveState));
}

#define JENKINS_MSG_QUEUE_LEN 5

void jenkinsInit(void)
{
    DEBUG("jenkins: init");

    static StaticQueue_t sQueue;
    static uint8_t sQueueBuf[sizeof(JENKINS_MSG_t) * JENKINS_MSG_QUEUE_LEN];
    sJenkinsMsgQueue = xQueueCreateStatic(JENKINS_MSG_QUEUE_LEN, sizeof(JENKINS_MSG_t), sQueueBuf, &sQueue);
    sJenkinsClearAll();
}

void jenkinsStart(void)
{
    DEBUG("jenkins: start");

    static StackType_t sJenkinsTaskStack[512];
    static StaticTask_t sJenkinsTaskTCB;
    xTaskCreateStatic(sJenkinsTask, "ff_jenkins", NUMOF(sJenkinsTaskStack), NULL, 2, sJenkinsTaskStack, &sJenkinsTaskTCB);
}

// eof
