/*!
    \file
    \brief flipflip's Tschenggins Lämpli: main application (see \ref USER_APP)

    - Copyright (c) 2017 Philippe Kehl <flipflip at oinkzwurgl dot org>,
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

    \defgroup USER_APP APP
    \ingroup USER

    This implements the main application (Jenkinst status display etc.).

    @{
*/
#ifndef __USER_APP_H__
#define __USER_APP_H__

#include "user_stuff.h"
#include "version_gen.h"

#if (FF_MODEL == 1)

//! the number of LEDs in the Tschenggins Lämpli Model 1
#  define APP_NUM_LEDS 8

//! the arrangement of the LEDs (index of the LED in the box -> index of the LED on the chain)
#  define APP_LED_MAP { 7, 0, 6, 1, 5, 2, 4, 3 } // box setup vertically ("portrait")
//#  define APP_LED_MAP { 0, 1, 2, 3, 7, 6, 5, 4 } // box setup horizontally ("landscape")

#elif (FF_MODEL == 2)

//! the number of LEDs in the Tschenggins Lämpli Model 2
#  define APP_NUM_LEDS 3

//! the arrangement of the LEDs (index of the LED in the box -> index of the LED on the chain)
#  define APP_LED_MAP { 0, 1, 2 }

#else
#  warning Please implement your model here.
#endif

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
