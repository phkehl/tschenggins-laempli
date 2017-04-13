// by Philippe Kehl <flipflip at oinkzwurgl dot org>

#include "user_stuff.h"
#include "user_status.h"

static os_timer_t sLedTimer;

static USER_STATUS_t sStatus;
static uint8_t sPeriod;
static uint8_t sNum;

static void ICACHE_FLASH_ATTR sLedTimerFunc(void *arg)
{
    (void)arg;
    static uint32_t tick = 0;

    if (sPeriod && sNum)
    {
        const uint8_t phase = tick % sPeriod;

        if ( phase < (2 * sNum) )
        {
            // on
            if ( (phase % 2) == 0 )
            {
                GPIO_OUT_CLR(PIN_D4); // LED logic inverted
                //GPIO_OUT_SET(PIN_D1);
            }
            // off
            else
            {
                GPIO_OUT_SET(PIN_D4); // LED logic inverted
                //GPIO_OUT_CLR(PIN_D1);
            }
        }
    }
    tick++;
}


void ICACHE_FLASH_ATTR statusInit(void)
{
    DEBUG("status: init");

    // select GPIO mode and set GPIO2 (=NodeMCU's built-in LED) to output mode (by not disabling the output)
    GPIO_ENA_PIN_D4();
    GPIO_DIR_SET(PIN_D4);
    GPIO_OUT_SET(PIN_D4); // LED logic is inverted

    // and an additional LED just for fun..
    //GPIO_ENA_PIN_D1();
    //GPIO_DIR_SET(PIN_D1);
    //GPIO_OUT_CLR(PIN_D1);

    // initial LED state always off
    statusSet(USER_STATUS_NONE);

    // setup LED timer
    os_timer_disarm(&sLedTimer);
    os_timer_setfn(&sLedTimer, (os_timer_func_t *)sLedTimerFunc, NULL);
    os_timer_arm(&sLedTimer, 100, 1); // 100ms interval, repeated
}



void ICACHE_FLASH_ATTR statusSet(const USER_STATUS_t status)
{
    if (status == sStatus)
    {
        return;
    }

    // LEDs off
    GPIO_OUT_SET(PIN_D4); // inverted
    //GPIO_OUT_CLR(PIN_D1);

    switch (status)
    {
        case USER_STATUS_NONE:
            DEBUG("statusSet() none");
            break;
        case USER_STATUS_HEARTBEAT:
            DEBUG("statusSet() heartbeat");
            sPeriod = 20;
            sNum    = 2;
            break;
        case USER_STATUS_OFFLINE:
            DEBUG("statusSet() offline");
            sPeriod = 20;
            sNum    = 1;
            break;
        case USER_STATUS_FAIL:
            DEBUG("statusSet() fail");
            sPeriod = 20;
            sNum    = 5;
            break;
        case USER_STATUS_UPDATE:
            DEBUG("statusSet() update");
            sPeriod = 2;
            sNum    = 1;
            break;
    }

    //if ( (2 * sNum) > sPeriod )
    //{
    //    sPeriod = 2 * sNum;
    //}

    sStatus = status;
}

// eof
