/*!
    \file
    \brief flipflip's Tschenggins Lämpli: system monitor (see \ref FF_MON)

    - Copyright (c) 2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli
*/

#include "stdinc.h"
#include <esp/rtc_regs.h>

#include "debug.h"
#include "stuff.h"
#include "wifi.h"
#include "backend.h"
#include "config.h"
#include "jenkins.h"
#include "mon.h"


#define MON_PERIOD 5000
#define MON_FIRST 1000
#define MAX_TASKS 10


static volatile uint32_t svMonIsrStart;
static volatile uint32_t svMonIsrCount;
static volatile uint32_t svMonIsrTime;

void monIsrEnter(void)
{
    svMonIsrStart = RTC.COUNTER;
    svMonIsrCount++;
}

void monIsrLeave(void)
{
    svMonIsrTime += RTC.COUNTER - svMonIsrStart;
}

static int sTaskSortFunc(const void *a, const void *b)
{
    return (int)((const TaskStatus_t *)a)->xTaskNumber - (int)((const TaskStatus_t *)b)->xTaskNumber;
}

static void sMonTask(void *pArg)
{
    TaskStatus_t *pTasks = NULL;

    static uint32_t sTick;
    sTick = -MS2TICKS(MON_PERIOD - MON_FIRST);

    while (true)
    {
        if (pTasks != NULL)
        {
            free(pTasks);
        }

        // wait until it's time to dump the status
        vTaskDelayUntil(&sTick, MS2TICKS(MON_PERIOD));

        const int nTasks = uxTaskGetNumberOfTasks();
        if (nTasks > MAX_TASKS)
        {
            ERROR("mon: too many tasks");
            continue;
        }

        // allocate memory for tasks status
        const unsigned int allocSize = nTasks * sizeof(TaskStatus_t);
        pTasks = malloc(allocSize);
        if (pTasks == NULL)
        {
            ERROR("mon: malloc");
            continue;
        }
        memset(pTasks, 0, allocSize);

        // get ISR runtime stats
        uint32_t isrCount, isrTime, isrTotalRuntime;
        static uint32_t sIsrLastRuntime;
        CS_ENTER;
        const uint32_t rtcCounter = RTC.COUNTER;
        isrTotalRuntime = rtcCounter - sIsrLastRuntime;
        sIsrLastRuntime = rtcCounter;
        isrCount = svMonIsrTime;
        isrTime = svMonIsrCount;
        svMonIsrCount = 0;
        svMonIsrTime = 0;
        CS_LEAVE;

        // get tasks info
        uint32_t totalRuntime;
        const int nnTasks = uxTaskGetSystemState(pTasks, nTasks, &totalRuntime);
        if (nTasks != nnTasks)
        {
            ERROR("mon: %u != %u", nTasks, nnTasks);
            continue;
        }

        // sort by task ID
        qsort(pTasks, nTasks, sizeof(TaskStatus_t), sTaskSortFunc);

        // total runtime (tasks, OS, ISRs) since we checked last
        static uint32_t sLastTotalRuntime;
        {
            const uint32_t runtime = totalRuntime;
            totalRuntime = totalRuntime - sLastTotalRuntime;
            sLastTotalRuntime = runtime;
        }

        // calculate time spent in each task since we checked last
        static uint32_t sLastRuntimeCounter[MAX_TASKS];
        uint32_t totalRuntimeTasks = 0;
        for (int ix = 0; ix < nTasks; ix++)
        {
            TaskStatus_t *pTask = &pTasks[ix];
            const uint32_t runtime = pTask->ulRunTimeCounter;
            pTask->ulRunTimeCounter = pTask->ulRunTimeCounter - sLastRuntimeCounter[ix];
            sLastRuntimeCounter[ix] = runtime;
            totalRuntimeTasks += pTask->ulRunTimeCounter;
        }

        // FIXME: why?
        if (totalRuntimeTasks > totalRuntime)
        {
            totalRuntime = totalRuntimeTasks;
        }

        // RTC
        static uint32_t sLastRtc;
        const uint32_t msss = sdk_system_get_time() / 1000; // ms
        const uint32_t thisRtc = sdk_system_get_rtc_time();
        const uint32_t drtc = /*roundl*/( (double)(sLastRtc ? thisRtc - sLastRtc : 0)
            * (1.0/1000.0/4096.0) * sdk_system_rtc_clock_cali_proc() ); // us -> ms
        sLastRtc = thisRtc;

        // print monitor info
        DEBUG("--------------------------------------------------------------------------------");
        DEBUG("mon: sys: ticks=%u msss=%u drtc=%u heap=%u isr=%u (%.2fkHz, %.1f%%) mhz=%u",
            sTick, msss, drtc, /*xPortGetFreeHeapSize(), */sdk_system_get_free_heap_size(),
            isrCount,
            (double)isrCount / ((double)MON_PERIOD / 1000.0) / 1000.0,
            (double)isrTime * 100.0 / (double)isrTotalRuntime, sdk_system_get_cpu_freq());
        debugMonStatus();
        wifiMonStatus();
        backendMonStatus();
        configMonStatus();
        jenkinsMonStatus();

        // print tasks info
        for (int ix = 0; ix < nTasks; ix++)
        {
            const TaskStatus_t *pkTask = &pTasks[ix];
            char state = '?';
            switch (pkTask->eCurrentState)
            {
                case eRunning:   state = 'X'; break;
                case eReady:     state = 'R'; break;
                case eBlocked:   state = 'B'; break;
                case eSuspended: state = 'S'; break;
                case eDeleted:   state = 'D'; break;
                case eInvalid:   state = 'I'; break;
            }
            char perc[8];
            if (pkTask->ulRunTimeCounter)
            {
                const double p = (double)pkTask->ulRunTimeCounter * 100.0 / (double)totalRuntimeTasks;
                if (p < 0.05)
                {
                    strcpy(perc, "<0.1%");
                }
                else
                {
                    snprintf(perc, sizeof(perc), "%5.1f%%", p);
                }
            }
            else
            {
                strcpy(perc, "0.0%");
            }
            DEBUG("mon: tsk: %02d %-16s %c %2i-%2i %4u %6s",
                (int)pkTask->xTaskNumber, pkTask->pcTaskName, state,
                (int)pkTask->uxCurrentPriority, (int)pkTask->uxBasePriority,
                pkTask->usStackHighWaterMark, perc);
        }
        DEBUG("--------------------------------------------------------------------------------");
        //PRINT("runtime: %u %u %u, %u", totalRuntime, totalRuntimeTasks, totalRuntime - totalRuntimeTasks, isrTotalRuntime);
    }
}

void monInit(void)
{
    DEBUG("mon: init");

    static StackType_t sMonTaskStack[384];
    static StaticTask_t sMonTaskTCB;
    xTaskCreateStatic(sMonTask, "ff_mon", NUMOF(sMonTaskStack), NULL, 9, sMonTaskStack, &sMonTaskTCB);
}

// eof
