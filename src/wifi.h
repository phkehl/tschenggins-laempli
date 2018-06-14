/*!
    \file
    \brief flipflip's Tschenggins Lämpli: wifi and network things (see \ref FF_WIFI)

    - Copyright (c) 2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

    \defgroup FF_WIFI WIFI
    \ingroup FF

    @{
*/
#ifndef __WIFI_H__
#define __WIFI_H__

#include "stdinc.h"

//! initialise
void wifiInit(void);

//! start
void wifiStart(void);

void wifiMonStatus(void);

#endif // __WIFI_H__
