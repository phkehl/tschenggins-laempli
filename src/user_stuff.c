/*!
    \file
    \brief flipflip's Tschenggins Lämpli: debugging output and other handy stuff (see \ref USER_STUFF)

    - Copyright (c) 2017 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

    \addtogroup USER_STUFF

    \todo move debugging stuff to separate files

    @{
*/

#include <alloca.h>

#include "user_stuff.h"
#include "version_gen.h"


#define NUM_REG 5

static uint32_t sTicTocRegs[NUM_REG];

void ICACHE_FLASH_ATTR tic(const uint8_t reg)
{
    if (reg < NUM_REG)
    {
        sTicTocRegs[reg] = system_get_time();
    }
}

uint32_t ICACHE_FLASH_ATTR toc(const uint8_t reg)
{
    uint32_t delta = 0;
    if (reg < NUM_REG)
    {
        delta = system_get_time() - sTicTocRegs[reg];
    }
    return (delta + 500) / 1000;
}

static volatile uint32_t svCriticalNesting = 0;

void csEnter(void) // RAM function
{
    if (!svCriticalNesting)
    {
        ETS_INTR_LOCK();
    }
    svCriticalNesting++;

}

void csLeave(void) // RAM function
{
    svCriticalNesting--;
    if (svCriticalNesting == 0)
    {
        ETS_INTR_UNLOCK();
    }
}

const char * ICACHE_FLASH_ATTR espconnErrStr(const int err)
{
    static char errStr[4];
    if ( (err >= -99) && (err <= 999) )
    {
        os_sprintf(errStr, "%d", 3, err);
    }
    else
    {
        errStr[0] = '?';
        errStr[1] = '?';
        errStr[2] = '?';
        errStr[3] = '\0';
    }
    const char *pkErrStr = errStr;
    switch (err)
    {
        case ESPCONN_OK:               pkErrStr = PSTR("OK");                    break;
        case ESPCONN_MEM:              pkErrStr = PSTR("mem alloc fail");        break;
        case ESPCONN_TIMEOUT:          pkErrStr = PSTR("timeout");               break;
        case ESPCONN_RTE:              pkErrStr = PSTR("routing problem");       break;
        case ESPCONN_INPROGRESS:       pkErrStr = PSTR("operation in progress"); break;
        case ESPCONN_MAXNUM:           pkErrStr = PSTR("maximum exceeded");      break;
        case ESPCONN_ABRT:             pkErrStr = PSTR("connectedion aborted");  break;
        case ESPCONN_RST:              pkErrStr = PSTR("connection reset");      break;
        case ESPCONN_CLSD:             pkErrStr = PSTR("connection closed");     break;
        case ESPCONN_CONN:             pkErrStr = PSTR("not connected");         break;
        case ESPCONN_ARG:              pkErrStr = PSTR("illegal argument");      break;
        case ESPCONN_IF:               pkErrStr = PSTR("UDP send error");        break;
        case ESPCONN_ISCONN:           pkErrStr = PSTR("already connected");     break;
        case ESPCONN_HANDSHAKE:        pkErrStr = PSTR("handshake fail");        break;
        case ESPCONN_SSL_INVALID_DATA: pkErrStr = PSTR("SSL data invalid");      break;
    }
    return pkErrStr;
}

