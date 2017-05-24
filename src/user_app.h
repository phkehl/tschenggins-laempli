/*!
    \file
    \brief flipflip's Tschenggins Lämpli: main application (see \ref USER_APP)

    - Copyright (c) 2017 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

    \defgroup USER_APP APP
    \ingroup USER

    This implements the main application (Jenkinst status display etc.).

    Configuration:
    - #USER_APP_NUM_LEDS
    - #USER_APP_LED_MAP

    @{
*/
#ifndef __USER_APP_H__
#define __USER_APP_H__

#include "user_stuff.h"
#include "version_gen.h"

//! initialise application
void appInit(void);

//! start the application
void appStart(void);

//! print the application status
void appStatus(void);

//! force update (force reconnect to status server)
bool appForceUpdate(void);

#endif // __USER_APP_H__
//@}
// eof
