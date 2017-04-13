// by Philippe Kehl <flipflip at oinkzwurgl dot org>

#include "user_stuff.h"
#include "user_ws2801.h"


/* ***** HSV to RGB conversion ******************************************************************* */

//! HSV2RGB conversion method to use
/*!
    - 1 = classic (à la wikipedia, linear ramps), optimized
    - 2 = using saturation/value dimming curve to to make it look more natural
    - 3 = FastLED's "rainbow" coversion with better yellow and neater hue sweeps
          FIXME: this has a problem with value = 0

    See below for references & credits.
*/
#define HSV2RGB_METHOD 2

#if ( (HSV2RGB_METHOD == 1) || (HSV2RGB_METHOD == 2) )
//! classic HSV2RGB code (#HSV2RGB_METHOD 1 and 2) à la Wikipedia
#  define HSV2RGB_CLASSIC(_H, _S, _V, _R, _G, _B) \
    const uint8_t __H = _H; \
    const uint8_t __S = _S; \
    const uint8_t __V = _V; \
    const uint32_t s = (6 * (uint32_t)__H) >> 8;               /* the segment 0..5 (360/60 * [0..255] / 256) */ \
    const uint32_t t = (6 * (uint32_t)__H) & 0xff;             /* within the segment 0..255 (360/60 * [0..255] % 256) */ \
    const uint32_t l = ((uint32_t)__V * (255 - (uint32_t)__S)) >> 8; /* lower level */ \
    const uint32_t r = ((uint32_t)__V * (uint32_t)__S * t) >> 16;    /* ramp */ \
    switch (s) \
    { \
        case 0: (_R) = (uint8_t)__V;        (_G) = (uint8_t)(l + r);    (_B) = (uint8_t)l;          break; \
        case 1: (_R) = (uint8_t)(__V - r);  (_G) = (uint8_t)__V;        (_B) = (uint8_t)l;          break; \
        case 2: (_R) = (uint8_t)l;          (_G) = (uint8_t)__V;        (_B) = (uint8_t)(l + r);    break; \
        case 3: (_R) = (uint8_t)l;          (_G) = (uint8_t)(__V - r);  (_B) = (uint8_t)__V;        break; \
        case 4: (_R) = (uint8_t)(l + r);    (_G) = (uint8_t)l;          (_B) = (uint8_t)__V;        break; \
        case 5: (_R) = (uint8_t)__V;        (_G) = (uint8_t)l;          (_B) = (uint8_t)(__V - r);  break; \
    }
#endif

// ***** classic conversion *****
#if (HSV2RGB_METHOD == 1)

//! classic HSV2RGB conversion
#  define HSV2RGB(H, S, V, R, G, B) HSV2RGB_CLASSIC(H, S, V, R, G, B)

// ***** saturation/value dimming *****
#elif (HSV2RGB_METHOD == 2)

//! saturation/value lookup table
/*!

    Saturation/Value lookup table to compensate for the nonlinearity of human
    vision.  Used in the getRGB function on saturation and brightness to make
    dimming look more natural. Exponential function used to create values below
    : x from 0 - 255 : y = round(pow( 2.0, x+64/40.0) - 1)

    From: http://www.kasperkamperman.com/blog/arduino/arduino-programming-hsb-to-rgb/
*/
static const uint8_t skMatrixDimCurve[] =
{
      0,   1,   1,   2,   2,   2,   2,   2,   2,   3,   3,   3,   3,   3,   3,   3,
      3,   3,   3,   3,   3,   3,   3,   4,   4,   4,   4,   4,   4,   4,   4,   4,
      4,   4,   4,   5,   5,   5,   5,   5,   5,   5,   5,   5,   5,   6,   6,   6,
      6,   6,   6,   6,   6,   7,   7,   7,   7,   7,   7,   7,   8,   8,   8,   8,
      8,   8,   9,   9,   9,   9,   9,   9,  10,  10,  10,  10,  10,  11,  11,  11,
     11,  11,  12,  12,  12,  12,  12,  13,  13,  13,  13,  14,  14,  14,  14,  15,
     15,  15,  16,  16,  16,  16,  17,  17,  17,  18,  18,  18,  19,  19,  19,  20,
     20,  20,  21,  21,  22,  22,  22,  23,  23,  24,  24,  25,  25,  25,  26,  26,
     27,  27,  28,  28,  29,  29,  30,  30,  31,  32,  32,  33,  33,  34,  35,  35,
     36,  36,  37,  38,  38,  39,  40,  40,  41,  42,  43,  43,  44,  45,  46,  47,
     48,  48,  49,  50,  51,  52,  53,  54,  55,  56,  57,  58,  59,  60,  61,  62,
     63,  64,  65,  66,  68,  69,  70,  71,  73,  74,  75,  76,  78,  79,  81,  82,
     83,  85,  86,  88,  90,  91,  93,  94,  96,  98,  99, 101, 103, 105, 107, 109,
    110, 112, 114, 116, 118, 121, 123, 125, 127, 129, 132, 134, 136, 139, 141, 144,
    146, 149, 151, 154, 157, 159, 162, 165, 168, 171, 174, 177, 180, 183, 186, 190,
    193, 196, 200, 203, 207, 211, 214, 218, 222, 226, 230, 234, 238, 242, 248, 255
};