const char * ICACHE_FLASH_ATTR wifiErrStr(const int err)
{
    const char *pkErrStr = NULL;
    switch (err)
    {
        case REASON_UNSPECIFIED:              pkErrStr = PSTR("UNSPECIFIED"); break;
        case REASON_AUTH_EXPIRE:              pkErrStr = PSTR("AUTH_EXPIRE"); break;
        case REASON_AUTH_LEAVE:               pkErrStr = PSTR("AUTH_LEAVE"); break;
        case REASON_ASSOC_EXPIRE:             pkErrStr = PSTR("ASSOC_EXPIRE"); break;
        case REASON_ASSOC_TOOMANY:            pkErrStr = PSTR("ASSOC_TOOMANY"); break;
        case REASON_NOT_AUTHED:               pkErrStr = PSTR("NOT_AUTHED"); break;
        case REASON_NOT_ASSOCED:              pkErrStr = PSTR("NOT_ASSOCED"); break;
        case REASON_ASSOC_LEAVE:              pkErrStr = PSTR("ASSOC_LEAVE"); break;
        case REASON_ASSOC_NOT_AUTHED:         pkErrStr = PSTR("ASSOC_NOT_AUTHED"); break;
        case REASON_DISASSOC_PWRCAP_BAD:      pkErrStr = PSTR("DISASSOC_PWRCAP_BAD"); break;
        case REASON_DISASSOC_SUPCHAN_BAD:     pkErrStr = PSTR("DISASSOC_SUPCHAN_BAD"); break;
        case REASON_IE_INVALID:               pkErrStr = PSTR("IE_INVALID"); break;
        case REASON_MIC_FAILURE:              pkErrStr = PSTR("MIC_FAILURE"); break;
        case REASON_4WAY_HANDSHAKE_TIMEOUT:   pkErrStr = PSTR("4WAY_HANDSHAKE_TIMEOUT"); break;
        case REASON_GROUP_KEY_UPDATE_TIMEOUT: pkErrStr = PSTR("GROUP_KEY_UPDATE_TIMEOUT"); break;
        case REASON_IE_IN_4WAY_DIFFERS:       pkErrStr = PSTR("IE_IN_4WAY_DIFFERS"); break;
        case REASON_GROUP_CIPHER_INVALID:     pkErrStr = PSTR("GROUP_CIPHER_INVALID"); break;
        case REASON_PAIRWISE_CIPHER_INVALID:  pkErrStr = PSTR("PAIRWISE_CIPHER_INVALID"); break;
        case REASON_AKMP_INVALID:             pkErrStr = PSTR("AKMP_INVALID"); break;
        case REASON_UNSUPP_RSN_IE_VERSION:    pkErrStr = PSTR("UNSUPP_RSN_IE_VERSION"); break;
        case REASON_INVALID_RSN_IE_CAP:       pkErrStr = PSTR("INVALID_RSN_IE_CAP"); break;
        case REASON_802_1X_AUTH_FAILED:       pkErrStr = PSTR("802_1X_AUTH_FAILED"); break;
        case REASON_CIPHER_SUITE_REJECTED:    pkErrStr = PSTR("CIPHER_SUITE_REJECTED"); break;
        case REASON_BEACON_TIMEOUT:           pkErrStr = PSTR("BEACON_TIMEOUT"); break;
        case REASON_NO_AP_FOUND:              pkErrStr = PSTR("NO_AP_FOUND"); break;
        case REASON_AUTH_FAIL:                pkErrStr = PSTR("AUTH_FAIL"); break;
        case REASON_ASSOC_FAIL:               pkErrStr = PSTR("ASSOC_FAIL"); break;
        case REASON_HANDSHAKE_TIMEOUT:        pkErrStr = PSTR("HANDSHAKE_TIMEOUT"); break;
    }
    if (pkErrStr == NULL)
    {
        static char errStr[4];
        if ( (err >= -99) && (err <= 999) )
        {
            os_sprintf(errStr, "%d", 3, err);
        }
        else
        {
            errStr[0] = '?';
            errStr[1] = '?';
            errStr[2] = '?';
            errStr[3] = '\0';
        }
        pkErrStr = errStr;
    }
    return pkErrStr;
}


// in-place urldecode
void ICACHE_FLASH_ATTR urlDecode(char *str)
{
    int len = os_strlen(str) - 2;
    int ix = 0;
    while (ix < len)
    {
        // percent-decode (https://en.wikipedia.org/wiki/Percent-encoding)
        if ( (str[ix] == '%') && isxdigit((int)str[ix + 1]) && isxdigit((int)str[ix + 2]) )
        {
            // first hex digit
            const char a = str[ix + 1];
            char val = 0;
            if      (a >= 'a') { val += (a - 'a' + 10) * 16; }
            else if (a >= 'A') { val += (a - 'A' + 10) * 16; }
            else               { val += (a - '0'     ) * 16; }
            // second hex digit
            const char b = str[ix + 2];
            if      (b >= 'a') { val += (b - 'a' + 10); }
            else if (b >= 'A') { val += (b - 'A' + 10); }
            else               { val += (b - '0'     ); }
            // replace %ab with actual char (but don't allow '\0')
            str[ix] = val != '\0' ? val : ' ';
            // move remaining chars
            len += 1;
            for (int ix2 = ix + 1; ix2 < len; ix2++)
            {
                str[ix2] = str[ix2 + 2];
            }
            // update str length
            len = os_strlen(str) - 2;
        }
        // '+' means ' ' (space)
        else if (str[ix] == '+')
        {
            str[ix] = ' ';
        }
        ix++;
    }
}

bool ICACHE_FLASH_ATTR urlEncode(const char *str, char *dst, const int dstSize)
{
    int size = 3; // "%ab"
    while ( (*str != '\0') && (size < dstSize) )
    {
        const char c = *str++;
        switch (c)
        {
            case ' ':
                *dst++ = '+';
                size++;
                break;
            case '!': case '#': case '$': case '&': case '\'': case '(': case ')': case '*': case '+':
            case ',': case '/': case ':': case ';': case '=': case '?': case '@': case '[': case ']':
            {
                *dst++ = '%';
                const char n1 = (c & 0xf0) >> 4;
                const char n2 =  c & 0x0f;
                const char c1 = n1 +  ( n1 > 0x09 ? '7' : '0' );
                const char c2 = n2 +  ( n2 > 0x09 ? '7' : '0' );
                //DEBUG("c=%c 0x%02x n1=0x%x n2=0x%x c1=%c c2=%c", c, c, n1, n2, c1, c2);
                *dst++ = c1;
                *dst++ = c2;
                size += 3;
                break;
            }
            default:
                *dst++ = c;
                size++;
                break;
        }
    }
    *dst = '\0';

    return *str == '\0' ? true : false;
}

