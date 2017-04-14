// by Philippe Kehl <flipflip at oinkzwurgl dot org>

#include "user_stuff.h"
#include "user_status.h"
#include "user_wifi.h"
#include "user_ws2801.h"
#include "user_app.h"
#include "user_tone.h"
#include "user_html.h"
#include "user_wget.h"
#include "user_fs.h"
#include "version_gen.h"
#include "user_config.h"

// forward declarations
static void sStartFunc(void *pArg);
void user_init(void); // the SDK's user "main"
static void sMonitorTimerCb(void *pArg);


/* ***** initialisation ************************************************************************** */

// initialise, and start stuff
void ICACHE_FLASH_ATTR user_init(void)
{
    // initialise GPIO subsystem
    gpio_init();

    //espconn_secure_set_size(ESPCONN_CLIENT, 2048);

    // initialise debugging output
    stuffInit();

    // say hello
    NOTICE("------------------------------------------------------------------------------------------");
    NOTICE(FF_PROJTITLE " - Model " STRINGIFY(FF_MODEL) " (" FF_PROJECT " " FF_BUILDVER " "FF_BUILDDATE ")");
    NOTICE("Copyright (c) " FF_COPYRIGHT " <" FF_COPYRIGHT_EMAIL ">");
    NOTICE("Parts copyright by others. See source code.");
    NOTICE(FF_PROJLINK);
    NOTICE("------------------------------------------------------------------------------------------");

    // print some system information
    DEBUG("SDK %s, Chip 0x%08x", system_get_sdk_version(), system_get_chip_id());
    DEBUG("GCC " FF_GCCVERSION);
    DEBUG("Boot ver: %u, mode: %u", system_get_boot_version(), system_get_boot_mode());
    DEBUG("Frequency: %u", system_get_cpu_freq()); // MHz
    static const char flashMap[][10] PROGMEM =
    {
        { "4M_256\0" }, { "2M\0" }, { "8M_512\0" }, { "16M_512\0" },
        { "32M_512\0" }, { "16M_1024\0" }, { "32M_1024\0" }
    };
    DEBUG("Flash: %s", flashMap[system_get_flash_size_map()]);
    static const char resetReason[][10] PROGMEM =
    {
        { "power\0" }, { "watchdog\0" }, { "exception\0" }, { "soft_wd\0" },
        { "soft\0" }, { "deepsleep\0" }, { "hardware\0" }
    };
    const struct rst_info *pkResetInfo = system_get_rst_info();
    switch (pkResetInfo->reason)
    {
        case REASON_SOFT_WDT_RST:
        case REASON_WDT_RST:
        case REASON_EXCEPTION_RST:
            ERROR("Reset: %s", resetReason[pkResetInfo->reason]);
            if (pkResetInfo->reason == REASON_EXCEPTION_RST)
            {
                const char *exceptDesc = PSTR("other exception");
                switch (pkResetInfo->exccause)
                {
                    case  0: exceptDesc = PSTR("invalid instruction"); break;
                    case  6: exceptDesc = PSTR("division by zero");    break;
                    case  9: exceptDesc = PSTR("unaligned access");    break;
                    case 28:
                    case 29: exceptDesc = PSTR("invalid address");     break;
                }
                ERROR("Fatal exception: %s", exceptDesc);
                ERROR("epc1=0x%08x epc2=0x%08x epc3=0x%08x excvaddr=0x%08x depc=0x%08x",
                    pkResetInfo->epc1, pkResetInfo->epc2, pkResetInfo->epc3,
                    pkResetInfo->excvaddr, pkResetInfo->depc);
            }
            break;
        default:
            DEBUG("Reset: %s", resetReason[pkResetInfo->reason]);
            break;
    }

    // and finally delay the start of the application a bit
    // (or the wifi stuff will complain, although it will work)
    static os_timer_t sStartTimer;
    os_timer_disarm(&sStartTimer);
    os_timer_setfn(&sStartTimer, (os_timer_func_t *)sStartFunc, NULL);
    os_timer_arm(&sStartTimer, 1000, 0); // fire in 1s

    // system_init_done_cb() doesn't work here
}


/* ***** main function *************************************************************************** */

static void ICACHE_FLASH_ATTR sStartFunc(void *pArg)
{
    UNUSED(pArg);

    // check if flash button is pressed
    GPIO_ENA_PIN_D3();
    GPIO_DIR_CLR(PIN_D3); // input
    PIN_PULLUP_EN(PERIPHS_IO_MUX_GPIO0_U);

    // reset configuration to default?
    const bool reset = GPIO_IN_READ(PIN_D3) ? false : true;
    if (reset)
    {
        WARNING("***** RESET *****");
    }

    // initialise our modules
    cfgInit(reset);  // configuration
    statusInit();       // status LED
    httpdInit();        // http server
    fsInit();           // filesystem
    wifiInit();         // wifi connectivity
    ws2801Init();       // WS2801 LEDs
    toneInit();         // tone/melody generator
    htmlInit();         // html pages
    wgetInit();         // http GETter init
    appInit();          // main application

    // print monitor info early
    sMonitorTimerCb(NULL);

    // setup monitor a "task" that will periodicylly print system status
    static os_timer_t sMonitorTimer;
    os_timer_disarm(&sMonitorTimer);
    os_timer_setfn(&sMonitorTimer, (os_timer_func_t *)sMonitorTimerCb, NULL);
    os_timer_arm(&sMonitorTimer, 5000, 1); // fire every 5s

    // start wifi and httpd
    wifiStart(true, true);
    httpdStart();

    // then we start the main user application
    appStart();
}


/* ***** monitor "task" ************************************************************************** */

// monitor things
static void ICACHE_FLASH_ATTR sMonitorTimerCb(void *pArg)
{
    UNUSED(pArg);
    static uint32_t lastRtc;
    const uint32_t msss = system_get_time() / 1000; // ms
    const uint32_t thisRtc = system_get_rtc_time();
    const uint32_t drtc = roundl( (double)(lastRtc ? thisRtc - lastRtc : 0)
        * (1.0/1000.0/4096.0) * system_rtc_clock_cali_proc() ); // us -> ms
    lastRtc = thisRtc;
    DEBUG("--------------------------------------------------------------------------------");
    DEBUG("mon: main: msss=%u drtc=%u heap=%u/%u/%u/%d", msss, drtc,
        memGetMinFree(), memGetFree(), memGetMaxFree(), memGetNumAlloc());
    stuffStatus();
    wifiStatus();
    httpdStatus();
    appStatus();
    DEBUG("--------------------------------------------------------------------------------");

    //DEBUG("Voltage: %dmV", lround(system_get_vdd33() * (1000.0/1024.0)));

#ifdef MEMLEAK_DEBUG
#  warning MEMLEAK_DEBUG enabled!
    system_print_meminfo();
    system_show_malloc();
#endif
}

#ifdef MEMLEAK_DEBUG
bool check_memleak_debug_enable(void);
bool ICACHE_FLASH_ATTR check_memleak_debug_enable(void)
{
    return MEMLEAK_DEBUG_ENABLE;
}
#endif


/* *********************************************************************************************** */

// eof
