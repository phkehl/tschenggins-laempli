/*!
    \file
    \brief flipflip's Tschenggins Lämpli: debugging output and other handy stuff (see \ref USER_STUFF)

    - Copyright (c) 2017 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

    \defgroup USER_STUFF STUFF
    \ingroup USER

    This implements a debugging output and other handy stuff. The debugging output is buffered and
    non-blocking (uses interrupts and the UART hardware FIFO, drops output if the buffer is full).

    Also, this header file includes most of the ESP8266 SDK and some useful c standard library
    interfaces.

    @{
*/
#ifndef __USER_STUFF_H__
#define __USER_STUFF_H__

/* ***** standard library ************************************************************************ */

#include <math.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdbool.h>
#include <ctype.h>
#include <stdarg.h>
#include <stddef.h>

/* ***** ESP SDK ********************************************************************************* */

#include <os_type.h>
#include <osapi.h>
#include <gpio.h>
#include <user_interface.h>
#include <ets_sys.h>
#include <mem.h>
#include <espconn.h>
#include <json/jsonparse.h>
#include <sntp.h>
#include <ping.h>
#include <spi_flash.h>

//! ESP8266 base address for flash mapped for code execution (I think)
#define ESP_FLASH_BASE 0x40200000

//! size of mapped flash region (apparently one MB can be mapped for code execution)
#define ESP_FLASH_SIZE   0x100000

//! ESP8266 base address for the data RAM
#define ESP_DRAM_BASE  0x3ffe8000

//! ESP8266 size of the data RAM (80kb)
#define ESP_DRAM_SIZE     0x14000

//! evaluates to true of the given address is in the data RAM \hideinitializer
#define ESP_IS_IN_DRAM(p) ( ((const void *)(p) < (const void *)ESP_DRAM_BASE) || ((const void *)(p) > (const void *)(ESP_DRAM_BASE + ESP_DRAM_SIZE)) ? false : true )


/* ***** stuff *********************************************************************************** */

//! initialise stuff
void stuffInit(void);

//! print stuff status via DEBUG()
void stuffStatus(void);


/* ***** debug helpers *************************************************************************** */

/*!
    \name Debug Output
    @{

    The DEBUG(), WARNING() etc. macros provided below automatically put the format argument into ROM
    space. They wrap around printf_PP(), which is a printf() variant that can handle string
    arguments in ROM space. Note that it doesn't handle padding for strings (e.g. "%-20s").

    The \c tools/debug.pl script can be used to receiver and display this debug output (in colours!).

    Configuration:
    - #USER_DEBUG_TXBUFSIZE
    - #USER_DEBUG_UART
    - #USER_DEBUG_USE_ISR

*/

//! print an error message \hideinitializer
#define ERROR(fmt, ...)   printf_PP(PSTR("E: " fmt "\n"), ## __VA_ARGS__)

//! print a warning message \hideinitializer
#define WARNING(fmt, ...) printf_PP(PSTR("W: " fmt "\n"), ## __VA_ARGS__)

//! print a notice \hideinitializer
#define NOTICE(fmt, ...)  printf_PP(PSTR("N: " fmt "\n"), ## __VA_ARGS__)

//! print a normal message \hideinitializer
#define PRINT(fmt, ...)   printf_PP(PSTR("P: " fmt "\n"), ## __VA_ARGS__)

//! print a debug message \hideinitializer
#define DEBUG(fmt, ...)   printf_PP(PSTR("D: " fmt "\n"), ## __VA_ARGS__)

//! like os_printf() but can handle fmt and string (%s) arguments in ROM,
/*!
    \note padding of ROM strings doesn't work (e.g. "%-20s")
*/
void printf_PP(const char *fmt, ...);

//! hex dump data
void hexdump(const void *pkData, int size);

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
#define CS_ENTER do { wdt_feed(); csEnter();
//! leave a critical section, re-enabling the interrupts if necessary \hideinitializer
#define CS_LEAVE csLeave(); } while (0)
//@}

void csEnter(void);
void csLeave(void);

/* ***** handy functions ************************************************************************* */

/*!
    \name Funky Functions
    @{
*/

//! start measuring time
void tic(const uint8_t reg);

//! stop measuring time and return measured time
uint32_t toc(const uint8_t reg);

//! get a system name up to length size-1
uint8_t getSystemName(char *name, const uint8_t size);

//! returns a per-chip unique system ID string
const char *getSystemId(void);

