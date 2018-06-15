/*!
    \file
    \brief flipflip's Tschenggins Lämpli: handy stuff (see \ref FF_STUFF)

    - Copyright (c) 2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

*/

#include "stdinc.h"

#include <lwip/err.h>

#include <esp/hwrand.h>

#include <bearssl.h>
#include <base64.h>

#include "debug.h"
#include "stuff.h"

uint8_t getSystemName(char *name, const uint8_t size)
{
    memset(name, 0, size);
    strncpy(name, "tschenggins-laempli", size);
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

int reqParamsFromUrl(const char *url, char *buf, const int bufSize,
    const char **host, const char **path, const char **query, const char **auth, bool *https, uint16_t *port)
{
    // TODO: url encode query string!

    *host = NULL;
    *path = NULL;
    *query = NULL;
    *https = false;
    *port = 0;

    // determine protocol, hostname and query
    const int urlLen = strlen(url);
    if (urlLen > (bufSize - 1))
    {
        WARNING("reqParamsFromUrl() url too long (%d > %d)", urlLen, (bufSize - 1));
        return 0;
    }

    if (url != buf)
    {
        strcpy(buf, url);
    }
    // buf is now something like
    // "http://foo.com"
    // "http://foo.com/"
    // "http://foo.com/query?with=parameters"
    // "http://user:pass@foo.com/query?with=parameters"
    char *pParse = buf;
    //DEBUG("pParse=%s", pParse);

    // protocol
    {
        char *endOfProt = strstr(pParse, "://");
        if (endOfProt == NULL)
        {
            WARNING("reqParamsFromUrl() missing protocol:// in %s", url);
            return 0;
        }
        *endOfProt = '\0';
        //DEBUG("prot=%s", pParse);
        if (strcmp(pParse, "http") == 0)
        {
            *https = false;
            *port = 80;
        }
        else if (strcmp(pParse, "https") == 0)
        {
            *https = true;
            *port = 443;
        }
        else
        {
            WARNING("reqParamsFromUrl() illegal protocol %s://", buf);
            return false;
        }
        pParse = endOfProt + 3; // "://"
    }
    //DEBUG("pParse=%s", pParse);

    // user:pass@
    int authLen = 0;
    {
        char *end = strstr(pParse, "/");
        if (end == NULL)
        {
            end = &buf[urlLen];
        }
        char *monkey = strstr(pParse, "@");
        //DEBUG("pParse=%p monkey=%p (%d) end=%p (%d)",
        //    pParse, monkey, monkey - pParse, end, end - pParse);
        if ( (monkey != NULL) && (monkey < (end - 3)) )
        {
            *monkey = '\0';
            char *userpass = pParse;
            const int userpassLen = strlen(userpass);
            //DEBUG("userpass=%s (%d)", userpass, userpassLen);
            pParse += userpassLen + 1; // "@"

            // base64 encoded auth token
            authLen = BASE64_ENCLEN(userpassLen);

            // put it at the end of buffer, compare sWgetDoRequest()
            const int authPos = bufSize - authLen - 1;
            char *pAuth = &buf[authPos];
            if ( (authPos < (urlLen + 1)) || !base64enc(userpass, pAuth, authLen))
            {
                WARNING("reqParamsFromUrl() buf error (%d, %d)", authPos, urlLen + 1);
                return false;
            }
            *auth = pAuth;
            //DEBUG("auth=%s", *auth);
        }
    }
    //DEBUG("pParse=%s", pParse);

    // hostname, port
    {
        char *endOfHost = strstr(pParse, "/");
        if (endOfHost == NULL)
        {
            endOfHost = &buf[urlLen];
        }
        *endOfHost = '\0';
        //DEBUG("host=%s", pParse);
        if (strlen(pParse) < 3)
        {
            WARNING("reqParamsFromUrl() illegal host '%s'", *host);
            return false;
        }
        *host = pParse;

        // port
        char *startOfPort = strstr(pParse, ":");
        if (startOfPort != NULL)
        {
            *startOfPort = '\0';
            startOfPort++;
            const int iPort = atoi(startOfPort);
            if ( (iPort < 1) || (iPort > 65535) )
            {
                WARNING("reqParamsFromUrl() illegal port '%s' (%d)", startOfPort, iPort);
            }
            else
            {
                *port = (uint16_t)iPort;
            }
        }

        pParse = endOfHost + 1;
    }
    //DEBUG("pParse=%s", pParse);

    // path
    {
        char *endOfPath = strstr(pParse, "?");
        if (endOfPath == NULL)
        {
            endOfPath = &buf[urlLen];
        }
        *endOfPath = '\0';
        *path = pParse;
        pParse = endOfPath + 1;
        //DEBUG("path=%s", *path);
    }

    // query
    {
        *query = pParse;
        //DEBUG("query=%s", *query);
    }

    return urlLen + 1; // + (authLen ? authLen + 1 : 0);
}

const char *lwipErrStr(const int8_t error)
{
    switch (error)
    {
        case ERR_OK:         return "OK";
        case ERR_MEM:        return "MEM";
        case ERR_BUF:        return "BUF";
        case ERR_TIMEOUT:    return "TIMEOUT";
        case ERR_RTE:        return "RTE";
        case ERR_INPROGRESS: return "INPROGRESS";
        case ERR_VAL:        return "VAL";
        case ERR_WOULDBLOCK: return "WOULDBLOCK";
        case ERR_USE:        return "USE";
        case ERR_ALREADY:    return "ALREADY";
        case ERR_ISCONN:     return "ISCONN";
        case ERR_CONN:       return "CONN";
        case ERR_IF:         return "IF";
        case ERR_ABRT:       return "ABRT";
        case ERR_RST:        return "RST";
        case ERR_CLSD:       return "CLSD";
        case ERR_ARG:        return "ARG";
    }
    return "???";
}

static uint32_t sOsTimePosix;
static uint32_t sOsTimeOs;

void osSetPosixTime(const uint32_t timestamp)
{
    sOsTimeOs = osTime();
    sOsTimePosix = timestamp;
    //DEBUG("osSetPosixTime() %u %u", sOsTimePosix, sOsTimeOs);
}

uint32_t osGetPosixTime(void)
{
    const uint32_t now = osTime();
    //DEBUG("osGetPosixTime() %u %u-%u=%u", sOsTimePosix, now, sOsTimeOs, now - sOsTimeOs);
    return sOsTimePosix + ((now - sOsTimeOs) / 1000);
}

const char *bearSslErrStr(int error, bool *pFatal)
{
    bool fatal = false;
    if (error > BR_ERR_SEND_FATAL_ALERT)
    {
        fatal = true;
        error -= BR_ERR_SEND_FATAL_ALERT;
    }
    if (error > BR_ERR_RECV_FATAL_ALERT)
    {
        fatal = true;
        error -= BR_ERR_RECV_FATAL_ALERT;
    }
    if (pFatal != NULL)
    {
        *pFatal = fatal;
    }
    switch (error)
    {
        // bear_ssl.h
        case BR_ERR_OK:                       return "OK";
        case BR_ERR_BAD_PARAM:                return "BAD_PARAM";
        case BR_ERR_BAD_STATE:                return "BAD_STATE";
        case BR_ERR_UNSUPPORTED_VERSION:      return "UNSUPPORTED_VERSION";
        case BR_ERR_BAD_VERSION:              return "BAD_VERSION";
        case BR_ERR_BAD_LENGTH:               return "BAD_LENGTH";
        case BR_ERR_TOO_LARGE:                return "TOO_LARGE";
        case BR_ERR_BAD_MAC:                  return "BAD_MAC";
        case BR_ERR_NO_RANDOM:                return "NO_RANDOM";
        case BR_ERR_UNKNOWN_TYPE:             return "UNKNOWN_TYPE";
        case BR_ERR_UNEXPECTED:               return "ERR_UNEXPECTED";
        case BR_ERR_BAD_CCS:                  return "BAD_CCS";
        case BR_ERR_BAD_ALERT:                return "BAD_ALERT";
        case BR_ERR_BAD_HANDSHAKE:            return "BAD_HANDSHAKE";
        case BR_ERR_OVERSIZED_ID:             return "OVERSIZED_ID";
        case BR_ERR_BAD_CIPHER_SUITE:         return "CIPHER_SUITE";
        case BR_ERR_BAD_COMPRESSION:          return "BAD_COMPRESSION";
        case BR_ERR_BAD_FRAGLEN:              return "BAD_FRAGLEN";
        case BR_ERR_BAD_SECRENEG:             return "BAD_SECRENEG";
        case BR_ERR_EXTRA_EXTENSION:          return "EXTRA_EXTENSION";
        case BR_ERR_BAD_SNI:                  return "BAD_SNI";
        case BR_ERR_BAD_HELLO_DONE:           return "BAD_HELLO_DONE";
        case BR_ERR_LIMIT_EXCEEDED:           return "LIMIT_EXCEEDED";
        case BR_ERR_BAD_FINISHED:             return "BAD_FINISHED";
        case BR_ERR_RESUME_MISMATCH:          return "RESUME_MISMATCH";
        case BR_ERR_INVALID_ALGORITHM:        return "INVALID_ALGORITHM";
        case BR_ERR_BAD_SIGNATURE:            return "BAD_SIGNATURE";
        case BR_ERR_WRONG_KEY_USAGE:          return "WRONG_KEY_USAGE";
        case BR_ERR_NO_CLIENT_AUTH:           return "NO_CLIENT_AUTH";
        case BR_ERR_IO:                       return "IO";
        // bearssl_x509.h
        case BR_ERR_X509_OK:                  return "X509_OK";
        case BR_ERR_X509_INVALID_VALUE:       return "X509_INVALID_VALUE";
        case BR_ERR_X509_TRUNCATED:           return "X509_TRUNCATED";
        case BR_ERR_X509_EMPTY_CHAIN:         return "X509_EMPTY_CHAIN";
        case BR_ERR_X509_INNER_TRUNC:         return "X509_INNER_TRUNC";
        case BR_ERR_X509_BAD_TAG_CLASS:       return "X509_BAD_TAG_CLASS";
        case BR_ERR_X509_BAD_TAG_VALUE:       return "X509_BAD_TAG_VALUE";
        case BR_ERR_X509_INDEFINITE_LENGTH:   return "X509_INDEFINITE_LENGTH";
        case BR_ERR_X509_EXTRA_ELEMENT:       return "X509_EXTRA_ELEMENT";
        case BR_ERR_X509_UNEXPECTED:          return "X509_UNEXPECTED";
        case BR_ERR_X509_NOT_CONSTRUCTED:     return "X509_NOT_CONSTRUCTED";
        case BR_ERR_X509_NOT_PRIMITIVE:       return "X509_NOT_PRIMITIVE";
        case BR_ERR_X509_PARTIAL_BYTE:        return "X509_PARTIAL_BYTE";
        case BR_ERR_X509_BAD_BOOLEAN:         return "X509_BAD_BOOLEAN";
        case BR_ERR_X509_OVERFLOW:            return "X509_OVERFLOW";
        case BR_ERR_X509_BAD_DN:              return "X509_BAD_DN";
        case BR_ERR_X509_BAD_TIME:            return "X509_BAD_TIME";
        case BR_ERR_X509_UNSUPPORTED:         return "X509_UNSUPPORTED";
        case BR_ERR_X509_LIMIT_EXCEEDED:      return "X509_LIMIT_EXCEEDED";
        case BR_ERR_X509_WRONG_KEY_TYPE:      return "X509_KEY_TYPE";
        case BR_ERR_X509_BAD_SIGNATURE:       return "X509_BAD_SIGNATURE";
        case BR_ERR_X509_TIME_UNKNOWN:        return "X509_TIME_UNKNOWN";
        case BR_ERR_X509_EXPIRED:             return "X509_EXPIRED";
        case BR_ERR_X509_DN_MISMATCH:         return "X509_DN_MISMATCH";
        case BR_ERR_X509_BAD_SERVER_NAME:     return "X509_BAD_SERVER_NAME";
        case BR_ERR_X509_CRITICAL_EXTENSION:  return "X509_CRITICAL_EXTENSION";
        case BR_ERR_X509_NOT_CA:              return "X509_NOT_CA";
        case BR_ERR_X509_FORBIDDEN_KEY_USAGE: return "X509_FORBIDDEN_KEY_USAGE";
        case BR_ERR_X509_WEAK_PUBLIC_KEY:     return "X509_WEAK_PUBLIC_KEY";
        case BR_ERR_X509_NOT_TRUSTED:         return "X509_NOT_TRUSTED";
    }
    return "???";
}


void stuffInit(void)
{
    const uint32_t randSeed = hwrand();
    DEBUG("stuff: init (0x%08x)", randSeed);
    srand(randSeed);
}


// eof
