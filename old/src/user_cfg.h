/*!
    \file
    \brief flipflip's Tschenggins Lämpli: configuration (see \ref USER_CFG)

    - Copyright (c) 2017 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

    \defgroup USER_CFG CFG
    \ingroup USER

    This implements the configuration interface. The configuration can be stored to and loaded from
    flash and there's a web interface at /config.

    Configuration:
    - see the \c Makefile for \c FF_CFGADDR

    @{
*/
#ifndef __USER_CFG_H__
#define __USER_CFG_H__

#include "user_stuff.h"
#include "user_httpd.h"
#include "user_app.h"
#include "user_config.h"

//! initialise configuration system (load config from flash and optionally reset to defaults)
/*!
    \param[in] reset  reset configuration to defaults
*/
void cfgInit(const bool reset);

//! configuration structure version
#define USER_CFG_VERSION 12 // bump when changing struct USER_CONFIG_s

//! configuration data
typedef struct USER_CFG_s
{
    char staSsid[32];        //!< station ssid
    char staPass[32];        //!< station password
    char staName[32];        //!< station host name

    char apSsid[32];         //!< access point ssid
    char apPass[32];         //!< access point password

    char userPass[USER_HTTPD_PASS_LEN_MAX];  //!< httpd user password
    char adminPass[USER_HTTPD_PASS_LEN_MAX]; //!< httpd admin password

    char statusUrl[256];         //!< tschenggins-status.pl server URL

    bool haveChewie;             //!< have Chewbacca sound module (or fall-back to melody)
    bool beNoisy;                //!< make sounds on events
    __PAD(2);                    //!< struct padding

    uint32_t leds[USER_APP_NUM_LEDS]; //!< LEDs assignment (job IDs)

} USER_CFG_t;

//! get a copy of the configuration
/*!
    \param[out] pCfg  configuration structure to fill in
*/
void cfgGet(USER_CFG_t *pCfg);

//! get pointer to the configuration
/*!
    \returns pointer to the configuration structure
*/
const USER_CFG_t *cfgGetPtr(void);

//! get configuration defaults
/*!
    \param[out] pCfg  configuration structure to fill in
*/
void cfgDefault(USER_CFG_t *pCfg);

//! set configuration
/*!
    \param[in] pkCfg  configuration data
*/
bool cfgSet(const USER_CFG_t *pkCfg);

//! debug configuration (via DEBUG())
/*!
    \param[in] pkCfg  configuration data
*/
void cfgDebug(const USER_CFG_t *pkCfg);


#endif // __USER_CFG_H__
//@}
// eof
