// by Philippe Kehl <flipflip at oinkzwurgl dot org>

#include "user_stuff.h"
#include "user_tone.h"
#include "user_httpd.h"
#include "user_html.h"
#include "html_gen.h"

/* *********************************************************************************************** */

#define TONE_TEST 0


/* *********************************************************************************************** */

// constants missing in eagle_soc.h
#define FRC1_DIV_1        (0)            // use timer clock
#define FRC1_DIV_16       (4)            // use timer clock / 16
#define FRC1_DIV_256      (8)            // use timer clock / 256
#define FRC1_INT_EDGE     (0)            // edge interrupt
#define FRC1_INT_LEVEL    (1)            // level interrupt
#define FRC1_ENABLE       (BIT7)         // timer enable
#define FRC1_AUTOLOAD     (BIT6)         // autoload timer value again (loop)

// forward declarations
static void sToneIsr(void *pArg);
static void sToneStop(void);
static void sToneStart(void);

static volatile bool sToneIsPlaying;

void ICACHE_FLASH_ATTR toneStart(const uint32_t freq, const uint32_t dur)
{
    sToneStop();

    // set "melody"
    const int16_t pkFreqDur[] = { freq, dur, TONE_END };
    toneMelody(pkFreqDur);
}

#define TONE_MELODY_N 50

static uint32_t sToneMelodyTimervals[TONE_MELODY_N + 1];
static int16_t sToneMelodyTogglecnts[TONE_MELODY_N + 1];
static volatile int16_t svToneMelodyIx;

#define PAUSE_FREQ 1000

void ICACHE_FLASH_ATTR toneMelody(const int16_t *pkFreqDur)
{
    sToneStop();
    uint32_t totalDur = 0;
    uint16_t nNotes = 0;

    os_memset(sToneMelodyTimervals, 0, sizeof(sToneMelodyTimervals));
    os_memset(sToneMelodyTogglecnts, 0, sizeof(sToneMelodyTogglecnts));
    svToneMelodyIx = 0;

    for (int16_t ix = 0; ix < TONE_MELODY_N; ix++)
    {
        const int16_t freq = pgm_read_uint16(&pkFreqDur[ 2 * ix ]);
        if ( (freq != TONE_END) && (freq > 0) )
        {
            const int16_t _freq = freq != TONE_PAUSE ? freq : PAUSE_FREQ;
            const int16_t dur = pgm_read_uint16(&pkFreqDur[ (2 * ix) + 1 ]);

            // timer value
            const uint32_t timerval = (APB_CLK_FREQ / 1000000 * 500000) / _freq;

            // number of times to toggle the PIO
            const int16_t togglecnt = 2 * _freq * dur / 1000;

            sToneMelodyTimervals[ix]  = timerval;
            sToneMelodyTogglecnts[ix] = freq != TONE_PAUSE ? togglecnt : -togglecnt;

            totalDur += dur;
            nNotes++;

#if (TONE_TEST > 0)
            DEBUG("toneMelody() %2d %4d %4d -> %6u %4d", ix, freq, dur, timerval, togglecnt);
#endif
        }
        else
        {
            break;
        }
    }

    DEBUG("toneMelody() %ums, %u", totalDur, nNotes);

    // configure hw timer
    RTC_REG_WRITE(FRC1_CTRL_ADDRESS, FRC1_AUTOLOAD | FRC1_DIV_1 | FRC1_ENABLE | FRC1_INT_EDGE);

    // attach hw timer ISR
    ETS_FRC_TIMER1_INTR_ATTACH(sToneIsr, NULL);
    ETS_FRC1_INTR_ENABLE();

    sToneIsPlaying = true;
    sToneStart();
}

/* *********************************************************************************************** */

static volatile int16_t svToneToggleCnt;
static volatile bool svToneSilent;