bool ICACHE_FLASH_ATTR attrEncode(const char *str, char *dst, const int dstSize)
{
    int size = 6; // "&quot;"
    while ( (*str != '\0') && (size < dstSize) )
    {
        const char c = *str++;
        switch (c)
        {
            case '<':
            {
                *dst++ = '&'; *dst++ = 'l'; *dst++ = 't'; *dst++ = ';';
                size += 4;
                break;
            }
            case '>':
            {
                *dst++ = '&'; *dst++ = 'g'; *dst++ = 't'; *dst++ = ';';
                size += 4;
                break;
            }
            case '"':
            {
                *dst++ = '&'; *dst++ = 'q'; *dst++ = 'u'; *dst++ = 'o'; *dst++ = 't'; *dst++ = ';';
                size += 6;
                break;
            }
            case '&':
            {
                *dst++ = '&'; *dst++ = 'a'; *dst++ = 'm'; *dst++ = 'p'; *dst++ = ';';
                size += 5;
                break;
            }
            default:
                *dst++ = c;
                size++;
                break;
        }
    }
    *dst = '\0';

    return *str == '\0' ? true : false;
}

bool ICACHE_FLASH_ATTR parseHex(const char *hexStr, uint32_t *val)
{
    int shift = os_strlen(hexStr) - 1;
    if (shift < 0)
    {
        return false;
    }
    uint32_t value = 0;
    bool res = true;
    while (*hexStr != '\0')
    {
        const char c = *hexStr;
        if (isxdigit((int)c))
        {
            const uint32_t v = (c >= 'a') ? (c - 'a' + 10) : ( (c >= 'A') ? (c - 'A' + 10) : (c - '0') );
            value += v * (1 << (4 * shift));
        }
        else
        {
            res = false;
            break;
        }
        hexStr++;
        shift--;
    }
    if (res)
    {
        *val = value;
    }
    return res;
}



// based on strstr() in the Public Domain C Library (PDCLib)
char * ICACHE_FLASH_ATTR strCaseStr(const char *s1, const char *s2)
{
    const char *p1 = s1;
    const char *p2;
    while ( *s1 )
    {
        p2 = s2;
        while ( *p2 && ( tolower((int)(*p1)) == tolower((int)(*p2)) ) )
        {
            ++p1;
            ++p2;
        }
        if ( ! *p2 )
        {
            return (char *) s1;
        }
        ++s1;
        p1 = s1;
    }
    return NULL;
}

static int sMemAllocCount;
static uint32_t sMemFreeMin;
static uint32_t sMemFreeMax;

uint32_t memGetFree(void)
{
    const uint32_t free = system_get_free_heap_size();
    if ( (sMemFreeMin == 0) || (free < sMemFreeMin) )
    {
        sMemFreeMin = free;
    }
    if (free > sMemFreeMax)
    {
        sMemFreeMax = free;
    }
    return free;
}

void * ICACHE_FLASH_ATTR memAlloc(const uint32_t size)
{
    void *pMem = os_malloc(size);
    if (pMem)
    {
        sMemAllocCount++;
    }
    memGetFree();
    //DEBUG("memAlloc() %p", pMem);

    return pMem;
}

void ICACHE_FLASH_ATTR memFree(void *pMem)
{
    if (pMem == NULL)
    {
        return;
    }
    sMemAllocCount--;

    //DEBUG("memFree() %p", pMem);
    os_free(pMem);

    memGetFree();
}

inline uint32_t memGetMinFree(void) { return sMemFreeMin; }
inline uint32_t memGetMaxFree(void) { return sMemFreeMax; }
inline int memGetNumAlloc(void) { return sMemAllocCount; }

// -------------------------------------------------------------------------------------------------

uint8_t ICACHE_FLASH_ATTR getSystemName(char *name, const uint8_t size)
{
    os_memset(name, 0, size);
    os_strncpy(name, FF_PROJECT, size);
    name[size - 1] = '\0';
    if (size >= 12)
    {
        const int len = os_strlen(name);
        const uint32_t chipId = system_get_chip_id(); // same as default mac[3..5]
        os_sprintf(&name[ len > (int)(size - 8) ? (int)(size - 8) : len ],
            "-%06x", chipId & 0x00ffffff);
    }
    const uint8_t len = os_strlen(name);
    //DEBUG("getSystemName() %s (%u/%u)", name, len, size - 1);
    return len;
}

const char * ICACHE_FLASH_ATTR getSystemId(void)
{
    static char name[7];
    const uint32_t chipId = system_get_chip_id(); // same as default mac[3..5]
    os_sprintf(name, "%06x", chipId & 0x00ffffff);
    return name;
}

