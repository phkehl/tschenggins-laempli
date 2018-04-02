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


typedef struct LEDS_STATE_s
{
    uint8_t hue;
    uint8_t sat;
    uint8_t val;

    int8_t  dVal;
    uint8_t minVal;
    uint8_t maxVal;
} LEDS_STATE_t;

void ledsSetState(const uint16_t ledIx, const LEDS_STATE_t *pkState);


#endif // __LEDS_H__
//@}
// eof