static void sToneStart(void) // RAM func
{
    GPIO_OUT_CLR(PIN_D2);

    const int32_t timerval  = sToneMelodyTimervals[svToneMelodyIx];
    const int16_t togglecnt = sToneMelodyTogglecnts[svToneMelodyIx];
    svToneMelodyIx++;

    if (togglecnt)
    {
        svToneToggleCnt = togglecnt < 0 ? -togglecnt : togglecnt;
        svToneSilent = togglecnt < 0 ? true : false;

        // arm timer (23 bits, 0-8388607)
        RTC_REG_WRITE(FRC1_LOAD_ADDRESS, timerval);
        TM1_EDGE_INT_ENABLE();
        RTC_REG_WRITE(FRC1_INT_ADDRESS, FRC1_INT_CLR_MASK);
    }
    else
    {
        sToneStop();
    }
}

static void sToneStop(void) // RAM func
{
    RTC_REG_WRITE(FRC1_CTRL_ADDRESS, 0);
    TM1_EDGE_INT_DISABLE();
    GPIO_OUT_CLR(PIN_D2);
    sToneIsPlaying = false;
}

void ICACHE_FLASH_ATTR toneStop(void)
{
    sToneStop();
}

inline bool toneIsPlaying(void)
{
    return sToneIsPlaying;
}


static void sToneIsr(void *pArg) // RAM func
{
    UNUSED(pArg);
    RTC_REG_WRITE(FRC1_INT_ADDRESS, FRC1_INT_CLR_MASK);

    // toggle PIO...
    if (!svToneSilent)
    {
        if (svToneToggleCnt & 0x1)
        {
            GPIO_OUT_SET(PIN_D2);
        }
        else
        {
            GPIO_OUT_CLR(PIN_D2);
        }
    }

    svToneToggleCnt--;

    // ...until done
    if (svToneToggleCnt <= 0)
    {
        sToneStart();
    }
}


/* *********************************************************************************************** */

void ICACHE_FLASH_ATTR toneBuiltinMelody(const char *name)
{
    const char *rtttl = rtttlBuiltinMelody(name);
    if (rtttl == NULL)
    {
        ERROR("tone: no such melody: %s", name);
        return;
    }
    toneRtttlMelody(rtttl);
}

void ICACHE_FLASH_ATTR toneRtttlMelody(const char *rtttl)
{
    toneStop();
    const int nMelody = (TONE_MELODY_N * 2) + 1;
    int16_t *pMelody = memAlloc( nMelody * sizeof(int16_t) );
    if (pMelody)
    {
        rtttlMelody(rtttl, pMelody, nMelody);
        toneMelody(pMelody);
        memFree(pMelody);
    }
    else
    {
        ERROR("tone: malloc fail");
    }
}

/* *********************************************************************************************** */

#if (TONE_TEST > 0)
#  warning TONE_TEST enabled
static int foo;
static void ICACHE_FLASH_ATTR sTestTimerFunc(void *arg)
{
    UNUSED(arg);
    DEBUG("tone: test");
    const int16_t pkMelody[] =
    {
        TONE(C4, 250), TONE(PAUSE, 50), // do
        TONE(D4, 250), TONE(PAUSE, 50), // re
        TONE(E4, 250), TONE(PAUSE, 50), // mi
        TONE(F4, 250), TONE(PAUSE, 50), // fa
        TONE(G4, 250), TONE(PAUSE, 50), // sol
        TONE(A4, 250), TONE(PAUSE, 50), // la
        TONE(B4, 250), TONE(PAUSE, 50), // si
        TONE_NOTE_END
    };
    toneMelody(pkMelody);
}
#endif

/* *********************************************************************************************** */


#if (TONE_TEST > 0)
static os_timer_t sTestTimer;
static void ICACHE_FLASH_ATTR sTestTimerFunc(void *arg);
#endif

void ICACHE_FLASH_ATTR toneInit(void)
{
    DEBUG("tone: init (%uMHz)", APB_CLK_FREQ/1000000);

    GPIO_ENA_PIN_D2();
    GPIO_DIR_SET(PIN_D2);
    GPIO_OUT_CLR(PIN_D2);

#if (TONE_TEST > 0)
    os_timer_disarm(&sTestTimer);
    os_timer_setfn(&sTestTimer, (os_timer_func_t *)sTestTimerFunc, NULL);
    os_timer_arm(&sTestTimer, 5000, 1); // 5s interval, repeated
#endif
}


/* *********************************************************************************************** */

// eof