void ICACHE_FLASH_ATTR hexdump(const void *pkData, int size)
{
    const char hexdigits[] = "0123456789abcdef";

    const char *data = pkData;
    for (int ix = 0; ix < size; )
    {
        char buf[128];
        os_memset(buf, ' ', sizeof(buf));
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

/* ***** buffered UART *************************************************************************** */

// registers from SDK (driver_lib/driver/uart.c, driver_lib/{driver/uart.c,include/driver/uart.h,include/driver/uart_register.h)
// and the internets (8F-ESP8266__Interface__UART_Registers_v0.1.xlsx, 8E-ESP8266__Interface_UART__EN_v0.2.pdf)
// Copyright (c) 2016 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>

// N.B. Only some stuff is covered here. There's much more (registers, statuses, interrupts, etc.).

// UART registers
#define REG_UART_BASE(i)      (0x60000000 + ((i)*0xf00))

// UART config register 0
// 0b .... .... .... .... .... .... ..ss ddPp
// - P = parity enable
// - p = parity mode
// - d = number of data bits
// - s = number of stop bits

#define UART_CONF_0(i)        (REG_UART_BASE(i) + 0x20) // (r/w)

#define UART_PARITY_NONE      0x00000000 // 0b00 << 0
#define UART_PARITY_EVEN      0x00000002 // 0b10 << 0
#define UART_PARITY_ODD       0x00000003 // 0b11 << 0

#define UART_DATA_FIVE        0x00000000 // 0b00 << 2
#define UART_DATA_SIX         0x00000004 // 0b01 << 2
#define UART_DATA_SEVEN       0x00000008 // 0b10 << 2
#define UART_DATA_EIGHT       0x0000000c // 0b11 << 2

#define UART_STOP_ONE         0x00000010 // 0b01 << 4
#define UART_STOP_HALF        0x00000020 // 0b10 << 4
#define UART_STOP_TWO         0x00000030 // 0b11 << 4

// UART config register 1
#define UART_CONF1(i)                   (REG_UART_BASE(i) + 0x24)
// 0b .... .... .... .... .ttt tttt .... ....
// - t = tx fifo empty interrupt threshold
#define UART_TX_EMPTY_THRS_M  0x0000007f // 7 bits, 0..127
#define UART_TX_EMPTY_THRS_S  8
#define UART_TX_EMPTY_THRS(i, thrs) \
    (SET_PERI_REG_MASK(UART_CONF1(i), ((thrs) & UART_TX_EMPTY_THRS_M) << UART_TX_EMPTY_THRS_S))

// UART clock, baud rate
#define UART_CLKDIV(i)        (REG_UART_BASE(i) + 0x14) // (r/w)
#define UART_BAUD_300         ((UART_CLK_FREQ /     300) & 0x000fffff)
#define UART_BAUD_600         ((UART_CLK_FREQ /     600) & 0x000fffff)
#define UART_BAUD_1200        ((UART_CLK_FREQ /    1200) & 0x000fffff)
#define UART_BAUD_2400        ((UART_CLK_FREQ /    2400) & 0x000fffff)
#define UART_BAUD_4800        ((UART_CLK_FREQ /    4800) & 0x000fffff)
#define UART_BAUD_9600        ((UART_CLK_FREQ /    9600) & 0x000fffff)
#define UART_BAUD_19200       ((UART_CLK_FREQ /   19200) & 0x000fffff)
#define UART_BAUD_38400       ((UART_CLK_FREQ /   38400) & 0x000fffff)
#define UART_BAUD_57600       ((UART_CLK_FREQ /   57600) & 0x000fffff)
#define UART_BAUD_74880       ((UART_CLK_FREQ /   74880) & 0x000fffff)
#define UART_BAUD_115200      ((UART_CLK_FREQ /  115200) & 0x000fffff)
#define UART_BAUD_230400      ((UART_CLK_FREQ /  230400) & 0x000fffff)
#define UART_BAUD_460800      ((UART_CLK_FREQ /  460800) & 0x000fffff)
#define UART_BAUD_921600      ((UART_CLK_FREQ /  921600) & 0x000fffff)
#define UART_BAUD_1843200     ((UART_CLK_FREQ / 1843200) & 0x000fffff)
#define UART_BAUD_3686400     ((UART_CLK_FREQ / 3686400) & 0x000fffff)

// UART status register
#define UART_STATUS(i)        (REG_UART_BASE(i) + 0x1c)
#define UART_TXFIFO_CNT_M     0x000000ff
#define UART_TXFIFO_CNT_S     16
#define UART_TXFIFO_CNT(i)    ( (READ_PERI_REG(UART_STATUS(i)) >> UART_TXFIFO_CNT_S) & UART_TXFIFO_CNT_M )

// UART FIFO
#define UART_FIFO(i)          (REG_UART_BASE(i) + 0x00)
#define UART_FIFO_READ(i)     (READ_PERI_REG(UART_FIFO(i)) & 0xff)
#define UART_FIFO_WRITE(i, c) (WRITE_PERI_REG(UART_FIFO(i), (c) & 0xff))
#define UART_FIFO_SIZE        128

// UART interrupt sources
#define UART_INT_RAW(i)       (REG_UART_BASE(i) + 0x04) // UART interrupt raw state (ro)
#define UART_INT_ST(i)        (REG_UART_BASE(i) + 0x08) // UART interrupt state (_RAW & _ENA) (ro)
#define UART_INT_ENA(i)       (REG_UART_BASE(i) + 0x0c) // UART interrupt enable register (r/w)
#define UART_INT_CLR(i)       (REG_UART_BASE(i) + 0x10) // UART interrupt clear register (wo)
#define UART_TXFIFO_EMPTY     (BIT(1))

#define IS_UART_TXFIFO_EMPTY_INT(i)  ( READ_PERI_REG(UART_INT_ST(i)) & UART_TXFIFO_EMPTY ? true : false )
#define ENA_UART_TXFIFO_EMPTY_INT(i) ( SET_PERI_REG_MASK(UART_INT_ENA(i), UART_TXFIFO_EMPTY) )
#define DIS_UART_TXFIFO_EMPTY_INT(i) ( CLEAR_PERI_REG_MASK(UART_INT_ENA(i), UART_TXFIFO_EMPTY) )
#define CLR_UART_TXFIFO_EMPTY_INT(i) ( WRITE_PERI_REG(UART_INT_CLR(i), UART_TXFIFO_EMPTY) )


// -------------------------------------------------------------------------------------------------
#if (USER_DEBUG_TXBUFSIZE == 0)

#  define DEBUG_PUTC_FUNC sDebugPutcBlock
// put character into tx fifo, possibly wait until space becomes available
static void sDebugPutcBlock(char c) // RAM function
{
    // wait for space in tx fifo
    while (UART_TXFIFO_CNT(USER_DEBUG_UART) > (UART_FIFO_SIZE - 1)) { }

    // add byte to output FIFO
    UART_FIFO_WRITE(USER_DEBUG_UART, c);
}

// put character to /dev/null
//static void sDebugPutcSink(char c)
//{
//    UNUSED(c);
//}

// -------------------------------------------------------------------------------------------------

#else // USER_DEBUG_TXBUFSIZE == 0

static volatile char     svDebugBuf[USER_DEBUG_TXBUFSIZE];  // debug buffer
static volatile uint16_t svDebugBufHead;               // write-to-buffer pointer (index)
static volatile uint16_t svDebugBufTail;               // read-from-buffer pointer (index)
static volatile uint16_t svDebugBufSize;               // size of buffered data
static volatile uint16_t svDebugBufPeak;               // peak output buffer size
static volatile uint16_t svDebugBufDrop;               // number of dropped bytes

#  define DEBUG_PUTC_FUNC sDebugPutcBuff

// put character to tx buffer or drop it if the buffer is full
static void sDebugPutcBuff(char c) // RAM function
{
#if (USER_DEBUG_USE_ISR > 0)
    CS_ENTER;
#endif

    // add to ring buffer if there's space
    if ( (svDebugBufSize == 0) || (svDebugBufHead != svDebugBufTail) )
    {
        svDebugBuf[svDebugBufHead] = c;
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

#if (USER_DEBUG_USE_ISR > 0)
    CS_LEAVE;
    // enable tx fifo empty interrupt
    ENA_UART_TXFIFO_EMPTY_INT(USER_DEBUG_UART);
#endif
}

// flush buffered debug data to the tx fifo
static void sDebugFlush(void) // RAM function
{
#if (USER_DEBUG_USE_ISR > 0)
    DIS_UART_TXFIFO_EMPTY_INT(USER_DEBUG_UART);
    //CS_ENTER;
#endif

    // write more data to the UART tx FIFO
    uint8_t fifoRemaining = UART_FIFO_SIZE - UART_TXFIFO_CNT(USER_DEBUG_UART);
    while (svDebugBufSize && fifoRemaining--)
    {
        const char c = svDebugBuf[svDebugBufTail];
        svDebugBufTail += 1;
        svDebugBufTail %= sizeof(svDebugBuf);
        svDebugBufSize--;
        UART_FIFO_WRITE(USER_DEBUG_UART, c);
    }

#if (USER_DEBUG_USE_ISR > 0)
    //CS_LEAVE;
    // FIXME: only re-enable if there's more in the buffer
    if (svDebugBufSize)
    {
        ENA_UART_TXFIFO_EMPTY_INT(USER_DEBUG_UART);
    }
#endif
}

#if (USER_DEBUG_USE_ISR > 0)

// interrupt handler (for *any* UART interrupt of *any* UART peripheral)
static void sUartISR(void *pArg) // RAM function
{
    UNUSED(pArg);

    if (IS_UART_TXFIFO_EMPTY_INT(USER_DEBUG_UART))
    {
        DIS_UART_TXFIFO_EMPTY_INT(USER_DEBUG_UART);
        sDebugFlush();
        CLR_UART_TXFIFO_EMPTY_INT(USER_DEBUG_UART);
    }
    // else if (...) // handle other sources of this interrupt
}

#else // (USER_DEBUG_USE_ISR > 0)

static os_timer_t sDebugFlushTimer;
static void sDebugFlushTimerFunc(void *arg)
{
    UNUSED(arg);
    sDebugFlush();
}

#endif // (USER_DEBUG_USE_ISR > 0)

#endif // USER_DEBUG_TXBUFSIZE == 0


// -------------------------------------------------------------------------------------------------



#define PRINTF_PS_MAXARGS 40

// printf() function that can handle strings in ROM, via %S (forced) or %s (auto-detect)
void ICACHE_FLASH_ATTR printf_PP(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    // copy format so that we can modify it
    const int fmtLen = strlen_P(fmt);
    char fmtCopy[fmtLen + 1];
    strcpy_P(fmtCopy, fmt);

    // generate list of arguments
    const void *pArgs[PRINTF_PS_MAXARGS + 1];
    char *pFmt = fmtCopy;
    const char *dummy = "<dummy>";
    int nArgs = 0;

    while (nArgs < (int)NUMOF(pArgs) - 1)
    {
        const char c = *(pFmt++);
        if (c == '\0')
        {
            break;
        }
        if ( (c == '%') && ((pFmt[0] != '*') && (pFmt[0] != '%')) )
        {
            const void *pkArg = va_arg(ap, const void *);

            // replace 'S' with 's' and copy ROM string to RAM
            if ( (pFmt[0] == 'S') || ((pFmt[0] == 's') && (pkArg >= (const void *)ESP_FLASH_BASE)) )
            {
                pFmt[0] = 's';
                pArgs[nArgs] = dummy;

                int strLen = 1;
                const char *pkRom = pkArg;
                while (pgm_read_uint8(pkRom) != '\0')
                {
                    strLen++;
                    pkRom++;
                }
                char *strCopy = alloca(strLen);
                strcpy_P(strCopy, pkArg);
                pArgs[nArgs] = strCopy;
            }
            else
            {
                pArgs[nArgs] = pkArg;
            }
            nArgs++;
        }
    }
    va_end(ap);

#if 0 // doesn't work :-( this might: https://github.com/libffi/libffi
#  pragma GCC diagnostic push
#  pragma GCC diagnostic ignored "-Wvarargs"
    pArgs[nArgs] = NULL;
    va_start(ap, pArgs);
    ets_vprintf(DEBUG_PUTC_FUNC, fmtCopy, ap);

    //va_list ap2;
    //pArgs[nArgs] = NULL;
    //va_start(ap2, pArgs);
    //va_end(ap2);

    //os_printf(DEBUG_PUTC_FUNC, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23], pArgs[24], pArgs[25], pArgs[26], pArgs[27], pArgs[28], pArgs[29]);
#  pragma GCC diagnostic pop
#else
    switch (nArgs)
    {
        case  0: os_printf(fmtCopy); break;
        case  1: os_printf(fmtCopy, pArgs[0]); break;
        case  2: os_printf(fmtCopy, pArgs[0], pArgs[1]); break;
        case  3: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2]); break;
        case  4: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3]); break;
        case  5: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4]); break;
        case  6: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5]); break;
        case  7: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6]); break;
        case  8: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7]); break;
        case  9: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8]); break;
        case 10: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9]); break;
        case 11: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10]); break;
        case 12: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11]); break;
        case 13: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12]); break;
        case 14: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13]); break;
        case 15: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14]); break;
        case 16: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15]); break;
        case 17: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16]); break;
        case 18: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17]); break;
        case 19: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18]); break;
        case 20: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19]); break;
        case 21: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20]); break;
        case 22: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21]); break;
        case 23: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22]); break;
        case 24: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23]); break;
        case 25: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23], pArgs[24]); break;
        case 26: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23], pArgs[24], pArgs[25]); break;
        case 27: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23], pArgs[24], pArgs[25], pArgs[26]); break;
        case 28: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23], pArgs[24], pArgs[25], pArgs[26], pArgs[27]); break;
        case 29: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23], pArgs[24], pArgs[25], pArgs[26], pArgs[27], pArgs[28]); break;
        case 30: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23], pArgs[24], pArgs[25], pArgs[26], pArgs[27], pArgs[28], pArgs[29]); break;
        case 31: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23], pArgs[24], pArgs[25], pArgs[26], pArgs[27], pArgs[28], pArgs[29], pArgs[30]); break;
        case 32: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23], pArgs[24], pArgs[25], pArgs[26], pArgs[27], pArgs[28], pArgs[29], pArgs[30], pArgs[31]); break;
        case 33: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23], pArgs[24], pArgs[25], pArgs[26], pArgs[27], pArgs[28], pArgs[29], pArgs[30], pArgs[31], pArgs[32]); break;
        case 34: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23], pArgs[24], pArgs[25], pArgs[26], pArgs[27], pArgs[28], pArgs[29], pArgs[30], pArgs[31], pArgs[32], pArgs[33]); break;
        case 35: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23], pArgs[24], pArgs[25], pArgs[26], pArgs[27], pArgs[28], pArgs[29], pArgs[30], pArgs[31], pArgs[32], pArgs[33], pArgs[34]); break;
        case 36: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23], pArgs[24], pArgs[25], pArgs[26], pArgs[27], pArgs[28], pArgs[29], pArgs[30], pArgs[31], pArgs[32], pArgs[33], pArgs[34], pArgs[35]); break;
        case 37: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23], pArgs[24], pArgs[25], pArgs[26], pArgs[27], pArgs[28], pArgs[29], pArgs[30], pArgs[31], pArgs[32], pArgs[33], pArgs[34], pArgs[35], pArgs[36]); break;
        case 38: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23], pArgs[24], pArgs[25], pArgs[26], pArgs[27], pArgs[28], pArgs[29], pArgs[30], pArgs[31], pArgs[32], pArgs[33], pArgs[34], pArgs[35], pArgs[36], pArgs[37]); break;
        case 39: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23], pArgs[24], pArgs[25], pArgs[26], pArgs[27], pArgs[28], pArgs[29], pArgs[30], pArgs[31], pArgs[32], pArgs[33], pArgs[34], pArgs[35], pArgs[36], pArgs[37], pArgs[38]); break;
        case 40: os_printf(fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23], pArgs[24], pArgs[25], pArgs[26], pArgs[27], pArgs[28], pArgs[29], pArgs[30], pArgs[31], pArgs[32], pArgs[33], pArgs[34], pArgs[35], pArgs[36], pArgs[37], pArgs[38], pArgs[39]); break;
