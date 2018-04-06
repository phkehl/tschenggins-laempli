// FreeRTOS config overrides (for settings in FreeRTOS/Source/include/FreeRTOSConfig.h)

#define configUSE_PREEMPTION                       1
#define configTICK_RATE_HZ                         1000
#define configUSE_TRACE_FACILITY                   1
#define configMAX_TASK_NAME_LEN                    16
#define configGENERATE_RUN_TIME_STATS              1
#define configMINIMAL_STACK_SIZE                   256
//#define configUSE_IDLE_HOOK                      1
#define configSUPPORT_STATIC_ALLOCATION            1
//#define configTIMER_QUEUE_LENGTH                 3
#define portCONFIGURE_TIMER_FOR_RUN_TIME_STATS()   /* nothing */
#define portGET_RUN_TIME_COUNTER_VALUE()           RTC.COUNTER
//#define configUSE_STATS_FORMATTING_FUNCTIONS     1
//#define configMINIMAL_STACK_SIZE                 128


#include_next<FreeRTOSConfig.h>
