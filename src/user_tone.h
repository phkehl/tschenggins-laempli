/*!
    \file
    \brief flipflip's Tschenggins Lämpli: tones and melodies (see \ref USER_TONE)

    - Copyright (c) 2017 Philippe Kehl <flipflip at oinkzwurgl dot org>,
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

    \defgroup USER_TONE TONE
    \ingroup USER

    This implements tones and melodies (piezo or other small speaker on GPIO, currently hard-coded
    to GPIO4/D2).

    @{
*/
#ifndef __USER_TONE_H__
#define __USER_TONE_H__

#include "user_stuff.h"
#include "rtttl.h"

//! initialise tone module
void toneInit(void);

//! play a tone
/*!
    \note This is non-blocking and returns immediately, while the tone might still be playing. Use
    toneMelody() for melodies.

    \param[in] freq  frequency [Hz]
    \param[in] dur   duration [ms]
*/
void toneStart(const uint32_t freq, const uint32_t dur);

//! stop playing tone/melody
void toneStop(void);

//! helper macro for toneMelody()
#define TONE(note, dur) RTTTL_NOTE_ ## note, dur

//! indicates end of melody
#define TONE_END RTTTL_NOTE_END

//! special note that indicates a pause
#define TONE_PAUSE RTTTL_NOTE_PAUSE

//! play a melody given a series of frequency-duration pairs
/*!
    \note This is non-blocking and returns immediately. It stopy any currently playing tone or
    melody.

    \param[in] pkFreqDur  list of pairs of tone frequency and duration

    Example:
\code{.c}
const int16_t pkFreqDur[] = { TONE(B4, 200), TONE(PAUSE, 50), TONE(C4, 500), TONE_END };
toneMelody(pkFreqDur);
\endcode
*/
void toneMelody(const int16_t *pkFreqDur);

//! play melody given a name of a melody
/*!
    \param[in] name  name of the melody (see rtttl.c for available names)
*/
void toneBuiltinMelody(const char *name);

//! play melody in RTTTL format
/*!
    \param[in] rtttl  string with melody in RTTTL format (see rtttl.c, https://en.wikipedia.org/wiki/Ring_Tone_Transfer_Language)
*/
void toneRtttlMelody(const char *rtttl);

#endif // __USER_TONE_H__
//@}
// eof