#if (PRINTF_PS_MAXARGS > 40)
#  error Please implement PRINTF_PS_MAXARGS > 40!
#endif
    }
#endif
}

#define SPRINTF_PS_MAXARGS 40

int ICACHE_FLASH_ATTR sprintf_PP(char *str, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);

    // copy format so that we can modify it
    const int fmtLen = strlen_P(fmt);
    char fmtCopy[fmtLen + 1];
    //strcpy_P(fmtCopy, fmt);
    os_strcpy(fmtCopy, fmt);

    // generate list of arguments
    const void *pArgs[SPRINTF_PS_MAXARGS + 1];
    char *pFmt = fmtCopy;
    const char *dummy = "<dummy>";
    int nArgs = 0;

    while (nArgs < (int)NUMOF(pArgs) - 1)
    {
        const char c = *(pFmt++);
        if (c == '\0')
        {
            break;
        }
        if ( (c == '%') && ((pFmt[0] != '*') && (pFmt[0] != '%')) )
        {
            const void *pkArg = va_arg(ap, const void *);

            // replace 'S' with 's' and copy ROM string to RAM
            if ( (pFmt[0] == 'S') || ((pFmt[0] == 's') && (pkArg >= (const void *)ESP_FLASH_BASE)) )
            {
                pFmt[0] = 's';
                pArgs[nArgs] = dummy;

                int strLen = 1;
                const char *pkRom = pkArg;
                while (pgm_read_uint8(pkRom) != '\0')
                {
                    strLen++;
                    pkRom++;
                }
                char *strCopy = alloca(strLen);
                strcpy_P(strCopy, pkArg);
                pArgs[nArgs] = strCopy;
            }
            else
            {
                pArgs[nArgs] = pkArg;
            }
            nArgs++;
        }
    }
    va_end(ap);

    int res = 0;
    switch (nArgs)
    {
        case  0: res = os_sprintf(str, fmtCopy); break;
        case  1: res = os_sprintf(str, fmtCopy, pArgs[0]); break;
        case  2: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1]); break;
        case  3: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2]); break;
        case  4: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3]); break;
        case  5: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4]); break;
        case  6: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5]); break;
        case  7: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6]); break;
        case  8: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7]); break;
        case  9: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8]); break;
        case 10: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9]); break;
        case 11: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10]); break;
        case 12: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11]); break;
        case 13: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12]); break;
        case 14: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13]); break;
        case 15: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14]); break;
        case 16: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15]); break;
        case 17: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16]); break;
        case 18: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17]); break;
        case 19: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18]); break;
        case 20: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19]); break;
        case 21: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20]); break;
        case 22: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21]); break;
        case 23: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22]); break;
        case 24: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23]); break;
        case 25: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23], pArgs[24]); break;
        case 26: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23], pArgs[24], pArgs[25]); break;
        case 27: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23], pArgs[24], pArgs[25], pArgs[26]); break;
        case 28: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23], pArgs[24], pArgs[25], pArgs[26], pArgs[27]); break;
        case 29: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23], pArgs[24], pArgs[25], pArgs[26], pArgs[27], pArgs[28]); break;
        case 30: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23], pArgs[24], pArgs[25], pArgs[26], pArgs[27], pArgs[28], pArgs[29]); break;
        case 31: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23], pArgs[24], pArgs[25], pArgs[26], pArgs[27], pArgs[28], pArgs[29], pArgs[30]); break;
        case 32: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23], pArgs[24], pArgs[25], pArgs[26], pArgs[27], pArgs[28], pArgs[29], pArgs[30], pArgs[31]); break;
        case 33: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23], pArgs[24], pArgs[25], pArgs[26], pArgs[27], pArgs[28], pArgs[29], pArgs[30], pArgs[31], pArgs[32]); break;
        case 34: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23], pArgs[24], pArgs[25], pArgs[26], pArgs[27], pArgs[28], pArgs[29], pArgs[30], pArgs[31], pArgs[32], pArgs[33]); break;
        case 35: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23], pArgs[24], pArgs[25], pArgs[26], pArgs[27], pArgs[28], pArgs[29], pArgs[30], pArgs[31], pArgs[32], pArgs[33], pArgs[34]); break;
        case 36: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23], pArgs[24], pArgs[25], pArgs[26], pArgs[27], pArgs[28], pArgs[29], pArgs[30], pArgs[31], pArgs[32], pArgs[33], pArgs[34], pArgs[35]); break;
        case 37: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23], pArgs[24], pArgs[25], pArgs[26], pArgs[27], pArgs[28], pArgs[29], pArgs[30], pArgs[31], pArgs[32], pArgs[33], pArgs[34], pArgs[35], pArgs[36]); break;
        case 38: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23], pArgs[24], pArgs[25], pArgs[26], pArgs[27], pArgs[28], pArgs[29], pArgs[30], pArgs[31], pArgs[32], pArgs[33], pArgs[34], pArgs[35], pArgs[36], pArgs[37]); break;
        case 39: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23], pArgs[24], pArgs[25], pArgs[26], pArgs[27], pArgs[28], pArgs[29], pArgs[30], pArgs[31], pArgs[32], pArgs[33], pArgs[34], pArgs[35], pArgs[36], pArgs[37], pArgs[38]); break;
        case 40: res = os_sprintf(str, fmtCopy, pArgs[0], pArgs[1], pArgs[2], pArgs[3], pArgs[4], pArgs[5], pArgs[6], pArgs[7], pArgs[8], pArgs[9], pArgs[10], pArgs[11], pArgs[12], pArgs[13], pArgs[14], pArgs[15], pArgs[16], pArgs[17], pArgs[18], pArgs[19], pArgs[20], pArgs[21], pArgs[22], pArgs[23], pArgs[24], pArgs[25], pArgs[26], pArgs[27], pArgs[28], pArgs[29], pArgs[30], pArgs[31], pArgs[32], pArgs[33], pArgs[34], pArgs[35], pArgs[36], pArgs[37], pArgs[38], pArgs[39]); break;
