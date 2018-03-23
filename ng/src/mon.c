/*!
    \file
    \brief flipflip's Tschenggins Lämpli: system monitor (see \ref FF_MON)

    - Copyright (c) 2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli
*/

#include <malloc.h>
#include <string.h>
#include <stdlib.h>

#include <FreeRTOS.h>
#include <task.h>

#include <esp/rtc_regs.h>

#include "debug.h"
#include "stuff.h"
#include "mon.h"


#define MON_PERIOD 2500
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

    while (true)
    {
        if (pTasks != NULL)
        {
            free(pTasks);
        }

        // wait until it's time to dump the status
        static uint32_t sTick;
        vTaskDelayUntil(&sTick, MS2TICK(MON_PERIOD));
        PRINT("mon: msss=%u heap=%u", TICKS2MS(sTick), xPortGetFreeHeapSize());

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

        // print ISR load (as far as we know)
        PRINT("mon: isr=%u (%.2fkHz, %.1f%%)", isrCount,
            (double)isrCount / ((double)MON_PERIOD / 1000.0) / 1000.0,
            (double)isrTime * 100.0 / (double)isrTotalRuntime);

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
            PRINT("mon: %02d %-16s %c %2i-%2i %4u %5.1f%%",
                (int)pkTask->xTaskNumber, pkTask->pcTaskName, state,
                (int)pkTask->uxCurrentPriority, (int)pkTask->uxBasePriority,
                pkTask->usStackHighWaterMark,
                (double)pkTask->ulRunTimeCounter * 100.0 / (double)totalRuntimeTasks);
        }
        //PRINT("runtime: %u %u %u, %u", totalRuntime, totalRuntimeTasks, totalRuntime - totalRuntimeTasks, isrTotalRuntime);
    }
}

void monInit(void)
{
    DEBUG("monInit()");

    xTaskCreate(sMonTask, "mon", 320, NULL, 5, NULL);

}

// eof
