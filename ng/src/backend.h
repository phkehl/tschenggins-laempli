/*!
    \file
    \brief flipflip's Tschenggins Lämpli: backend data handling (see \ref FF_BACKEND)

    - Copyright (c) 2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

    \defgroup FF_BACKEND BACKEND
    \ingroup FF

    @{
*/
#ifndef __BACKEND_H__
#define __BACKEND_H__

#include "stdinc.h"

//! initialise
void backendInit(void);


bool backendConnect(char *resp, const int len);
bool backendHandle(char *resp, const int len);
void backendDisconnect(void);

void backendMonStatus(void);

#endif // __BACKEND_H__
