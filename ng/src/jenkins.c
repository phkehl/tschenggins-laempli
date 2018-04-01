/*!
    \file
    \brief flipflip's Tschenggins Lämpli: Jenkins (see \ref FF_JENKINS)

    - Copyright (c) 2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli
*/

#include "stdinc.h"

#include "debug.h"
#include "stuff.h"
#include "jenkins.h"

static const char * const skJenkinsStateStrs[] =
{
    [JENKINS_STATE_UNKNOWN] = "unknown", [JENKINS_STATE_OFF] = "off",
    [JENKINS_STATE_RUNNING] = "running", [JENKINS_STATE_IDLE] = "idle",
};

static const char * const  skJenkinsResultStrs[] =
{
    [JENKINS_RESULT_UNKNOWN] = "unknown", [JENKINS_RESULT_OFF] = "off",
    [JENKINS_RESULT_SUCCESS] = "success", [JENKINS_RESULT_UNSTABLE] = "unstable",
    [JENKINS_RESULT_FAILURE] "failure",
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
    else if (strcmp(str, skJenkinsResultStrs[JENKINS_RESULT_OFF]) == 0)
    {
        result = JENKINS_RESULT_OFF;
    }
    return result;
}

const char *jenkinsStateToStr(const JENKINS_STATE_t state)
{
    switch (state)
    {
        case JENKINS_STATE_OFF:     return "off";
        case JENKINS_STATE_UNKNOWN: return "unknown";
        case JENKINS_STATE_RUNNING: return "running";
        case JENKINS_STATE_IDLE:    return "idle";
    }
    return "???";
}

const char *jenkinsResultToStr(const JENKINS_RESULT_t result)
{
    switch (result)
    {
        case JENKINS_RESULT_OFF:      return "off";
        case JENKINS_RESULT_UNKNOWN:  return "unknown";
        case JENKINS_RESULT_SUCCESS:  return "success";
        case JENKINS_RESULT_UNSTABLE: return "unstable";
        case JENKINS_RESULT_FAILURE:  return "failure";
    }
    return "???";
}

static QueueHandle_t sJenkinsInfoQueue;

bool jenkinsAddInfo(const JENKINS_INFO_t *pkInfo)
{
    if (sJenkinsInfoQueue == NULL)
    {
        ERROR("jenkins: no queue");
        return false;
    }
    if (xQueueSend(sJenkinsInfoQueue, pkInfo, 0) == pdTRUE)
    {
        return true;
    }
    else
    {
        WARNING("jenkins: queue full");
        return false;
    }
}

static void sJenkinsTask(void *pArg)
{
    while (true)
    {
        JENKINS_INFO_t jInfo;
        if (xQueueReceive(sJenkinsInfoQueue, &jInfo, 5000))
        {
            if (jInfo.active)
            {
                const char *state  = jenkinsStateToStr(jInfo.state);
                const char *result = jenkinsResultToStr(jInfo.result);
                const uint32_t now = osGetPosixTime();
                const uint32_t age = now - jInfo.time;
                PRINT("jenkins: info: #%02d %-"STRINGIFY(JENKINS_JOBNAME_LEN)"s %-"STRINGIFY(JENKINS_SERVER_LEN)"s %-7s %-8s %6.1fh",
                    jInfo.chIx, jInfo.job, jInfo.server, state, result, (double)age / 3600.0);
            }
            else
            {
                PRINT("jenkins: info: #%02d <inactive>", jInfo.chIx);
            }
        }
        else
        {
            DEBUG("jenkins: no news");
        }
    }
}


void jenkinsInit(void)
{
    DEBUG("jenkinsInit()");

    sJenkinsInfoQueue = xQueueCreate(JENKINS_MAX_CH, sizeof(JENKINS_INFO_t));
    if (sJenkinsInfoQueue == NULL)
    {
        ERROR("jenkins: create queue");
    }

    xTaskCreate(sJenkinsTask, "ff_jenkins", 512, NULL, 2, NULL);
}

// eof
