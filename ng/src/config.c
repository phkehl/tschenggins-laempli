/*!
    \file
    \brief flipflip's Tschenggins Lämpli: system config (noises and LED) (see \ref FF_CONFIG)

    - Copyright (c) 2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli
*/

#include "stdinc.h"

#include <jsmn.h>

#include "debug.h"
#include "status.h"
#include "config.h"
#include "cfg_gen.h"

void configInit(void)
{
    DEBUG("configInit()");
}

void configParseJson(char *resp, const int len)
{
    DEBUG("config: [%d] %s", len, resp);
    statusMakeNoise(STATUS_NOISE_ERROR);
}


// eof
