/*!
    \file
    \brief flipflip's Tschenggins Lämpli: debugging output (see \ref FF_DEBUG)

    - Copyright (c) 2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli
*/

#include "stdinc.h"

#include <stdout_redirect.h>
#include <user_exception.h>
#include <esp/uart.h>
#include <esp/interrupts.h>

#include "stuff.h"
#include "mon.h"
#include "debug.h"

#define UART_NUM 0
#define TXBUF_SIZE 4096

#if (TXBUF_SIZE <= 0)
// blocking, unbuffered

IRAM static ssize_t sWriteStdoutFunc(struct _reent *r, int fd, const void *ptr, size_t len)
{
    const char *pkBuf = (const char *)ptr;
    for (int i = 0; i < len; i++)
    {
        uart_putc(0, *pkBuf++);
    }
    return len;
}

void debugLock(void)
{
}

void debugUnlock(void)
{
}


#else // (TXBUF_SIZE <= 0)
// non-blocking, buffered

static volatile char     svDebugBuf[TXBUF_SIZE];       // debug buffer
static volatile uint16_t svDebugBufHead;               // write-to-buffer pointer (index)
static volatile uint16_t svDebugBufTail;               // read-from-buffer pointer (index)
static volatile uint16_t svDebugBufSize;               // size of buffered data
static volatile uint16_t svDebugBufPeak;               // peak output buffer size
static volatile uint16_t svDebugBufDrop;               // number of dropped bytes

// add stdio output data to buffer
static ssize_t sWriteStdoutFunc(struct _reent *r, int fd, const void *ptr, size_t len)
{
    CS_ENTER;

    const char *pkBuf = (const char *)ptr;
    for (int i = 0; i < len; i++)
    {

        // add to ring buffer if there's space
        if ( (svDebugBufSize == 0) || (svDebugBufHead != svDebugBufTail) )
        {
            svDebugBuf[svDebugBufHead] = *pkBuf;
            svDebugBufHead += 1;
            svDebugBufHead %= sizeof(svDebugBuf);
            svDebugBufSize++;
            // keep statistics on the buffer size
            if (svDebugBufSize > svDebugBufPeak)
            {
                svDebugBufPeak = svDebugBufSize;
            }
        }
        // drop char otherwise
        else
        {
            // FIXME: put "\nE: tx buf\n" into buffer
            svDebugBufDrop++;
        }

        pkBuf++;
    }

    CS_LEAVE;

    // enable tx fifo empty interrupt
    UART(UART_NUM).INT_ENABLE |= UART_INT_ENABLE_TXFIFO_EMPTY;

    return len;
}


// interrupt handler (for *any* UART interrupt of *any* UART peripheral)
// flushs buffered debug data to the tx fifo
IRAM static void sUartISR(void *pArg) // RAM function
{
    monIsrEnter();

    //UNUSED(pArg);

    // is it the tx fifo empty interrupt?
    if (UART(UART_NUM).INT_STATUS & UART_INT_STATUS_TXFIFO_EMPTY)
    {
        // disable interrupt
        UART(UART_NUM).INT_ENABLE &= ~UART_INT_ENABLE_TXFIFO_EMPTY;

        // write more data to the UART tx FIFO
        uint8_t fifoRemaining = (UART_FIFO_MAX + 1) - FIELD2VAL(UART_STATUS_TXFIFO_COUNT, UART(UART_NUM).STATUS);
        while (svDebugBufSize && fifoRemaining--)
        {
            const char c = svDebugBuf[svDebugBufTail];
            svDebugBufTail += 1;
            svDebugBufTail %= sizeof(svDebugBuf);
            svDebugBufSize--;
            UART(UART_NUM).FIFO = (c & UART_FIFO_DATA_M) << UART_FIFO_DATA_S;
            //UART(UART_NUM).FIFO = SET_FIELD_M(UART(UART_NUM).FIFO, UART_FIFO_DATA, c);
        }

        // there's more data to fill to the FIFO once it's empty
        if (svDebugBufSize)
        {
            UART(UART_NUM).INT_ENABLE |= UART_INT_ENABLE_TXFIFO_EMPTY;
        }

        // clear interrupt
        UART(UART_NUM).INT_CLEAR = UART_INT_CLEAR_TXFIFO_EMPTY;
    }
    // else if (...) // handle other sources of this interrupt

    monIsrLeave();
}

