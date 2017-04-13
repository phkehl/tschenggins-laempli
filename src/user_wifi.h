/*!
    \file
    \brief flipflip's Tschenggins Lämpli: WiFi access point and station (see \ref USER_WIFI)

    - Copyright (c) 2017 Philippe Kehl <flipflip at oinkzwurgl dot org>,
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

    \defgroup USER_WIFI WIFI
    \ingroup USER

    This implements the WiFi access point and station functionality. It also installs a status page
    on the \ref USER_HTTPD at /wifi.

    @{
*/
#ifndef __USER_WIFI_H__
#define __USER_WIFI_H__

#include "user_stuff.h"


//! initialise WiFi stuff
void wifiInit(void);

//! SDK stuff
void user_rf_pre_init(void);

//! SDK stuff
uint32_t ICACHE_FLASH_ATTR user_rf_cal_sector_set(void);

//! start wifi
/*!
    \param[in] sta  start station
    \param[in] ap   start access point
*/
void wifiStart(const bool sta, const bool ap);

//! print wifi status via DEBUG()
void wifiStatus(void);

//! are we online?
/*!
    \returns true if the station is online, false otherwise
*/
bool wifiIsOnline(void);

//! check if a given address on the access point network
/*!
    \param[in] ip  IP address to check in four-digits notation

    \returns true if the given IO address is on the access point network, false if it is not
*/
bool wifiIsApNet(const uint8_t ip[4]);


#endif // __USER_WIFI_H__
//@}
// eof
