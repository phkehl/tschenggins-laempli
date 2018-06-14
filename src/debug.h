/*!
    \file
    \brief flipflip's Tschenggins Lämpli: debugging output (see \ref FF_DEBUG)

    - Copyright (c) 2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

    This implements buffered and non-blocking debugging output (uses interrupts and the UART
    hardware FIFO, drops output if the buffer is full). The various DEBUG(), PRINT() etc. functions
    are line-oriented and use a mutex to guarantee intact lines in the output. (Hence they're not
    usable from interrupts!). The output goes via the normal stdout handle, so that it mixes well
    with direct printf() calls (incl. those from the SDK).

    \defgroup FF_DEBUG DEBUG
    \ingroup FF

    @{
*/
#ifndef __DEBUG_H__
#define __DEBUG_H__

#include "stdinc.h"

//! initialise debugging output
void debugInit(void);

void debugMonStatus(void);

void debugLock(void);
void debugUnlock(void);

//! print an error message \hideinitializer
#define ERROR(fmt, ...)   debugLock(); printf("E: " fmt "\n", ## __VA_ARGS__); debugUnlock()

//! print a warning message \hideinitializer
#define WARNING(fmt, ...) debugLock(); printf("W: " fmt "\n", ## __VA_ARGS__); debugUnlock()

//! print a notice \hideinitializer
#define NOTICE(fmt, ...)  debugLock(); printf("N: " fmt "\n", ## __VA_ARGS__); debugUnlock()

//! print a normal message \hideinitializer
#define PRINT(fmt, ...)   debugLock(); printf("P: " fmt "\n", ## __VA_ARGS__); debugUnlock()

//! print a debug message \hideinitializer
#define DEBUG(fmt, ...)   debugLock(); printf("D: " fmt "\n", ## __VA_ARGS__); debugUnlock()

//! hex dump data
void HEXDUMP(const void *pkData, int size);



#endif // __DEBUG_H__
