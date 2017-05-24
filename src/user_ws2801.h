/*!
    \file
    \brief flipflip's Tschenggins Lämpli: WS2801 RGB LEDs (see \ref USER_WS2801)

    - Copyright (c) 2017 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

    - Register info (in user_ws2801.c) copyright (c) 2016 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>

    \defgroup USER_WS2801 WS2801
    \ingroup USER

    This implements a driver for a WS2801 RGB LED strip. It uses the HSPI interface (see \c
    README.md for the wiring) in interrupt mode. I.e. the interface is non-blocking and arbitrary
    number of LEDs can be controlled without blocking the system while the data is being
    transferred. Up to 16 words (64 bytes) are loaded to the SPI FIFO in one interrupt.

    Configuration:
    - #USER_WS2801_NUMLEDS
    - #USER_WS2801_ORDER
    - see user_ws2801.c for SPI clock speed and more

    @{
*/
#ifndef __USER_WS2801_H__
#define __USER_WS2801_H__

#include "user_stuff.h"


//! initialise WS2801 LEDs
/*!
    Configuration: #USER_WS2801_NUMLEDS
*/
void ws2801Init(void);

//! clear all LEDs
void ws2801Clear(void);

//! set LED colour given hue, saturation and (brightness) value
/*!
    \param[in] ix  the index of the LED on the strip (0..(#USER_WS2801_NUMLEDS - 1))
    \param[in] H   hue value (0..255 corresponds to 0..360 degrees)
    \param[in] S   saturation value (0..255)
    \param[in] V   (brightness) value (0..255)
*/
void ws2801SetHSV(const uint16_t ix, const uint8_t H, const uint8_t S, const uint8_t V);

//! set LED colour given red, green and blue values
/*!
    \param[in] ix  the index of the LED on the strip (0..(#USER_WS2801_NUMLEDS - 1))
    \param[in] R   red value (0..255)
    \param[in] G   green value (0..255)
    \param[in] B   blue value (0..255)
*/
void ws2801SetRGB(const uint16_t ix, const uint8_t R, const uint8_t G, const uint8_t B);

//! update LEDs, initiate writing data to the LED chain
void ws2801Flush(void);


#endif // __USER_WS2801_H__
//@}
// eof