//! classic HSV2RGB conversion with saturation/value dimming
#  define HSV2RGB(H, S, V, R, G, B) \
    HSV2RGB_CLASSIC(H, 255 - skMatrixDimCurve[255-(S)], skMatrixDimCurve[V], R, G, B)


// ***** rainbow conversion *****
#elif (HSV2RGB_METHOD == 3)

//  FastLED code (excerpt) from https://github.com/FastLED/FastLED
#  define scale8(i, scale) ((int)(i) * (int)(scale) ) >> 8
#  define scale8_video_LEAVING_R1_DIRTY( g, Gscale) scale8_video(g, Gscale)
#  define scale8_video(i, scale) (i == 0) ? 0 : (((int)i * (int)(scale) ) >> 8) + ((scale != 0) ? 1 : 0)
#  define nscale8x3(r, g, b, scale) \
    (r) = ((I)(r) * (I)(scale) ) >> 8; \
    (g) = ((I)(g) * (I)(scale) ) >> 8; \
    (b) = ((I)(b) * (I)(scale) ) >> 8
#  define nscale8x3_video(r, g, b, scale) \
    const uint8_t nonzeroscale = (scale != 0) ? 1 : 0; \
    (r) = ((r) == 0) ? 0 : (((I)(r) * (I)(scale) ) >> 8) + nonzeroscale; \
    (g) = ((g) == 0) ? 0 : (((I)(g) * (I)(scale) ) >> 8) + nonzeroscale; \
    (b) = ((b) == 0) ? 0 : (((I)(b) * (I)(scale) ) >> 8) + nonzeroscale;
