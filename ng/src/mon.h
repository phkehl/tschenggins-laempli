/*!
    \file
    \brief flipflip's Tschenggins Lämpli: system monitor (see \ref FF_MON)

    - Copyright (c) 2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

    \defgroup FF_MON MON
    \ingroup FF

    @{
*/
#ifndef __MON_H__
#define __MON_H__

#include "stdinc.h"

//! initialise system monitor
void monInit(void);

void monIsrEnter(void);
void monIsrLeave(void);


#endif // __MON_H__
