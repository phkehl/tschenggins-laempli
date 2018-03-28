/*!
    \file
    \brief flipflip's Tschenggins Lämpli: system status (noises and LED) (see \ref FF_STATUS)

    - Copyright (c) 2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli
*/

#include "stdinc.h"

#include "debug.h"
#include "stuff.h"
#include "tone.h"
#include "status.h"


void statusMakeNoise(const STATUS_NOISE_t noise)
{
    // FIXME: not if beNoisy=false

    if (toneIsPlaying())
    {
        return;
    }

    switch (noise)
    {
        case STATUS_NOISE_ABORT:
        {
            static const int16_t skNoiseAbort[] =
            {
                TONE(A5, 30), TONE(PAUSE, 20), TONE(G5, 60), TONE_END
            };
            toneMelody(skNoiseAbort);
            break;
        }
        case STATUS_NOISE_FAIL:
        {
            static const int16_t skNoiseFail[] =
            {
                TONE(A5, 30), TONE(PAUSE, 20), TONE(G5, 60), TONE(PAUSE, 20), TONE(F5, 100), TONE_END
            };
            toneMelody(skNoiseFail);
            break;
        }
        case STATUS_NOISE_ONLINE:
        {
            static const int16_t skNoiseOnline[] =
            {
                TONE(D6, 30), TONE(PAUSE, 20), TONE(E6, 60), TONE_END
            };
            toneMelody(skNoiseOnline);
            break;
        }
        case STATUS_NOISE_OTHER:
        {
            static const int16_t skNoiseOther[] =
            {
                TONE(C6, 30), TONE_END
            };
            toneMelody(skNoiseOther);
            break;
        }
    }
}






void statusInit(void)
{
    DEBUG("statusInit()");
}

// eof