#if (SPRINTF_PS_MAXARGS > 40)
#  error Please implement SPRINTF_PS_MAXARGS > 40!
#endif
    }
    return res;
}


// -------------------------------------------------------------------------------------------------

void ICACHE_FLASH_ATTR stuffInit(void)
{

#if (USER_DEBUG_UART == 0)
    // enable TXD0 (GPIO1, D10)
    PIN_PULLUP_DIS(PERIPHS_IO_MUX_U0TXD_U);
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_U0TXD_U, FUNC_U0TXD);
#elif (USER_DEBUG_UART == 1)
    // enable TXD1 (GPIO2, D4)
    PIN_FUNC_SELECT(PERIPHS_IO_MUX_GPIO2_U, FUNC_U1TXD_BK);
#else
#  error Illegal value for USER_DEBUG_UART!
#endif

    // setup UART0 to 8N1
    WRITE_PERI_REG(UART_CONF_0(USER_DEBUG_UART),
        UART_DATA_EIGHT | UART_PARITY_NONE | UART_STOP_ONE);

    // set UART0 baudrate to 115200 (ROM default is is 74880)
    WRITE_PERI_REG(UART_CLKDIV(USER_DEBUG_UART), UART_BAUD_115200);
    // = uart_div_modify(UART_CLKDIV(USER_DEBUG_UART), UART_CLK_FREQ / 115200)

    // enable os_printf()
    system_set_os_print(true);

