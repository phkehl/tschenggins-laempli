/*!
    \file
    \brief flipflip's Tschenggins Lämpli: tones and melodies (see \ref FF_TONE)

    - Copyright (c) 2017-2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

    \addtogroup FF_TONE

    @{
*/

#include "stdinc.h"

#include <esp8266.h>

#include "stuff.h"
#include "debug.h"
#include "mon.h"
#include "tone.h"

/* *********************************************************************************************** */

#define TONE_GPIO 4

// forward declarations
static void sToneIsr(void *pArg);
static void sToneStop(void);
static void sToneStart(void);

static volatile bool sToneIsPlaying;

void toneStart(const uint32_t freq, const uint32_t dur)
{
    sToneStop();

    // set "melody"
    const int16_t pkFreqDur[] = { freq, dur, TONE_END };
    toneMelody(pkFreqDur);
}

#define TONE_MELODY_N 50

static volatile uint32_t svToneMelodyTimervals[TONE_MELODY_N + 1];
static volatile int16_t  svToneMelodyTogglecnts[TONE_MELODY_N + 1];
static volatile int16_t  svToneMelodyIx;

#define PAUSE_FREQ 1000

void toneMelody(const int16_t *pkFreqDur)
{
    sToneStop();
    uint32_t totalDur = 0;
    uint16_t nNotes = 0;

    //memset(svToneMelodyTimervals, 0, sizeof(svToneMelodyTimervals));
    for (int ix = 0; ix < NUMOF(svToneMelodyTimervals); ix++)
    {
        svToneMelodyTimervals[ix] = 0;
    }
    //memset(svToneMelodyTogglecnts, 0, sizeof(svToneMelodyTogglecnts));
    for (int ix = 0; ix < NUMOF(svToneMelodyTogglecnts); ix++)
    {
        svToneMelodyTogglecnts[ix] = 0;
    }
    svToneMelodyIx = 0;

    for (int16_t ix = 0; ix < TONE_MELODY_N; ix++)
    {
        const int16_t freq = pkFreqDur[ 2 * ix ];
        if ( (freq != TONE_END) && (freq > 0) )
        {
            const int16_t _freq = freq != TONE_PAUSE ? freq : PAUSE_FREQ;
            const int16_t dur = pkFreqDur[ (2 * ix) + 1 ];

            // timer value
            const uint32_t timerval = (APB_CLK_FREQ / 1000000 * 500000) / _freq;

            // number of times to toggle the PIO
            const int16_t togglecnt = 2 * _freq * dur / 1000;

            svToneMelodyTimervals[ix]  = timerval;
            svToneMelodyTogglecnts[ix] = freq != TONE_PAUSE ? togglecnt : -togglecnt;

            totalDur += dur;
            nNotes++;

            //DEBUG("toneMelody() %2d %4d %4d -> %6u %4d", ix, freq, dur, timerval, togglecnt);
        }
        else
        {
            break;
        }
    }

    DEBUG("toneMelody() %ums, %u", totalDur, nNotes);

    // configure hw timer
    timer_set_divider(FRC1, TIMER_CLKDIV_1);
    timer_set_reload(FRC1, true);
    timer_set_interrupts(FRC1, true); // enable and unmask interrupt

    sToneIsPlaying = true;
    sToneStart();
}

/* *********************************************************************************************** */

static volatile int16_t svToneToggleCnt;
static volatile bool svToneSilent;

static void sToneStart(void) // RAM func
{
    gpio_write(TONE_GPIO, false);

    const int32_t timerval  = svToneMelodyTimervals[svToneMelodyIx];
    const int16_t togglecnt = svToneMelodyTogglecnts[svToneMelodyIx];
    svToneMelodyIx++;

    if (togglecnt)
    {
        svToneToggleCnt = togglecnt < 0 ? -togglecnt : togglecnt;
        svToneSilent = togglecnt < 0 ? true : false;

        // arm timer (23 bits, 0-8388607)
        timer_set_load(FRC1, timerval);
        timer_set_run(FRC1, true);
    }
    else
    {
        sToneStop();
    }
}

static void sToneStop(void) // RAM func
{
    timer_set_run(FRC1, false); // stop timer
    timer_set_interrupts(FRC1, false); // disable and mask interrupt

    gpio_write(TONE_GPIO, false);
    sToneIsPlaying = false;
}

void toneStop(void)
{
    sToneStop();
}

__INLINE bool toneIsPlaying(void)
{
    return sToneIsPlaying;
}


IRAM static void sToneIsr(void *pArg) // RAM func
{
    monIsrEnter();
    //UNUSED(pArg);

    // toggle PIO...
    if (!svToneSilent)
    {
        if (svToneToggleCnt & 0x1)
        {
            gpio_write(TONE_GPIO, true);
        }
        else
        {
            gpio_write(TONE_GPIO, false);
        }
    }

    svToneToggleCnt--;

    // ...until done
    if (svToneToggleCnt <= 0)
    {
        sToneStart();
    }

    monIsrLeave();
}


/* *********************************************************************************************** */

void toneBuiltinMelody(const char *name)
{
    const char *rtttl = rtttlBuiltinMelody(name);
    if (rtttl == NULL)
    {
        ERROR("tone: no such melody: %s", name);
        return;
    }
    toneRtttlMelody(rtttl);
}

void toneRtttlMelody(const char *rtttl)
{
    toneStop();
    const int nMelody = (TONE_MELODY_N * 2) + 1;
    int16_t *pMelody = malloc( nMelody * sizeof(int16_t) );
    if (pMelody)
    {
        rtttlMelody(rtttl, pMelody, nMelody);
        toneMelody(pMelody);
        free(pMelody);
    }
    else
    {
        ERROR("tone: malloc fail");
    }
}

/* *********************************************************************************************** */


void toneInit(void)
{
    DEBUG("tone: init (%uMHz)", APB_CLK_FREQ/1000000);

    gpio_enable(TONE_GPIO, GPIO_OUTPUT);
    gpio_write(TONE_GPIO, false);

    // disable and mask interrupt
    timer_set_interrupts(FRC1, false);

    // stop timer
    timer_set_run(FRC1, false);

    // attach ISR
    _xt_isr_attach(INUM_TIMER_FRC1, sToneIsr, NULL);
}


/* *********************************************************************************************** */
//@}
// eof
