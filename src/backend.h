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

typedef enum BACKEND_STATUS_e
{
    BACKEND_STATUS_OKAY,
    BACKEND_STATUS_RECONNECT,
    BACKEND_STATUS_FAIL,

} BACKEND_STATUS_t;

#define BACKEND_STABLE_CONN_THRS  300 // [s]
#define BACKEND_RECONNECT_INTERVAL 10 // [s]
#define BACKEND_RECONNECT_INTERVAL_SLOW 300 // [s]
#if (BACKEND_RECONNECT_INTERVAL_SLOW <= BACKEND_RECONNECT_INTERVAL)
#  error Nope!
#endif

bool backendConnect(char *resp, const int len);

BACKEND_STATUS_t backendHandle(char *resp, const int len);

bool backendIsOkay(void);
void backendDisconnect(void);

void backendMonStatus(void);

#endif // __BACKEND_H__
//@}
// eof
