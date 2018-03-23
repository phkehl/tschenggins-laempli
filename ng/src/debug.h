/*!
    \file
    \brief flipflip's Tschenggins Lämpli: debugging output (see \ref FF_DEBUG)

    - Copyright (c) 2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

    This implements buffered and non-blocking debugging output (uses interrupts and the UART
    hardware FIFO, drops output if the buffer is full).

    \defgroup FF_DEBUG DEBUG
    \ingroup FF

    @{
*/
#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <stdio.h>

//! initialise debugging output
void debugInit(void);

//! print an error message \hideinitializer
#define ERROR(fmt, ...)   printf("E: " fmt "\n", ## __VA_ARGS__)

//! print a warning message \hideinitializer
#define WARNING(fmt, ...) printf("W: " fmt "\n", ## __VA_ARGS__)

//! print a notice \hideinitializer
#define NOTICE(fmt, ...)  printf("N: " fmt "\n", ## __VA_ARGS__)

//! print a normal message \hideinitializer
#define PRINT(fmt, ...)   printf("P: " fmt "\n", ## __VA_ARGS__)

//! print a debug message \hideinitializer
#define DEBUG(fmt, ...)   printf("D: " fmt "\n", ## __VA_ARGS__)

//! hex dump data
void HEXDUMP(const void *pkData, int size);



#endif // __DEBUG_H__
