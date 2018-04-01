/*!
    \file
    \brief flipflip's Tschenggins Lämpli: main program and startup

    - Copyright (c) 2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli
*/

#include "stdinc.h"

#include <lwip/init.h>

#include "stuff.h"
#include "debug.h"
#include "mon.h"
#include "jenkins.h"
#include "wifi.h"
#include "tone.h"
#include "config.h"
#include "status.h"
#include "backend.h"
#include "version_gen.h"

//void vApplicationIdleHook(void)
//{
//    // sdk_wifi_set_sleep_type(WIFI_SLEEP_MODEM);
//}

static void sMainHello(void)
{
    NOTICE("------------------------------------------------------------------------------------------");
    NOTICE("Tschenggins Lämpli NG ("FF_BUILDVER" "FF_BUILDDATE")");
    NOTICE("Copyright (c) 2018 Philippe Kehl & flipflip industries <flipflip at oinkzwurgl dot org>");
    NOTICE("Parts copyright by others. See source code.");
    NOTICE("https://oinkzwurgl.org/projaeggd/tschenggins-laempli");
    NOTICE("------------------------------------------------------------------------------------------");

    // print some system information
    DEBUG("SDK %s, Chip 0x%08x", sdk_system_get_sdk_version(), sdk_system_get_chip_id());
    DEBUG("GCC "FF_GCCVERSION);
    DEBUG("LwIP "LWIP_VERSION_STRING", FreeRTOS " tskKERNEL_VERSION_NUMBER ", Newlib "_NEWLIB_VERSION);
    DEBUG("Boot ver: %u, mode: %u", sdk_system_get_boot_version(), sdk_system_get_boot_mode());
    DEBUG("Frequency: %uMHz", sdk_system_get_cpu_freq()); // MHz
    //static const char const flashMap[][10] =
    //{
    //    { "4M_256\0" }, { "2M\0" }, { "8M_512\0" }, { "16M_512\0" },
    //   { "32M_512\0" }, { "16M_1024\0" }, { "32M_1024\0" }
    //};
    //DEBUG("Flash: %s", flashMap[sdk_system_get_flash_size_map()]);
    DEBUG("Flash: id=0x%08x size=%u (%uKiB)",
        sdk_spi_flash_get_id(), sdk_flashchip.chip_size, sdk_flashchip.chip_size >> 10);
    static const char * const resetReason[] =
    {
        [DEFAULT_RST] = "default", [WDT_RST] = "watchdog", [EXCEPTION_RST] = "exception", [SOFT_RST] = "soft"
    };
    const struct sdk_rst_info *pkResetInfo = sdk_system_get_rst_info();
    switch ((enum sdk_rst_reason)pkResetInfo->reason)
    {
        case WDT_RST:
        case EXCEPTION_RST:
        case SOFT_RST:
            ERROR("Reset: %s", resetReason[pkResetInfo->reason]);
            if (pkResetInfo->reason == WDT_RST)
            {
                const char *exceptDesc = "other exception";
                switch (pkResetInfo->exccause)
                {
                    case  0: exceptDesc = "invalid instruction"; break;
                    case  6: exceptDesc = "division by zero";    break;
                    case  9: exceptDesc = "unaligned access";    break;
                    case 28:
                    case 29: exceptDesc = "invalid address";     break;
                }
                ERROR("Fatal exception: %s", exceptDesc);
                ERROR("epc1=0x%08x epc2=0x%08x epc3=0x%08x excvaddr=0x%08x depc=0x%08x",
                    pkResetInfo->epc1, pkResetInfo->epc2, pkResetInfo->epc3,
                    pkResetInfo->excvaddr, pkResetInfo->depc);
            }
            break;
        case DEFAULT_RST:
            DEBUG("Reset: default");
            break;
        default:
            WARNING("Reset: %u",pkResetInfo->reason);
            break;
    }
}


void user_init(void)
{
    // initialise stuff
    debugInit(); // must be first
    stuffInit();
    configInit();
    monInit();
    toneInit();
    statusInit();
    backendInit();

    // trigger core dump
    //*((volatile uint32_t *)0) = 0; // null pointer deref, instant crash
    //abort();

    // say hello
    sMainHello();

    NOTICE("here we go...");

    osSleep(100);

    // start stuff
    jenkinsInit();
    wifiInit();

}

// eof