//! "rainbow" HSV2RGB conversion
static void sMatrixHSV2RGBrainbow(IN const uint8_t hue, IN const uint8_t sat, IN const uint8_t val,
                                  OUT uint8_t *R, OUT uint8_t *G, OUT uint8_t *B)
{
    // Yellow has a higher inherent brightness than
    // any other color; 'pure' yellow is perceived to
    // be 93% as bright as white. In order to make
    // yellow appear the correct relative brightness,
    // it has to be rendered brighter than all other
    // colors.
    // Level Y1 is a moderate boost, the default.
    // Level Y2 is a strong boost.
    const uint8_t Y1 = 0;
    const uint8_t Y2 = 1;

    // G2: Whether to divide all greens by two.
    // Depends GREATLY on your particular LEDs
    const uint8_t G2 = 0;

    // Gscale: what to scale green down by.
    // Depends GREATLY on your particular LEDs
    const uint8_t Gscale = 0;

    const uint8_t offset = hue & 0x1F; // 0..31

    // offset8 = offset * 8
    uint8_t offset8 = offset * 8;
    //{
    //    offset8 <<= 1;
    //    asm volatile("");
    //    offset8 <<= 1;
    //    asm volatile("");
    //    offset8 <<= 1;
    //}

    const uint8_t third = scale8( offset8, (256 / 3));

    uint8_t r, g, b;

    if ( ! (hue & 0x80) )
    {
        // 0XX
        if ( ! (hue & 0x40) )
        {
            // 00X
            //section 0-1
            if ( ! (hue & 0x20) )
            {
                // 000
                //case 0: // R -> O
                r = 255 - third;
                g = third;
                b = 0;
            }
            else
            {
                // 001
                //case 1: // O -> Y
                if ( Y1 )
                {
                    r = 171;
                    g = 85 + third;
                    b = 0;
                }
                if ( Y2 )
                {
                    r = 171 + third;
                    uint8_t twothirds = (third << 1);
                    g = 85 + twothirds;
                    b = 0;
                }
            }
        }
        else
        {
            //01X
            // section 2-3
            if ( ! (hue & 0x20) )
            {
                // 010
                //case 2: // Y -> G
                if ( Y1 )
                {
                    uint8_t twothirds = (third << 1);
                    r = 171 - twothirds;
                    g = 171 + third;
                    b = 0;
                }
                if ( Y2 )
                {
                    r = 255 - offset8;
                    g = 255;
                    b = 0;
                }
            }
            else
            {
                // 011
                // case 3: // G -> A
                r = 0;
                g = 255 - third;
                b = third;
            }
        }
    }
    else
    {
        // section 4-7
        // 1XX
        if ( ! (hue & 0x40) )
        {
            // 10X
            if ( ! ( hue & 0x20) )
            {
                // 100
                //case 4: // A -> B
                r = 0;
                uint8_t twothirds = (third << 1);
                g = 171 - twothirds;
                b = 85 + twothirds;

            }
            else
            {
                // 101
                //case 5: // B -> P
                r = third;
                g = 0;
                b = 255 - third;

            }
        }
        else
        {
            if ( ! (hue & 0x20) )
            {
                // 110
                //case 6: // P -- K
                r = 85 + third;
                g = 0;
                b = 171 - third;

            }
            else
            {
                // 111
                //case 7: // K -> R
                r = 171 + third;
                g = 0;
                b = 85 - third;

            }
        }
    }

    // This is one of the good places to scale the green down,
    // although the client can scale green down as well.
    if ( G2 )
    {
        g = g >> 1;
    }
    if ( Gscale )
    {
        g = scale8_video_LEAVING_R1_DIRTY( g, Gscale);
    }

    // Scale down colors if we're desaturated at all
    // and add the brightness_floor to r, g, and b.
    if ( sat != 255 )
    {
        nscale8x3_video( r, g, b, sat);

        uint8_t desat = 255 - sat;
        desat = scale8( desat, desat);

        uint8_t brightness_floor = desat;
        r += brightness_floor;
        g += brightness_floor;
        b += brightness_floor;
    }

    // Now scale everything down if we're at value < 255.
    if ( val != 255 )
    {
        const uint8_t _val = scale8_video_LEAVING_R1_DIRTY(val, val);
        nscale8x3_video( r, g, b, _val);
    }

    // Here we have the old AVR "missing std X+n" problem again
    // It turns out that fixing it winds up costing more than
    // not fixing it.
    // To paraphrase Dr Bronner, profile! profile! profile!
    //asm volatile( "" : : : "r26", "r27" );
    //asm volatile (" movw r30, r26 \n" : : : "r30", "r31");
    *R = r;
    *G = g;
    *B = b;
}
#  undef scale8
#  undef scale8_video
#  undef scale8_video_LEAVING_R1_DIRTY
#  undef nscale8x3
#  undef nscale8x3_video
#  define HSV2RGB(H, S, V, R, G, B) sMatrixHSV2RGBrainbow(H, S, V, &(R), &(G), &(B))

#else
#  error Illegal value for HSV2RGB_METHOD!
#endif


/* ***** LED framebuffer ************************************************************************* */

// LED frame buffer
static uint8_t sLedData[USER_WS2801_NUMLEDS][3];

void ICACHE_FLASH_ATTR ws2801Clear(void)
{
    os_memset(sLedData, 0, sizeof(sLedData));
}

// red, green and blue channel order
enum { IX_R = 1, IX_G = 2, IX_B = 0 };

void ICACHE_FLASH_ATTR ws2801SetHSV(const uint16_t ix, const uint8_t H, const uint8_t S, const uint8_t V)
{
    if (ix < USER_WS2801_NUMLEDS)
    {
        uint8_t R, G, B;
        HSV2RGB(H, S, V, R, G, B);
        sLedData[ix][IX_R] = R;
        sLedData[ix][IX_G] = G;
        sLedData[ix][IX_B] = B;
    }
}

void ICACHE_FLASH_ATTR ws2801SetRGB(const uint16_t ix, const uint8_t R, const uint8_t G, const uint8_t B)
{
    if (ix < USER_WS2801_NUMLEDS)
    {
        sLedData[ix][IX_R] = R;
        sLedData[ix][IX_G] = G;
        sLedData[ix][IX_B] = B;
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

// eof
