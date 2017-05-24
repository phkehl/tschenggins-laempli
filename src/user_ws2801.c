/*!
    \file
    \brief flipflip's Tschenggins Lämpli: WS2801 RGB LEDs (see \ref USER_WS2801)

    - Copyright (c) 2017 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

    - Register info (in user_ws2801.c) copyright (c) 2016 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>

    \addtogroup USER_WS2801

    @{
*/

#include "user_stuff.h"
#include "user_hsv2rgb.h"
#include "user_ws2801.h"
#include "user_config.h"


/* ***** LED framebuffer ************************************************************************* */

// LED frame buffer
static uint8_t sLedData[USER_WS2801_NUMLEDS][3];

void ICACHE_FLASH_ATTR ws2801Clear(void)
{
    os_memset(sLedData, 0, sizeof(sLedData));
}

// check valid colour ordering
#if   (USER_WS2801_ORDER == 123) // RGB
enum { _R_ = 0, _G_ = 1, _B_ = 2 };
#elif (USER_WS2801_ORDER == 132) // RBG
enum { _R_ = 0, _B_ = 1, _G_ = 2 };
#elif (USER_WS2801_ORDER == 213) // GRB
enum { _G_ = 0, _R_ = 1, _B_ = 2 };
#elif (USER_WS2801_ORDER == 231) // GBR
enum { _G_ = 0, _B_ = 1, _R_ = 2 };
#elif (USER_WS2801_ORDER == 312) // BRG
enum { _B_ = 0, _R_ = 1, _G_ = 2 };
#elif (USER_WS2801_ORDER == 321) // BGR
enum { _B_ = 0, _G_ = 1, _R_ = 2 };
#else
#  error Illegal value for USER_WS2801_ORDER
#endif


void ICACHE_FLASH_ATTR ws2801SetHSV(const uint16_t ix, const uint8_t H, const uint8_t S, const uint8_t V)
{
    if (ix < USER_WS2801_NUMLEDS)
    {
        uint8_t R, G, B;
        hsv2rgb(H, S, V, &R, &G, &B);
        sLedData[ix][_R_] = R;
        sLedData[ix][_G_] = G;
        sLedData[ix][_B_] = B;
    }
}

void ICACHE_FLASH_ATTR ws2801SetRGB(const uint16_t ix, const uint8_t R, const uint8_t G, const uint8_t B)
{
    if (ix < USER_WS2801_NUMLEDS)
    {
        sLedData[ix][_R_] = R;
        sLedData[ix][_G_] = G;
        sLedData[ix][_B_] = B;
    }
}


/* ***** SPI stuff ******************************************************************************* */

// registers from SDK (driver_lib/driver/spi.c, driver_lib/{driver/spi_interface.c,include/driver/spi_interface.h,include/driver/spi_register.h)
// and the internets (8N-ESP8266_SPI_Reference_en_v1.0.pdf, and others)
// Copyright (c) 2016 <ESPRESSIF SYSTEMS (SHANGHAI) PTE LTD>

#define SPI                   0    // first SPI device (used to connect to the flash I think)
#define HSPI                  1    // second, usable SPI device

// SPI clock divisor (SPI clock speed = CPU_CLK_FREQ / SPI_CLK_PREDIV / SPI_CLK_CNTDIV)
#define SPI_CLK_PREDIV        10   // 1 ... 8191
#define SPI_CLK_CNTDIV        2    // 2, 4, ..., 62


// from spi_register.h
#define REG_SPI_BASE(i)       ( 0x60000200 - ((i) * 0x100) )

#define SPI_CLOCK(i)          ( REG_SPI_BASE(i) + 0x18 )
#define SPI_CLKDIV_PRE        0x00001fff
#define SPI_CLKDIV_PRE_S      18
#define SPI_CLKCNT_N          0x0000003f
#define SPI_CLKCNT_N_S        12
#define SPI_CLKCNT_H          0x0000003f
#define SPI_CLKCNT_H_S        6
#define SPI_CLKCNT_L          0x0000003f
#define SPI_CLKCNT_L_S        0

#define SPI_USER(i)           ( REG_SPI_BASE(i) + 0x1c )
#define SPI_WR_BYTE_ORDER     (BIT(11))
#define SPI_CS_SETUP          (BIT(5))
#define SPI_CS_HOLD           (BIT(4))
#define SPI_FLASH_MODE        (BIT(2))

#define SPI_CMD(i)            ( REG_SPI_BASE(i) + 0x00 )
#define SPI_IS_BUSY(spi_no)   ( READ_PERI_REG( SPI_CMD(spi_no) ) & SPI_USR )
#define SPI_USR               (BIT(18))
#define SPI_USR_COMMAND       (BIT(31))
#define SPI_USR_ADDR          (BIT(30))
#define SPI_USR_DUMMY         (BIT(29))
#define SPI_USR_MISO          (BIT(28))
#define SPI_USR_MOSI          (BIT(27))

#define SPI_USER1(i)          ( REG_SPI_BASE(i) + 0x20 )
#define SPI_USR_MOSI_BITLEN_M   0x000001ff
#define SPI_USR_MOSI_BITLEN_S 17

