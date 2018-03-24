/*!
    \file
    \brief flipflip's Tschenggins Lämpli: handy stuff (see \ref FF_STUFF)

    - Copyright (c) 2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

*/

#include "stdinc.h"

#include "debug.h"
#include "stuff.h"

uint8_t getSystemName(char *name, const uint8_t size)
{
    memset(name, 0, size);
    strncpy(name, "tschenggins-laempli-ng", size);
    name[size - 1] = '\0';
    if (size >= 12)
    {
        const int len = strlen(name);
        const uint32_t chipId = sdk_system_get_chip_id(); // same as default mac[3..5]
        sprintf(&name[ len > (int)(size - 8) ? (int)(size - 8) : len ],
            "-%06x", chipId & 0x00ffffff);
    }
    const uint8_t len = strlen(name);
    //DEBUG("getSystemName() %s (%u/%u)", name, len, size - 1);
    return len;
}

const char *getSystemId(void)
{
    static char name[7];
    const uint32_t chipId = sdk_system_get_chip_id(); // same as default mac[3..5]
    sprintf(name, "%06x", chipId & 0x00ffffff);
    return name;
}


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

const char *sdkStationConnectStatusStr(const uint8_t status)
{
    switch (status)
    {
        case STATION_IDLE:           return "IDLE";
        case STATION_CONNECTING:     return "CONNECTING";
        case STATION_WRONG_PASSWORD: return "WRONG_PASSWORD";
        case STATION_NO_AP_FOUND:    return "NO_AP_FOUND";
        case STATION_CONNECT_FAIL:   return "CONNECT_FAIL";
        case STATION_GOT_IP:         return "GOT_IP";
    }
    return "???";
}

const char *sdkWifiOpmodeStr(const uint8_t opmode)
{
    switch (opmode)
    {
        case NULL_MODE:      return "NULL";
        case STATION_MODE:   return "STATION";
        case SOFTAP_MODE:    return "SOFTAP";
        case STATIONAP_MODE: return "STATIONAP";
    }
    return "???";
}

const char *sdkDhcpStatusStr(const enum sdk_dhcp_status status)
{
    switch (status)
    {
        case DHCP_STOPPED: return "STOPPED";
        case DHCP_STARTED: return "STARTED";
    }
    return "???";
}

const char *sdkWifiPhyModeStr(const enum sdk_phy_mode mode)
{
    switch (mode)
    {
        case PHY_MODE_11B: return "11B";
        case PHY_MODE_11G: return "11G";
        case PHY_MODE_11N: return "11N";
    }
    return "???";
}

const char *sdkWifiSleepTypeStr(const enum sdk_sleep_type type)
{
    switch (type)
    {
        case WIFI_SLEEP_NONE:  return "NONE";
        case WIFI_SLEEP_LIGHT: return "LIGHT";
        case WIFI_SLEEP_MODEM: return "MODEM";
    }
    return "???";
}

void stuffInit(void)
{
    DEBUG("stuffInit()");
}


// eof
