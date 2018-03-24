/*!
    \file
    \brief flipflip's Tschenggins Lämpli: wifi things (see \ref FF_WIFI)

    - Copyright (c) 2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli
*/

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#include <espressif/esp_system.h>
#include <espressif/user_interface.h>
#include <espressif/esp_wifi.h>
#include <espressif/esp_sta.h>
#include <espressif/esp_misc.h>
#include <espressif/queue.h>

#include <FreeRTOS.h>
#include <task.h>

#include "debug.h"
#include "stuff.h"
#include "wifi.h"
#include "cfg_gen.h"

#if (!defined FF_CFG_STASSID || !defined FF_CFG_STAPASS || !defined FF_CFG_BACKENDURL)
#  define HAVE_CONFIG 0
#  warning incomplete or missing configuration
#else
#  define HAVE_CONFIG 1
#endif

#if (HAVE_CONFIG > 0)
static void sWifiTask(void *pArg)
{
    while (true)
    {
        PRINT("wifi: ...");
        osSleep(1000);
    }
}



#else // (HAVE_CONFIG > 0)

#define WIFI_SCAN_PERIOD 5000

void sWifiScanDoneCb(void *pArg, sdk_scan_status_t status)
{
    static const char * const skScanStatusStrs [] =
    {
        [SCAN_OK] = "OK", [SCAN_FAIL] = "FAIL", [SCAN_PENDING] = "PENDING", [SCAN_BUSY] = "BUSY", [SCAN_CANCEL] = "CANCEL"
    };
    switch (status)
    {
        case SCAN_FAIL:
        case SCAN_PENDING:
        case SCAN_BUSY:
        case SCAN_CANCEL:
            ERROR("wifi: scan fail: %s", skScanStatusStrs[status]);
            return;
        case SCAN_OK:
            break;
        default:
            ERROR("wifi: scan fail: %u", status);
            return;
    }

    // we get a list of found access points
    const struct sdk_bss_info *pkBss = (const struct sdk_bss_info *)pArg;
    pkBss = STAILQ_NEXT(pkBss, next); // the first is rubbish

    while (pkBss != NULL)
    {
        char ssid[34];
        strncpy(ssid, (const char *)pkBss->ssid, sizeof(ssid) - 1);
        const int len = strlen(ssid);
        if (pkBss->is_hidden != 0)
        {
            ssid[len] = '*';
            ssid[len + 1] = '\0';
        }

        PRINT("wifi: scan: ssid=%-33s bssid="MACSTR" channel=%02u rssi=%02d auth=%s",
            ssid, MAC2STR(pkBss->bssid), pkBss->channel, pkBss->rssi, sdkAuthModeStr(pkBss->authmode));

        pkBss = STAILQ_NEXT(pkBss, next);
    }
}

static void sWifiTask(void *pArg)
{
    while (true)
    {
        static uint32_t sTick;
        vTaskDelayUntil(&sTick, MS2TICK(WIFI_SCAN_PERIOD));
        PRINT("wifi: no config -- initiating wifi scan");
        sdk_wifi_station_scan(NULL, sWifiScanDoneCb);
    }
}

#endif // (HAVE_CONFIG > 0)

void wifiInit(void)
{
    DEBUG("wifiInit()");

    sdk_wifi_set_opmode(STATION_MODE);
    sdk_wifi_set_sleep_type(WIFI_SLEEP_MODEM);

    xTaskCreate(sWifiTask, "ff_wifi", 320, NULL, 2, NULL);
}

// eof
