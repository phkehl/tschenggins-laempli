/*!
    \file
    \brief flipflip's Tschenggins Lämpli: compile-time software configuration (see \ref USER_CONFIG)

    - Copyright (c) 2017 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

    \defgroup USER_CONFIG CONFIG
    \ingroup USER

    @{
*/
#ifndef __USER_CONFIG_H__
#define __USER_CONFIG_H__

#include "version_gen.h"
#include "cfg_gen.h"

//---------------------------------------------------------------------------------------------------

//#define MEMLEAK_DEBUG

//---------------------------------------------------------------------------------------------------

// note: some configuration is in the Makefile

//---------------------------------------------------------------------------------------------------

#ifdef __DOXYGEN__
//! the number of LEDs
#  define USER_APP_NUM_LEDS 8
//! the arrangement of the LEDs (index of the LED in the box -> index of the LED on the chain)
#  define USER_APP_LED_MAP { 0, 1, 2, 3, 4, 5, 6, 7 }
#endif


// ----- model 1 - the original, with Chewie (s/n 0001) -----
#if (FF_MODEL == 1)
#  define USER_APP_NUM_LEDS 8
#  define USER_APP_LED_MAP { 7, 0, 6, 1, 5, 2, 4, 3 } // box setup vertically ("portrait")

// ----- model 2 - given to A. (s/n 0002) -----
#elif (FF_MODEL == 2)
#  define USER_APP_NUM_LEDS 3
#  define USER_APP_LED_MAP { 0, 1, 2 }

// ----- model 3 - the tiny one (s/n 0003) -----
#elif (FF_MODEL == 3)
#  define USER_APP_NUM_LEDS 3
#  define USER_APP_LED_MAP { 2, 1, 0 }
#  define USER_WS2801_ORDER 123

// ----- model 4 - ali style, given to colleagues (s/n 0004, 0005, 0006, 0007, 0008) -----
#elif (FF_MODEL == 4)
#  define USER_APP_NUM_LEDS 2
#  define USER_APP_LED_MAP { 0, 1 }

#endif

//---------------------------------------------------------------------------------------------------

//! number of LEDs to drive
#define USER_WS2801_NUMLEDS 8

//---------------------------------------------------------------------------------------------------

//! HSV2RGB conversion method to use
/*!
    - 1 = classic (à la wikipedia, linear ramps), optimized
    - 2 = using saturation/value dimming curve to to make it look more natural

    See user_hsv2rgb.c for references & credits.
*/
#define USER_HSV2RGB_METHOD 2

#ifndef USER_WS2801_ORDER
//! LED colour ordering (123 = RGB, 213 = GRB, 231 = GBR, etc.)
#  define USER_WS2801_ORDER 231
#endif

//---------------------------------------------------------------------------------------------------

//! debug tx buffer size,set to 0 for blocking output (besides the 128 bytes UART tx FIFO), set to something resonably high otherwise (recommended)
#define USER_DEBUG_TXBUFSIZE 4096 // 1536

//! UART peripheral to use, 0 or 1
#define USER_DEBUG_UART      0

//! use interrupt (recommended) instead of timer for buffered tx (only if #USER_DEBUG_TXBUFSIZE > 0)
#define USER_DEBUG_USE_ISR   1

//---------------------------------------------------------------------------------------------------

//! enable access point
#define USER_WIFI_USE_AP 1

//---------------------------------------------------------------------------------------------------

//! maximum number of redirects to follow (3xx status), set to 0 to disable
#define USER_WGET_MAX_REDIRECTS 3

//! default timeout
#define USER_WGET_DEFAULT_TIMEOUT 10000

//---------------------------------------------------------------------------------------------------

//! the maximum number of GET/POST parameters that are handled
#define USER_HTTP_NUMPARAM 25

//! maximum number of http server paths that can be registered
#define USER_HTTPD_REQUESTCB_NUM 20

//! maximum number of connections we can maintain in parallel
#define USER_HTTPD_CONN_NUM 10

//! maximum username length
#define USER_HTTPD_USER_LEN_MAX 16

//! maximum password length
#define USER_HTTPD_PASS_LEN_MAX 16

//---------------------------------------------------------------------------------------------------

//! how often to DEBUG() system information
#define USER_MAIN_MON_PERIOD 5000

//---------------------------------------------------------------------------------------------------

//! maximum length of tschenggins-status.pl?cmd=list respons we'll handle
#define USER_CFG_LIST_RESP_MAX_LEN 4096

//---------------------------------------------------------------------------------------------------



#endif // __USER_CONFIG_H__

/* *********************************************************************************************** */
//@}
// eof
