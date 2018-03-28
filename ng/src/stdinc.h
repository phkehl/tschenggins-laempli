/*!
    \file
    \brief flipflip's Tschenggins Lämpli: standard includes

    - Copyright (c) 2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli
*/
#ifndef __STDINC_H__
#define __STDINC_H__

#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <stdio.h>
#include <malloc.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <unistd.h>

#include <espressif/user_interface.h>
#include <espressif/esp_common.h>

#include <FreeRTOS.h>
#include <task.h>
#include <queue.h>



// not defined in espressif/esp_wifi.h
extern bool sdk_wifi_set_opmode_current(uint8_t opmode);

#endif // __STDINC_H__