#define SPI_W0(i)             ( REG_SPI_BASE(i) + 0x40 )
// there are 15 more..
#define SPI_W_BASE(i)         SPI_W0(i)
#define SPI_W_NUM             16

// interrupt stuff
#define SPI_SLAVE(i)          (REG_SPI_BASE(i)  + 0x30)
#define SPI_TRANS_CNT_M       0x0000000f
#define SPI_TRANS_CNT_S       23
// bits 9..5 are the interrupt source enable flags
#define SPI_TRANS_DONE_EN     (BIT(9))   // transfer done enable
// bits 4..0 are the interrupt done flags
#define SPI_TRANS_DONE        (BIT(4))   // transfer done flag

// all interrupt sources and all interrupt flags
#define SPI_SLAVE_ALL_EN      0x000003e0
#define SPI_SLAVE_ALL         0x0000001f

// SPI ISR status register (http://esp8266-re.foogod.com/w/index.php?title=Memory_Map)
#define SPI_ISR_STATUS        0x3ff00020
#define SPI_ISR_STATUS_SPI0   BIT(4)
#define SPI_ISR_STATUS_SPI1   BIT(7)
#define SPI_ISR_STATUS_I2S    BIT(9)


void ICACHE_FLASH_ATTR ws2801Init(void)
{
    DEBUG("ws2801: init");

    // based on https://github.com/MetalPhreak/ESP8266_SPI_Driver

    // initialise GPIO pins for HSPI
	// spi_init_gpio(spi_no, SPI_CLK_USE_DIV);

    WRITE_PERI_REG(  PERIPHS_IO_MUX_CONF_U, 0x105 ); // FIXME: why 0x105 (0b 0001 0000 0101) ?
    // from driver_lib/driver/spi.c:
    // - bit9 should be cleared when HSPI clock doesn't equal CPU clock
    // - bit8 should be cleared when SPI clock doesn't equal CPU clock
    // more magic numbers (2) from driver_lib/driver/spi.c:
    PIN_FUNC_SELECT( PERIPHS_IO_MUX_MTDI_U, 2 ); // GPIO12 = PIN_D6 = HMISO
    PIN_FUNC_SELECT( PERIPHS_IO_MUX_MTCK_U, 2 ); // GPIO13 = PIN_D7 = HMOSI
    PIN_FUNC_SELECT( PERIPHS_IO_MUX_MTMS_U, 2 ); // GPIO14 = PIN_D5 = HSCLK
    PIN_FUNC_SELECT( PERIPHS_IO_MUX_MTDO_U, 2 ); // GPIO15 = PIN_D8 = HCS

    // setup SPI clock speed: CPU_CLK_FREQ / SPI_CLK_PREDIV / SPI_CLK_CNTDIV
    // spi_clock(spi_no, SPI_CLK_PREDIV, SPI_CLK_CNTDIV);
    WRITE_PERI_REG( SPI_CLOCK(HSPI),
        ( ( (SPI_CLK_PREDIV-1)  & SPI_CLKDIV_PRE ) << SPI_CLKDIV_PRE_S ) |
        ( ( (SPI_CLK_CNTDIV-1)  & SPI_CLKCNT_N   ) << SPI_CLKCNT_N_S   ) |
        ( ( (SPI_CLK_CNTDIV>>1) & SPI_CLKCNT_H   ) << SPI_CLKCNT_H_S   ) |
        ( ( (0)                 & SPI_CLKCNT_L   ) << SPI_CLKCNT_L_S   ) );

    // byte order for tx
    //SET_PERI_REG_MASK(   SPI_USER(HSPI), SPI_WR_BYTE_ORDER ); // high to low: bit 31..0
    CLEAR_PERI_REG_MASK( SPI_USER(HSPI), SPI_WR_BYTE_ORDER ); // low to high: 0x12345678 -> 0x78 0x56 0x34 0x12

    // use and hold CS (FIXME: what does that mean?)
	SET_PERI_REG_MASK(   SPI_USER(HSPI), SPI_CS_SETUP | SPI_CS_HOLD );

    // not flash mode (FIXME: ???)
    CLEAR_PERI_REG_MASK( SPI_USER(HSPI), SPI_FLASH_MODE );

    ws2801Clear();
    ws2801Flush();
}


// copy of working frame buffer for transferring to SPI
static uint32_t sSpiBuf[ sizeof(sLedData) / 4 + 1 ];
static uint32_t sSpiBufIx;

