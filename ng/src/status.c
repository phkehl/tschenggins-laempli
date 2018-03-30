/*!
    \file
    \brief flipflip's Tschenggins Lämpli: system status (noises and LED) (see \ref FF_STATUS)

    - Copyright (c) 2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli
*/

#include "stdinc.h"

#include <esp8266.h>

#include "debug.h"
#include "stuff.h"
#include "tone.h"
#include "status.h"

#define STATUS_GPIO 2

static uint8_t sPeriod;
static uint8_t sNum;

static void sStatusLedTimerFunc(TimerHandle_t timer)
{
    static uint32_t tick = 0;

    if (sPeriod && sNum)
    {
        const uint8_t phase = tick % sPeriod;
        if ( phase < (2 * sNum) )
        {
            gpio_write(STATUS_GPIO, (phase % 2) == 0 ? false : true);
        }
    }
    tick++;
}

void statusSetLed(const STATUS_LED_t status)
{
    gpio_write(STATUS_GPIO, false);
    switch (status)
    {
        case STATUS_LED_NONE:
            DEBUG("statusSetLed() none");
            break;
        case STATUS_LED_HEARTBEAT:
            DEBUG("statusSetLed() heartbeat");
            sPeriod = 20;
            sNum    = 2;
            break;
        case STATUS_LED_OFFLINE:
            DEBUG("statusSetLed() offline");
            sPeriod = 20;
            sNum    = 1;
            break;
        case STATUS_LED_FAIL:
            DEBUG("statusSetLed() fail");
            sPeriod = 20;
            sNum    = 5;
            break;
        case STATUS_LED_UPDATE:
            DEBUG("statusSetLed() update");
            sPeriod = 2;
            sNum    = 1;
            break;
    }
}


void statusMakeNoise(const STATUS_NOISE_t noise)
{
    // FIXME: not if beNoisy=false

    if (toneIsPlaying())
    {
        return;
    }

    switch (noise)
    {
        case STATUS_NOISE_ABORT:
        {
            static const int16_t skNoiseAbort[] =
            {
                TONE(A5, 30), TONE(PAUSE, 20), TONE(G5, 60), TONE_END
            };
            toneMelody(skNoiseAbort);
            break;
        }
        case STATUS_NOISE_FAIL:
        {
            static const int16_t skNoiseFail[] =
            {
                TONE(A5, 30), TONE(PAUSE, 20), TONE(G5, 60), TONE(PAUSE, 20), TONE(F5, 100), TONE_END
            };
            toneMelody(skNoiseFail);
            break;
        }
        case STATUS_NOISE_ONLINE:
        {
            static const int16_t skNoiseOnline[] =
            {
                TONE(D6, 30), TONE(PAUSE, 20), TONE(E6, 60), TONE_END
            };
            toneMelody(skNoiseOnline);
            break;
        }
        case STATUS_NOISE_OTHER:
        {
            static const int16_t skNoiseOther[] =
            {
                TONE(C6, 30), TONE_END
            };
            toneMelody(skNoiseOther);
            break;
        }
        case STATUS_NOISE_ERROR:
        {
            static const int16_t skNoiseError[] =
            {
                TONE(C4, 200), TONE(PAUSE, 50), TONE(C4, 200), TONE_END
            };
            toneMelody(skNoiseError);
            break;
        }
    }
}

void statusInit(void)
{
    DEBUG("statusInit()");

    gpio_enable(STATUS_GPIO, GPIO_OUTPUT);
    gpio_write(STATUS_GPIO, true); // off, LED logic is inverted

    // setup LED timer
    TimerHandle_t timer = xTimerCreate("status_led", MS2TICKS(100), true, NULL, sStatusLedTimerFunc);
    if ( (timer == NULL) || (xTimerStart(timer, 1000) != pdPASS) )
    {
        ERROR("status: timer");
    }

    statusSetLed(STATUS_LED_NONE);
}

// eof
