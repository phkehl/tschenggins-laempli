/*!
    \file
    \brief flipflip's Tschenggins Lämpli: LEDS WS2801 and SK9822 LED driver (see \ref FF_LEDS)

    - Copyright (c) 2017-2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

    \defgroup FF_LEDS LEDS
    \ingroup FF

    @{
*/
#ifndef __LEDS_H__
#define __LEDS_H__

#include "stdinc.h"

//! initialise
void ledsInit(void);

//! start
void ledsStart(void);

typedef enum LEDS_FX_e
{
    LEDS_FX_STILL,
    LEDS_FX_PULSE,
    LEDS_FX_FLICKER,
    LEDS_FX_BLINK,

} LEDS_FX_t;


typedef struct LEDS_PARAM_s
{
    // base colour
    uint8_t hue;
    uint8_t sat;
    uint8_t val;

    // effect
    LEDS_FX_t fx;
    int       arg;

} LEDS_PARAM_t;

#define LEDS_MAKE_PARAM(_hue, _sat, _val, _fx, _arg) \
    { .hue = (_hue), .sat = (_sat), .val = (_val), .fx = CONCAT(LEDS_FX_, _fx), .arg = (_arg) }


void ledsSetState(const uint16_t ledIx, const LEDS_PARAM_t *pkParam);

void ledsSetStateHello(const LEDS_PARAM_t *pkParamHead, const LEDS_PARAM_t *pkParamBow);

#endif // __LEDS_H__
//@}
// eof