// load next words into SPI and send
static void sSpiBufLoad(void) // RAM function
{
    // fill SPI buffer
    uint32_t nBits = 0;
    uint32_t ix = 0;
    while ( (sSpiBufIx < NUMOF(sSpiBuf) && (ix < SPI_W_NUM)) )
    {
        WRITE_PERI_REG( SPI_W_BASE(HSPI) + (sizeof(uint32_t) * ix), sSpiBuf[sSpiBufIx] );
        ix++;
        sSpiBufIx++;
        nBits += sizeof(uint32_t) * 8; // don't bother if the last word is only partially used
    }

    // data to send left
    if (nBits)
    {
        // set number of bits to send
        WRITE_PERI_REG( SPI_USER1(HSPI), ((nBits - 1) & SPI_USR_MOSI_BITLEN_M) << SPI_USR_MOSI_BITLEN_S );

        // send
        SET_PERI_REG_MASK( SPI_CMD(HSPI), SPI_USR );
    }
    // all done
    else
    {
        CLEAR_PERI_REG_MASK(SPI_SLAVE(HSPI), SPI_TRANS_DONE_EN);
        ETS_SPI_INTR_DISABLE();
    }

}

// SPI interrupt handler
static void sSpiIsr(void *pArg) // RAM function
{
    UNUSED(pArg);

    // this must be read first (_before_ reading the status or clearing the interrupts)
    const uint32_t isrStatus = READ_PERI_REG(SPI_ISR_STATUS);

    /*if (isrStatus & SPI_ISR_STATUS_SPI0)
    {
    }
    else */
    if (isrStatus & SPI_ISR_STATUS_SPI1)
    {
        const uint32_t hspiStatus = READ_PERI_REG(SPI_SLAVE(HSPI));
        if (hspiStatus & SPI_TRANS_DONE)
        {
            // load more data into SPI FIFO
            sSpiBufLoad();
        }
    }

    // clear all interrupts (must be done _after_ reading the status registers)
    CLEAR_PERI_REG_MASK(SPI_SLAVE(1), SPI_SLAVE_ALL);
}


// update LEDs (send data to SPI)
void ICACHE_FLASH_ATTR ws2801Flush(void)
{
    // it seems to be crucial to disable all interrupts on _both_ SPIs
    // (some enabled by default?!), similar to the UART IRQs (see user_stuff.c)
    CLEAR_PERI_REG_MASK(SPI_SLAVE(SPI), SPI_SLAVE_ALL_EN | SPI_SLAVE_ALL);
    CLEAR_PERI_REG_MASK(SPI_SLAVE(HSPI), SPI_SLAVE_ALL_EN | SPI_SLAVE_ALL);

    // enable transfer done interrupt source
    SET_PERI_REG_MASK(SPI_SLAVE(HSPI), SPI_TRANS_DONE_EN);

    // attach ISR and enable global SPI interrupt
    ETS_SPI_INTR_ATTACH(sSpiIsr, NULL);
    ETS_SPI_INTR_ENABLE();

    // copy framebuffer
    static uint32_t buf[ sizeof(sLedData) / 4 + 1 ];
    os_memset(sSpiBuf, 0, sizeof(buf));
    os_memcpy(sSpiBuf, sLedData, sizeof(sLedData));
    sSpiBufIx = 0;

    // disable MOSI, MISO, ADDR, COMMAND, DUMMY in case previously set
    CLEAR_PERI_REG_MASK(SPI_USER(HSPI), SPI_USR_MOSI|SPI_USR_MISO|SPI_USR_COMMAND|SPI_USR_ADDR|SPI_USR_DUMMY);

    // enable MOSI
    SET_PERI_REG_MASK( SPI_USER(HSPI), SPI_USR_MOSI );

    // load next words into SPI buffer and send
    sSpiBufLoad();
}


#if 0 // old version not using interrupts
// update LEDs (send data to SPI)
void ICACHE_FLASH_ATTR ws2801Flush(void)
{
    // copy framebuffer
    os_memset(buf, 0, sizeof(buf));
    os_memcpy(buf, sLedData, sizeof(sLedData));

    // disable MOSI, MISO, ADDR, COMMAND, DUMMY in case previously set
    CLEAR_PERI_REG_MASK(SPI_USER(HSPI), SPI_USR_MOSI|SPI_USR_MISO|SPI_USR_COMMAND|SPI_USR_ADDR|SPI_USR_DUMMY);

    // enable MOSI
    SET_PERI_REG_MASK( SPI_USER(HSPI), SPI_USR_MOSI );

    // send word by word
    // FIXME: can we use the W1, W2, .. registers to load more words in one go?

    CS_ENTER;
    uint32_t ix = 0;
    while (ix < NUMOF(buf))
    {
        const uint32_t data = buf[ix];
        const uint32_t remBytes = sizeof(sLedData) - (ix * sizeof(uint32_t));
        const uint32_t nBits = (remBytes >= 4 ? 4 : remBytes) * 8;

        // busy-wait for SPI to be ready
        while (SPI_IS_BUSY(HSPI));

        // number of bits to send
        WRITE_PERI_REG( SPI_USER1(HSPI), ((nBits - 1) & SPI_USR_MOSI_BITLEN_M) << SPI_USR_MOSI_BITLEN_S );

        // write data
        WRITE_PERI_REG( SPI_W0(HSPI), data );

        // send
        SET_PERI_REG_MASK( SPI_CMD(HSPI), SPI_USR );

        ix++;
    }
    CS_LEAVE;
}
#endif


/* *********************************************************************************************** */
//@}
// eof
