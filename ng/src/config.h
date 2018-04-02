/*!
    \file
    \brief flipflip's Tschenggins Lämpli: system config (see \ref FF_CONFIG)

    - Copyright (c) 2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

    \defgroup FF_CONFIG CONFIG
    \ingroup FF

    @{
*/
#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "stdinc.h"

//! initialise
void configInit(void);

void configMonStatus(void);

typedef enum CONFIG_MODEL_e
{
    CONFIG_MODEL_UNKNOWN,
    CONFIG_MODEL_STANDARD,
    CONFIG_MODEL_HELLO,
} CONFIG_MODEL_t;

typedef enum CONFIG_DRIVER_e
{
    CONFIG_DRIVER_UNKNOWN,
    CONFIG_DRIVER_WS2801,
    //CONFIG_DRIVER_WS2812,
    CONFIG_DRIVER_SK9822,
} CONFIG_DRIVER_t;

typedef enum CONFIG_ORDER_e
{
    CONFIG_ORDER_UNKNOWN,
    CONFIG_ORDER_RGB,
    CONFIG_ORDER_RBG,
    CONFIG_ORDER_GRB,
    CONFIG_ORDER_GBR,
    CONFIG_ORDER_BRG,
    CONFIG_ORDER_BGR,
} CONFIG_ORDER_t;

typedef enum CONFIG_BRIGHT_e
{
    CONFIG_BRIGHT_UNKNOWN,
    CONFIG_BRIGHT_LOW,
    CONFIG_BRIGHT_MEDIUM,
    CONFIG_BRIGHT_HIGH,
    CONFIG_BRIGHT_FULL,
} CONFIG_BRIGHT_t;

typedef enum CONFIG_NOISE_e
{
    CONFIG_NOISE_UNKNOWN,
    CONFIG_NOISE_NONE,
    CONFIG_NOISE_SOME,
    CONFIG_NOISE_MORE,
} CONFIG_NOISE_t;

CONFIG_MODEL_t  configGetModel(void);
CONFIG_DRIVER_t configGetDriver(void);
CONFIG_ORDER_t  configGetOrder(void);
CONFIG_BRIGHT_t configGetBright(void);
CONFIG_NOISE_t  configGetNoise(void);

bool configParseJson(char *resp, const int respLen);


#endif // __CONFIG_H__
