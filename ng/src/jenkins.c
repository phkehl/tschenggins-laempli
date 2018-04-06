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

static JENKINS_STATE_t  sJenkinsStates[JENKINS_MAX_CH];
static JENKINS_RESULT_t sJenkinsResults[JENKINS_MAX_CH];
static JENKINS_RESULT_t sJenkinsWorstResult;

void jenkinsSetInfo(const JENKINS_INFO_t *pkInfo)
{
    // update LEDs
    if (pkInfo->active)
    {
        const char *state  = jenkinsStateToStr(pkInfo->state);
        const char *result = jenkinsResultToStr(pkInfo->result);
        const uint32_t now = osGetPosixTime();
        const uint32_t age = now - pkInfo->time;
        PRINT("jenkins: info: #%02d %-"STRINGIFY(JENKINS_JOBNAME_LEN)"s %-"STRINGIFY(JENKINS_SERVER_LEN)"s %-7s %-8s %6.1fh",
            pkInfo->chIx, pkInfo->job, pkInfo->server, state, result, (double)age / 3600.0);
        ledsSetState(pkInfo->chIx, sJenkinsLedStateFromJenkins(pkInfo->state, pkInfo->result));
    }
    else
    {
        PRINT("jenkins: info: #%02d <unused>", pkInfo->chIx);
        ledsSetState(pkInfo->chIx, sJenkinsLedStateFromJenkins(JENKINS_STATE_UNKNOWN, JENKINS_RESULT_UNKNOWN));
    }

    // store new result and state
    if (pkInfo->chIx < NUMOF(sJenkinsStates))
    {
        sJenkinsStates[pkInfo->chIx]  = pkInfo->active ? pkInfo->state  : JENKINS_STATE_UNKNOWN;
        sJenkinsResults[pkInfo->chIx] = pkInfo->active ? pkInfo->result : JENKINS_RESULT_UNKNOWN;
    }

    // find worst result
    JENKINS_RESULT_t worstResult = JENKINS_RESULT_UNKNOWN;
    for (int ix = 0; ix < NUMOF(sJenkinsResults); ix++)
    {
        if (sJenkinsResults[ix] >= worstResult)
        {
            worstResult = sJenkinsResults[ix];
        }
    }
    DEBUG("jenkins: worst is now %s", jenkinsResultToStr(worstResult));

    // play sound if we changed from failure/warning to success or from success/warning to failure
    if (worstResult != sJenkinsWorstResult)
    {
        switch (worstResult)
        {
            case JENKINS_RESULT_FAILURE:
                PRINT("jenkins: failure!");
                break;
            case JENKINS_RESULT_SUCCESS:
                PRINT("jenkins: success!");
                break;
            case JENKINS_RESULT_UNSTABLE:
                PRINT("jenkins: unstable!");
                break;
            default:
                break;
        }
    }

    sJenkinsWorstResult = worstResult;
}

void jenkinsClearInfo(void)
{
    for (uint16_t ix = 0; ix < JENKINS_MAX_CH; ix++)
    {
        JENKINS_INFO_t info = { .chIx = ix, .active = false };
        jenkinsSetInfo(&info);
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
        const char *stateStr  = jenkinsStateToStr(sJenkinsStates[ix]);
        const char *resultStr = jenkinsResultToStr(sJenkinsResults[ix]);
        const char stateChar  = sJenkinsStates[ix ] == JENKINS_STATE_UNKNOWN  ? '?' : stateStr[0];
        const char resultChar = sJenkinsResults[ix] == JENKINS_RESULT_UNKNOWN ? '?' : resultStr[0];
        const int n = snprintf(pStr, len, " %02i=%c%c", ix, toupper((int)stateChar), toupper((int)resultChar));
        len -= n;
        pStr += n;
        ix++;
        last = (ix >= NUMOF(sJenkinsStates)) || (len < 1);
        if ( ((ix % 10) == 0) || last )
        {
            DEBUG("mon: jenkins:%s", str);
            pStr = str;
            len = sizeof(str) - 1;
        }
    }
    DEBUG("mon: jenkins: worst=%s", jenkinsResultToStr(sJenkinsWorstResult));
}


void jenkinsInit(void)
{
    DEBUG("jenkins: init");

}


// eof
