/*!
    \file
    \brief flipflip's Tschenggins Lämpli: system status (noises and LED) (see \ref FF_STATUS)

    - Copyright (c) 2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

    \defgroup FF_STATUS STATUS
    \ingroup FF

    @{
*/
#ifndef __STATUS_H__
#define __STATUS_H__

#include "stdinc.h"

//! initialise
void statusInit(void);

typedef enum STATUS_NOISE_e
{
    STATUS_NOISE_ABORT,
    STATUS_NOISE_FAIL,
    STATUS_NOISE_ONLINE,
    STATUS_NOISE_OTHER
} STATUS_NOISE_t;

void statusMakeNoise(const STATUS_NOISE_t noise);

#endif // __STATUS_H__
