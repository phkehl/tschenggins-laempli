/*!
    \file
    \brief flipflip's Tschenggins Lämpli: LEDs (see \ref FF_LED)

    - Copyright (c) 2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli
*/

#include <stdbool.h>
#include <stdint.h>

#include <FreeRTOS.h>
#include <task.h>

#include <esp/gpio.h>

#include "debug.h"
#include "stuff.h"
#include "led.h"

#define LED_GPIO 2

static void sLedTask(void *pArg)
{
    while (true)
    {
        PRINT("led: blink...");
        gpio_write(LED_GPIO, 1);
        osSleep(500);
        gpio_write(LED_GPIO, 0);
        osSleep(500);
    }
}

void ledInit(void)
{
    DEBUG("ledInit()");
    gpio_enable(LED_GPIO, GPIO_OUTPUT);

    xTaskCreate(sLedTask, "ff_led", 320, NULL, 2, NULL);
}

// eof
