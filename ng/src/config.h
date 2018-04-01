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

void configParseJson(char *resp, const int len);


#endif // __CONFIG_H__
