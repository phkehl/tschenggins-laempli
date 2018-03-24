/*!
    \file
    \brief flipflip's Tschenggins Lämpli: handy stuff (see \ref FF_STUFF)

    - Copyright (c) 2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

*/

#include "debug.h"
#include "stuff.h"

const char *sdkAuthModeStr(const AUTH_MODE authmode)
{
    switch (authmode)
    {
        case AUTH_OPEN:         return "open";
        case AUTH_WEP:          return "WEP";
        case AUTH_WPA_PSK:      return "WPA/PSK";
        case AUTH_WPA2_PSK:     return "WPA2/PSK";
        case AUTH_WPA_WPA2_PSK: return "WPA/WPA2/PSK";
        default:                return "???";
    }
}



void stuffInit(void)
{
    DEBUG("stuffInit()");
}


// eof
