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

#include "stuff.h"


static int ucount;

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
        printf("blink %u %i\n", portTICK_PERIOD_MS, ucount);
    }
}

void blaTask(void *pvParameters)
{
    static int count;
    while(1) {
        vTaskDelay(100 / portTICK_PERIOD_MS);
        printf("blaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa %i\n", count++);
    }
}



void user_init(void)
{
    stuffInit();

    xTaskCreate(blinkenTask, "blinkenTask", 256, NULL, 2, NULL);
    xTaskCreate(blaTask, "blaTask", 256, NULL, 2, NULL);
    //xTaskCreate(blinkenRegisterTask, "blinkenRegisterTask", 256, NULL, 2, NULL);
    printf("here we go...\n");


    printf("here we go again...\n");
}
