/*
TODO:
- setup cscope
- buffered debug tx
- debug macros
- debug.pl
- monitor task
- make flash && make debug

- uart.h, uart_regs.h

- cflags for our files only? -Wunused etc.

- FreeRTOS API reference https://www.freertos.org/a00106.html



*/

// standard library (libc/xtensa-lx106-elf/include/)
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>

// ESP SDK (include/)
#include <espressif/esp_common.h>

// FreeRTOS (FreeRTOS/Source/include/)
#include <FreeRTOS.h>
#include <task.h>

// open RTOS (core/include/)
#include <common_macros.h>
#include <esp/registers.h>
#include <esp/interrupts.h>
#include <esp/iomux.h>
#include <esp/gpio.h>
#include <esp/timer.h>

#include "stuff.h"
#include "debug.h"


const int gpio = 2;

/* This task uses the high level GPIO API (esp_gpio.h) to blink an LED.
 *
 */
void blinkenTask(void *pvParameters)
{
    gpio_enable(gpio, GPIO_OUTPUT);
    while(1) {
        gpio_write(gpio, 1);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        gpio_write(gpio, 0);
        vTaskDelay(1000 / portTICK_PERIOD_MS);
        PRINT("blink %u", portTICK_PERIOD_MS);
    }
}

void blaTask(void *pvParameters)
{
    static int count;
    while(1) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        DEBUG("bla %i", count++);
    }
}



void user_init(void)
{
    debugInit();
    stuffInit();

    xTaskCreate(blinkenTask, "blinkenTask", 256, NULL, 2, NULL);
    xTaskCreate(blaTask, "blaTask", 256, NULL, 2, NULL);
    //xTaskCreate(blinkenRegisterTask, "blinkenRegisterTask", 256, NULL, 2, NULL);
    NOTICE("here we go...");

}