#if (USER_DEBUG_TXBUFSIZE == 0)

    // install putc() callback
    os_install_putc1(DEBUG_PUTC_FUNC);
    //os_install_putc1(sDebugPutcSink);

#else // USER_DEBUG_TXBUFSIZE == 0

    // initialise the output buffer
    svDebugBufHead = 0;
    svDebugBufTail = 0;
    svDebugBufSize = 0;
    svDebugBufPeak = 0;
    svDebugBufDrop = 0;

    // install putc() callback
    os_install_putc1(DEBUG_PUTC_FUNC);

#  if (USER_DEBUG_USE_ISR > 0)

    // attach UART ISR, configure tx fifo empty threshold
    ETS_UART_INTR_ATTACH(sUartISR, NULL);
    UART_TX_EMPTY_THRS(USER_DEBUG_UART, 16);

    // disable all UART interrupt sources (this seems to be crucial!) and enable UART interrupt
    WRITE_PERI_REG(UART_INT_ENA(USER_DEBUG_UART), 0x0);
    WRITE_PERI_REG(UART_INT_CLR(USER_DEBUG_UART), ~0x0);
    ETS_UART_INTR_ENABLE();

#  else // (USER_DEBUG_USE_ISR > 0)

    os_timer_disarm(&sDebugFlushTimer);
    os_timer_setfn(&sDebugFlushTimer, (os_timer_func_t *)sDebugFlushTimerFunc, NULL);
    os_timer_arm(&sDebugFlushTimer, 10, 1); // every 10ms

