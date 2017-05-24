/*!
    \file
    \brief flipflip's Tschenggins Lämpli: xxx (see \ref USER_STATUS)

    - Copyright (c) 2017 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

    \defgroup USER_STATUS STATUS
    \ingroup USER

    This implements a status display on (a LED on) a GPIO with different blink patterns. Currently
    the (NodeMCU's) built-in LED on GPIO2/D2 is hard-coded.

    @{
*/
#ifndef __USER_STATUS_H__
#define __USER_STATUS_H__

#include "user_stuff.h"

// status LED (NodeMCU's built-in LED)
// uses the same LED as the wifi module, see wifi_status_led_install() in wifiStart()


//! initialise status display
void statusInit(void);

//! status display modes
typedef enum USER_STATUS_e
{
    USER_STATUS_NONE,       //!< LED off
    USER_STATUS_HEARTBEAT,  //!< LED doing a heart beat style blinking (2 seconds period)
    USER_STATUS_OFFLINE,    //!< one blink every two seconds
    USER_STATUS_FAIL,       //!< bursts of five fast blinks every two seconds
    USER_STATUS_UPDATE,     //!< fast blinking while things are updating (or so)

} USER_STATUS_t;


//! set status LED blink pattern
/*!
    \param[in] status  display mode
*/
void statusSet(const USER_STATUS_t status);


#endif // __USER_STATUS_H__
//@}
// eof