//! like strcasestr(), but with a slightly different name to avoid (or increase?) mixup
char *strCaseStr(const char *s1, const char *s2);

//! error string for espconn API error codes
const char *espconnErrStr(const int err);

//! error string for wifi API error codes
const char *wifiErrStr(const int err);

//! in-place URL decode
void urlDecode(char *str);

//! URL encode
bool urlEncode(const char *str, char *dst, const int dstSize);

//! HTML attribute encoding
bool attrEncode(const char *str, char *dst, const int dstSize);

//! parse hex number, strtol() doesn't seem to work.. :-(
bool parseHex(const char *hexStr, uint32_t *val);


//@}

/* ***** memory allocation *********************************************************************** */

/*!
    \name Heap Memory Functions
    @{
*/
//! allocate memory
void *memAlloc(size_t size);
//! free memory
void memFree(void *pMem);
//! get minimum free heap size
uint32_t memGetMinFree(void);
//! get current free heap size
uint32_t memGetFree(void);
//! get maximum free heap size
uint32_t memGetMaxFree(void);
//! get number of chunks currently allocated
int memGetNumAlloc(void);
//@}

/* ***** rom data stuff ************************************************************************** */

// sprintf() that can use format and string argument from ROM (and RAM)
// note: padding of ROM strings doesn't work!
int sprintf_PP(char *str, const char *fmt, ...);

// see pgmspace.h for more
#include "pgmspace.h"


/* ***** NodeMCU pins **************************************************************************** */

/*!
    \name NodeMCU / ESP8266 Pins
    @{
*/

#define GPIO_DIR_ALL(pins) GPIO_REG_WRITE( (GPIO_ENABLE_ADDRESS),      (pins) & GPIO_OUT_W1TC_DATA_MASK )
#define GPIO_DIR_SET(pins) GPIO_REG_WRITE( (GPIO_ENABLE_W1TS_ADDRESS), (pins) & GPIO_OUT_W1TC_DATA_MASK ) // set output
#define GPIO_DIR_CLR(pins) GPIO_REG_WRITE( (GPIO_ENABLE_W1TC_ADDRESS), (pins) & GPIO_OUT_W1TC_DATA_MASK ) // set input

#define GPIO_OUT_ALL(pins) GPIO_REG_WRITE( (GPIO_OUT_ADDRESS),         (pins) & GPIO_OUT_W1TC_DATA_MASK )
#define GPIO_OUT_SET(pins) GPIO_REG_WRITE( (GPIO_OUT_W1TS_ADDRESS),    (pins) & GPIO_OUT_W1TC_DATA_MASK )
#define GPIO_OUT_CLR(pins) GPIO_REG_WRITE( (GPIO_OUT_W1TC_ADDRESS),    (pins) & GPIO_OUT_W1TC_DATA_MASK )

#define GPIO_IN_READ(pin)  (gpio_input_get() & (pin) ? true : false)


#define GPIO00 BIT0
#define GPIO01 BIT1
#define GPIO02 BIT2
#define GPIO03 BIT3
#define GPIO04 BIT4
#define GPIO05 BIT5
//#define GPIO06 BIT6
//#define GPIO07 BIT7
//#define GPIO08 BIT8
//#define GPIO09 BIT9
//#define GPIO10 BIT10
//#define GPIO11 BIT11
#define GPIO12 BIT12
#define GPIO13 BIT13
#define GPIO14 BIT14
#define GPIO15 BIT15
#define GPIO16 BIT16

// NodeMCU
#define PIN_D0  GPIO16
#define PIN_D1  GPIO05
#define PIN_D2  GPIO04
#define PIN_D3  GPIO00
#define PIN_D4  GPIO02
#define PIN_D5  GPIO14
#define PIN_D6  GPIO12
#define PIN_D7  GPIO13
#define PIN_D8  GPIO15
#define PIN_D9  GPIO03
#define PIN_D10 GPIO01

#define GPIO_ENA_PIN_D1(x) PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO5_U, FUNC_GPIO5)
#define GPIO_ENA_PIN_D2(x) PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO4_U, FUNC_GPIO4)
#define GPIO_ENA_PIN_D3(x) PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO0_U, FUNC_GPIO0)
#define GPIO_ENA_PIN_D4(x) PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_GPIO2)
//@}

/* *********************************************************************************************** */

#endif // __USER_STUFF_H__
//@}
// eof
