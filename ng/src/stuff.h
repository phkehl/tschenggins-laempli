/*!
    \file
    \brief flipflip's Tschenggins Lämpli: debugging output and other handy stuff (see \ref FF_STUFF)

    - Copyright (c) 2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

    This implements a debugging output and other handy stuff. The debugging output is buffered and
    non-blocking (uses interrupts and the UART hardware FIFO, drops output if the buffer is full).

    Also, this header file includes most of the ESP8266 SDK and some useful c standard library
    interfaces.

    \defgroup FF_STUFF STUFF
    \ingroup FF

    @{
*/
#ifndef __STUFF_H__
#define __STUFF_H__

// standard library (libc/xtensa-lx106-elf/include/)
#include <stdlib.h>
#include <math.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>

// ESP SDK (include/)
#include <espressif/esp_common.h>

// FreeRTOS (FreeRTOS/Source/include/)
#include <FreeRTOS.h>
#include <task.h>

// open RTOS (core/include/)
#include <common_macros.h>
#include <esp/registers.h>
#include <esp/interrupts.h>
#include <esp/iomux.h>
#include <esp/gpio.h>
#include <esp/timer.h>


//! initialise stuff
void stuffInit(void);


/* ***** debug helpers *************************************************************************** */

/*!
    \name Debug Output
    @{

    The \c tools/debug.pl script can be used to receiver and display this debug output (in colours!).
*/
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

//@}


/* ***** handy macros **************************************************************************** */

/*!
    \name Handy Macros
    @{
*/

//#define BIT(bit) (1<<(bit))   //!< bit
#ifndef UNUSED
#  define UNUSED(foo) (void)foo //!< unused variable
#endif
#ifndef NULL
#  define NULL (void *)0    //!< null pointer     \hideinitializer
#endif /* NULL */
#define NUMOF(x) (sizeof(x)/sizeof(*(x)))       //!< number of elements in vector     \hideinitializer
#define ENDLESS true          //!< for endless while loops     \hideinitializer
#define FALLTHROUGH           //!< switch fall-through marker     \hideinitializer
#define __PAD(n) uint8_t __PADNAME(__LINE__)[n]  //!< struct padding macro     \hideinitializer
#define __PADFILL { 0 }           //!< to fill const padding     \hideinitializer
#define MIN(a, b)  ((b) < (a) ? (b) : (a)) //!< smaller value of a and b     \hideinitializer
#define MAX(a, b)  ((b) > (a) ? (b) : (a)) //!< bigger value of a and b     \hideinitializer
#define ABS(a) ((a) > 0 ? (a) : -(a)) //!< absolute value     \hideinitializer
#define CLIP(x, a, b) ((x) <= (a) ? (a) : ((x) >= (b) ? (b) : (x))) //!< clip value in range [a:b]     \hideinitializer
#define STRINGIFY(x) _STRINGIFY(x) //!< stringify argument     \hideinitializer
#define CONCAT(a, b) _CONCAT(a, b) //!< concatenate arguments     \hideinitializer
#define SWAP2(x) ( (( (x) >>  8)                                                             | ( (x) <<  8)) )
#define SWAP4(x) ( (( (x) >> 24) | (( (x) & 0x00FF0000) >>  8) | (( (x) & 0x0000FF00) <<  8) | ( (x) << 24)) )
//@}

#ifndef __DOXYGEN__
#  define _STRINGIFY(x) #x
#  define _CONCAT(a, b)  a ## b
#  define ___PADNAME(x) __pad##x
#  define __PADNAME(x) ___PADNAME(x)
#endif

//! \name Compiler Hints etc.
//@{
#define __PURE()              __attribute__ ((pure))          //!< pure \hideinitializer
#define __IRQ()               __attribute__ ((interrupt))     //!< irq \hideinitializer
#define __WEAK()              __attribute__ ((weak))          //!< weak \hideinitializer
#define __PACKED              __attribute__ ((packed))        //!< packed \hideinitializer
#define __ALIGN(n)            __attribute__ ((aligned (n)))   //!< align \hideinitializer
#ifdef __INLINE
#  undef __INLINE
#endif
#define __INLINE              inline                                 //!< inline \hideinitializer
#define __NOINLINE            __attribute__((noinline))              //!< no inline \hideinitializer
#define __FORCEINLINE         __attribute__((always_inline)) inline  //!< force inline (also with -Os) \hideinitializer
#define __USED                __attribute__((used))                  //!< used \hideinitializer
#define __NORETURN            __attribute__((noreturn))              //!< no return \hideinitializer
#define __PRINTF(six, aix)    __attribute__((format(printf, six, aix))) //!< printf() style func \hideinitializer
#define __SECTION(sec)        __attribute__((section (STRINGIFY(sec)))); //!< place symbol in section \hideinitializer
#define __NAKED               __attribute__((naked))                 //!< naked function \hideinitializer
//@}

/* ***** critical sections *********************************************************************** */

/*!
    \name Critical Sections
    @{
*/
//! enter a critical section, asserting that interrupts are off \hideinitializer
#define CS_ENTER do { taskENTER_CRITICAL();
//! leave a critical section, re-enabling the interrupts if necessary \hideinitializer
#define CS_LEAVE taskLEAVE_CRITICAL } while (0)
//@}



/* *********************************************************************************************** */

#endif // __STUFF_H__