static SemaphoreHandle_t sDebugMutex;

__INLINE void debugLock(void)
{
    xSemaphoreTake(sDebugMutex, portMAX_DELAY);
}

__INLINE void debugUnlock(void)
{
    xSemaphoreGive(sDebugMutex);
}


#endif // (TXBUF_SIZE <= 0)


void debugMonStatus(void)
{
#if (TXBUF_SIZE > 0)
    uint16_t size, peak, drop;
    CS_ENTER;
    size = svDebugBufSize;
    peak = svDebugBufPeak;
    drop = svDebugBufDrop;
    svDebugBufPeak = 0;
    svDebugBufDrop = 0;
    CS_LEAVE;
    uint16_t percPeak = ((peak * 8 * 100 / sizeof(svDebugBuf)) + 4) >> 3;
    if (drop)
    {
        WARNING("mon: debug: size=%u/%u peak=%u (%u%%) drop=%u",
            size, sizeof(svDebugBuf), peak, percPeak, drop);
    }
    else
    {
        DEBUG("mon: debug: size=%u/%u peak=%u (%u%%) drop=%u",
            size, sizeof(svDebugBuf), peak, percPeak, drop);
    }
#endif
}


// turn off buffered output, for exceptions
// https://github.com/SuperHouse/esp-open-rtos/wiki/Crash-Dumps
// to test: *((volatile uint32_t *)0) = 0;
static void sDebugResetStdout(void)
{
    // dump what's in the buffer
    while (svDebugBufSize > 0)
    {
        const char c = svDebugBuf[svDebugBufTail];
        svDebugBufTail += 1;
        svDebugBufTail %= sizeof(svDebugBuf);
        svDebugBufSize--;
        uart_putc(0, c);
    }

    // revert back to blocking direct-to-UART stdout
    set_write_stdout(NULL);
}

void debugInit(void)
{
    // 115200 8N1
    uart_set_baud(UART_NUM, 115200);
    uart_set_parity_enabled(UART_NUM, false);
    uart_set_stopbits(UART_NUM, UART_STOPBITS_1);

    printf("..................................................\n");
    osSleep(250);

    static StaticSemaphore_t sMutex;
    sDebugMutex = xSemaphoreCreateMutexStatic(&sMutex);

#if (TXBUF_SIZE > 0)

    // clear tx fifo
    uart_clear_txfifo(UART_NUM);

    DEBUG("debug: init buf=%u fifo=%u", sizeof(svDebugBuf), UART_FIFO_MAX);

    set_write_stdout(sWriteStdoutFunc);
    //stderr = stdout; // FIXME: makes the system freeze (or stops output?) at some point.. :-(

    // attach UART ISR
    _xt_isr_attach(INUM_UART, sUartISR, NULL);

    // configure tx fifo empty threshold
    UART(UART_NUM).CONF1 = SET_FIELD_M(UART(UART_NUM).CONF1, UART_CONF1_TXFIFO_EMPTY_THRESHOLD, 16);

    // disable all UART interrupt sources (this seems to be crucial!) and enable UART interrupt
    UART(UART_NUM).INT_ENABLE = 0;
    UART(UART_NUM).INT_CLEAR = ~0x0;

    // unmask (enable) UART interrupts
    _xt_isr_unmask(BIT(INUM_UART));

    set_user_exception_handler(sDebugResetStdout);

#else

    DEBUG("debug: init (blocking, unbuffered)");
    //sdk_os_install_putc1(sPutcFunc); // nope!
    set_write_stdout(sWriteStdoutFunc);

#endif
}

void HEXDUMP(const void *pkData, int size)
{
    const char hexdigits[] = "0123456789abcdef";
    const char *data = pkData;
    for (int ix = 0; ix < size; )
    {
        char buf[128];
        memset(buf, ' ', sizeof(buf));
        for (int ix2 = 0; ix2 < 16; ix2++)
        {
            const uint8_t c = data[ix + ix2];
            buf[3 * ix2    ] = hexdigits[ (c >> 4) & 0xf ];
            buf[3 * ix2 + 1] = hexdigits[  c       & 0xf ];
            buf[3 * 16 + 2 + ix2] = isprint((int)c) ? c : '.';
            buf[3 * 16 + 3 + ix2] = '\0';
        }
        DEBUG("0x%08x  %s", (uint32_t)data + ix, buf);
        ix += 16;
    }
}


// eof