#  endif // (USER_DEBUG_USE_ISR > 0)

#endif // USER_DEBUG_TXBUFSIZE == 0

    os_delay_us(50000);
    int n = 20;
    while (n--)
    {
        printf_PP(PSTR("\n\n\n\n\n\n\n\n\n\n"));
    }
    os_delay_us(50000);
    DEBUG("stuff: init (" STRINGIFY(USER_DEBUG_TXBUFSIZE) ")");
}

// -------------------------------------------------------------------------------------------------

void ICACHE_FLASH_ATTR stuffStatus(void)
{
#if (USER_DEBUG_TXBUFSIZE > 0)
    uint16_t size, peak, drop;
    CS_ENTER;
    size = svDebugBufSize;
    peak = svDebugBufPeak;
    drop = svDebugBufDrop;
    svDebugBufPeak = 0;
    svDebugBufDrop = 0;
    CS_LEAVE;
    uint16_t percPeak = ((peak * 8 * 100 / sizeof(svDebugBuf)) + 4) >> 3;
    if (svDebugBufDrop)
    {
        WARNING("mon: debug: size=%u/%u peak=%u/%u%%drop=%u", // SDK bug: %% produces "% " (a space too many)
            size, sizeof(svDebugBuf), peak, percPeak, drop);
    }
    else
    {
        DEBUG("mon: debug: size=%u/%u peak=%u/%u%%drop=%u",
            size, sizeof(svDebugBuf), peak, percPeak, drop);
    }
#endif
}


/* *********************************************************************************************** */
//@}
// eof
