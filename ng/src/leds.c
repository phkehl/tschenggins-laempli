/*!
    \file
    \brief flipflip's Tschenggins Lämpli: LEDS WS2801 and SK9822 LED driver (see \ref FF_LEDS)

    - Copyright (c) 2017-2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

    \addtogroup FF_LEDS

    This uses the following GPIOs and the HSPI peripheral:
    - GPIO 12 (D6) = MISO
    - GPIO 13 (D7) = MOSI
    - GPIO 14 (D5) = SCK

    @{
*/

#include "stdinc.h"

#include <esp/spi.h>

#include "stuff.h"
#include "debug.h"
#include "mon.h"
#include "jenkins.h"
#include "config.h"
#include "hsv2rgb.h"
#include "leds.h"

#define LEDS_SPI 1
#define LEDS_NUM JENKINS_MAX_CH
#define LEDS_FPS 100

/* *********************************************************************************************** */

// LED frame buffer
static uint8_t sLedsData[LEDS_NUM][3];

static void sLedsClear(void)
{
    memset(&sLedsData, 0, sizeof(sLedsData));
}

enum { _R_ = 0, _G_ = 1, _B_ = 2 };

static void sLedsSetRGB(const uint16_t ix, const uint8_t R, const uint8_t G, const uint8_t B)
{
    if (ix < LEDS_NUM)
    {
        switch (configGetOrder())
        {
            case CONFIG_ORDER_RGB: sLedsData[ix][0] = R; sLedsData[ix][1] = G; sLedsData[ix][2] = B; break;
            case CONFIG_ORDER_RBG: sLedsData[ix][0] = R; sLedsData[ix][1] = B; sLedsData[ix][2] = G; break;
            case CONFIG_ORDER_GRB: sLedsData[ix][0] = G; sLedsData[ix][1] = R; sLedsData[ix][2] = B; break;
            case CONFIG_ORDER_GBR: sLedsData[ix][0] = G; sLedsData[ix][1] = B; sLedsData[ix][2] = R; break;
            case CONFIG_ORDER_BRG: sLedsData[ix][0] = B; sLedsData[ix][1] = R; sLedsData[ix][2] = G; break;
            case CONFIG_ORDER_BGR: sLedsData[ix][0] = B; sLedsData[ix][1] = G; sLedsData[ix][2] = R; break;
            case CONFIG_ORDER_UNKNOWN:
            {
                const uint8_t RGB = ((uint16_t)R + (uint16_t)G + (uint16_t)B) / 3;
                sLedsData[ix][0] = RGB; sLedsData[ix][1] = RGB; sLedsData[ix][2] = RGB;
                break;
            }
        }
    }
}

static void sLedsSetHSV(const uint16_t ix, const uint8_t H, const uint8_t S, const uint8_t V)
{
    if (ix < LEDS_NUM)
    {
        uint8_t R, G, B;
        hsv2rgb(H, S, V, &R, &G, &B);
        sLedsSetRGB(ix, R, G, B);
    }
}

#define LEDS_WS2801_BUFSIZE sizeof(sLedsData)

static int sLedsRenderWS2801(uint8_t *outBuf, const int bufSize)
{
    memset(outBuf, 0, bufSize);
    const int outSize = MIN(bufSize, sizeof(sLedsData));
    memcpy(outBuf, sLedsData, outSize);
    // TODO: adjust brightness
    return outSize;
}

#define LEDS_SK9822_END_BYTES ( LEDS_NUM / 2 / 8 + 1 )
#define LEDS_SK9822_BUFSIZE ( 4 + (LEDS_NUM * 4) + 4 + LEDS_SK9822_END_BYTES )

static int sLedsRenderSK9822(uint8_t *outBuf, const int bufSize)
{
    memset(outBuf, 0, bufSize);

    // Tim (https://cpldcpu.wordpress.com/2016/12/13/sk9822-a-clone-of-the-apa102/) says:
    // «A protocol that is compatible to both the SK9822 and the APA102 consists of the following:
    //  1. A start frame of 32 zero bits (<0x00> <0x00> <0x00> <0x00> )
    //  2. A 32 bit LED frame for each LED in the string (<0xE0+brightness> <blue> <green> <red>)
    //  3. A SK9822 reset frame of 32 zero bits (<0x00> <0x00> <0x00> <0x00> ).
    //  4. An end frame consisting of at least (n/2) bits of 0, where n is the number of LEDs in the string.»

    int outIx = 0;

    // 1. start frame
    outBuf[outIx++] = 0x00;
    outBuf[outIx++] = 0x00;
    outBuf[outIx++] = 0x00;
    outBuf[outIx++] = 0x00;

    // 2. LEDs data
    for (int ix = 0; (ix < LEDS_NUM) && (outIx < (bufSize - 4 - LEDS_SK9822_END_BYTES )); ix++)
    {
        outBuf[outIx++] = 0xe0 | (0x0a & 0x1f); // global brightness, TODO: adjust brightness
        outBuf[outIx++] = sLedsData[ix][0];
        outBuf[outIx++] = sLedsData[ix][1];
        outBuf[outIx++] = sLedsData[ix][2];
    }

    // 3. reset frame
    outBuf[outIx++] = 0x00;
    outBuf[outIx++] = 0x00;
    outBuf[outIx++] = 0x00;
    outBuf[outIx++] = 0x00;

    // 4. end frame
    int n = LEDS_SK9822_END_BYTES;
    while (n-- > 0)
    {
        outBuf[outIx++] = 0x00;
    }

    return outIx;
}



/* *********************************************************************************************** */

// copy of working frame buffer for transferring to SPI
static uint32_t sLedsSpiBuf[ MAX(LEDS_WS2801_BUFSIZE, LEDS_SK9822_BUFSIZE) / 4 + 1 ];
static int sLedsSpiBufIx;
static int sLedsSpiBufNum;

// load next words into SPI and send
IRAM static void sLedsSpiBufLoad(void)
{
    // fill SPI buffer
    uint32_t nBits = 0;
    uint32_t ix = 0;
    while ( (sLedsSpiBufIx < sLedsSpiBufNum) && (ix < NUMOF(SPI(LEDS_SPI).W)) )
    {
        SPI(LEDS_SPI).W[ix] = sLedsSpiBuf[sLedsSpiBufIx];
        ix++;
        sLedsSpiBufIx++;
        nBits += sizeof(uint32_t) * 8; // don't bother if the last word is only partially used
    }

    // data to send left
    if (nBits)
    {
        // set number of bits to send
        //SPI(LEDS_SPI).USER1 = VAL2FIELD_M(SPI_USER1_MOSI_BITLEN, nBits - 1);
        SET_MASK_BITS(SPI(LEDS_SPI).USER1, VAL2FIELD_M(SPI_USER1_MOSI_BITLEN, nBits - 1));

        // trigger send
        SET_MASK_BITS(SPI(LEDS_SPI).CMD, SPI_CMD_USR);
    }
    // all done
    else
    {
        // disable and mask interrupt
        CLEAR_MASK_BITS(SPI(LEDS_SPI).SLAVE0, SPI_SLAVE0_TRANS_DONE_EN);
        _xt_isr_mask(BIT(INUM_SPI));
    }

}

// all interrupt status flags
#define SPI_SLAVE0_ALL_DONE    (SPI_SLAVE0_TRANS_DONE    | SPI_SLAVE0_WR_STA_DONE    | SPI_SLAVE0_RD_STA_DONE    | SPI_SLAVE0_WR_BUF_DONE    | SPI_SLAVE0_RD_BUF_DONE)

// all interrupt enable flags
#define SPI_SLAVE0_ALL_DONE_EN (SPI_SLAVE0_TRANS_DONE_EN | SPI_SLAVE0_WR_STA_DONE_EN | SPI_SLAVE0_RD_STA_DONE_EN | SPI_SLAVE0_WR_BUF_DONE_EN | SPI_SLAVE0_RD_BUF_DONE_EN)

// SPI interrupt handler
IRAM static void sLedsSpiIsr(void *pArg)
{
    monIsrEnter();

    // this must be read first (_before_ reading the status or clearing the interrupts)
    const uint32_t isrStatus = DPORT.SPI_INT_STATUS;

    /*if ((isrStatus & DPORT_SPI_INT_STATUS_SPI0) != 0)
    {
    }
    else */
    if ((isrStatus & DPORT_SPI_INT_STATUS_SPI1) != 0)
    {
        const uint32_t hspiStatus = SPI(LEDS_SPI).SLAVE0;
        if ((hspiStatus & SPI_SLAVE0_TRANS_DONE) != 0)
        {
            // load more data into SPI FIFO
            sLedsSpiBufLoad();
        }
    }

    // clear all interrupts (must be done _after_ reading the status registers)
    CLEAR_MASK_BITS(SPI(LEDS_SPI).SLAVE0, SPI_SLAVE0_ALL_DONE);

    monIsrLeave();
}

// update LEDs (send data to SPI)
static void sLedsFlush(const CONFIG_DRIVER_t driver)
{
    // it seems to be crucial to clear and disable all interrupts on _both_ SPIs
    // (some enabled by default?!), similar to the UART IRQs (see user_stuff.c)
    CLEAR_MASK_BITS(SPI(0).SLAVE0, SPI_SLAVE0_ALL_DONE | SPI_SLAVE0_ALL_DONE_EN);
    CLEAR_MASK_BITS(SPI(1).SLAVE0, SPI_SLAVE0_ALL_DONE | SPI_SLAVE0_ALL_DONE_EN);

    // copy framebuffer
    int nBytesToSend = 0;
    switch (driver)
    {
        case CONFIG_DRIVER_UNKNOWN:
            break;
        case CONFIG_DRIVER_WS2801:
            nBytesToSend = sLedsRenderWS2801((uint8_t *)sLedsSpiBuf, sizeof(sLedsSpiBuf));
            break;
        case CONFIG_DRIVER_SK9822:
            nBytesToSend = sLedsRenderSK9822((uint8_t *)sLedsSpiBuf, sizeof(sLedsSpiBuf));
            break;
    }
    const int nWordsToSend = nBytesToSend / 4 + 1;
    //DEBUG("sLedsFlush() %d %d", nBytesToSend, nWordsToSend);
    if ( (nBytesToSend > 0) && (nWordsToSend > 0) )
    {
        sLedsSpiBufIx = 0;
        sLedsSpiBufNum = nWordsToSend;
    }
    else
    {
        return;
    }

    // enable transfer done interrupt source
    SET_MASK_BITS(SPI(LEDS_SPI).SLAVE0, SPI_SLAVE0_TRANS_DONE_EN);

    // disable MOSI, MISO, ADDR, COMMAND, DUMMY in case previously set
    CLEAR_MASK_BITS(SPI(LEDS_SPI).USER0, SPI_USER0_COMMAND | SPI_USER0_ADDR | SPI_USER0_DUMMY | SPI_USER0_MISO | SPI_USER0_MOSI);

    // enable only MOSI part of SPI transaction
    SET_MASK_BITS(SPI(LEDS_SPI).USER0, SPI_USER0_MOSI);

    // unmask SPI interrupt
    _xt_isr_unmask(BIT(INUM_SPI));

    // load next words into SPI buffer and send
    sLedsSpiBufLoad();
}


/* *********************************************************************************************** */

static LEDS_STATE_t sLedsStates[LEDS_NUM];

void ledsSetState(const uint16_t ledIx, const LEDS_STATE_t *pkState)
{
    if (ledIx < LEDS_NUM)
    {
        sLedsStates[ledIx] = *pkState;
    }
}


static void sLedsTask(void *pArg)
{
    static CONFIG_DRIVER_t sConfigDriverLast = CONFIG_DRIVER_UNKNOWN;
    static CONFIG_ORDER_t  sConfigOrderLast  = CONFIG_ORDER_UNKNOWN;
    static CONFIG_BRIGHT_t sConfigBrightLast = CONFIG_BRIGHT_UNKNOWN;

    while (true)
    {
        const CONFIG_DRIVER_t configDriver = configGetDriver();
        const CONFIG_ORDER_t  configOrder  = configGetOrder();
        const CONFIG_BRIGHT_t configBright = configGetBright();

        // handle config changes
        bool doDemo = false;
        if (sConfigDriverLast != configDriver)
        {
            DEBUG("leds: driver change");
            sLedsClear();
            sLedsFlush(sConfigDriverLast);
            sConfigDriverLast = configDriver;
            doDemo = true;
        }
        if (sConfigOrderLast != configOrder)
        {
            DEBUG("leds: order change");
            sConfigOrderLast = configOrder;
            doDemo = true;
        }
        if (sConfigBrightLast != configBright)
        {
            DEBUG("leds: bright change");
            sConfigBrightLast = configBright;
            doDemo = true;
        }

        // cannot do much if we don't know the driver
        if (configGetDriver() == CONFIG_DRIVER_UNKNOWN)
        {
            osSleep(100);
            continue;
        }

        // demo?
        if (doDemo)
        {
            DEBUG("leds: demo");
            for (uint16_t ix = 0; ix < LEDS_NUM; ix += 3)
            {
                sLedsSetRGB(ix + 0, 255, 0, 0);
                sLedsSetRGB(ix + 1, 0, 255, 0);
                sLedsSetRGB(ix + 2, 0, 0, 255);
            }
            for (uint16_t n = 0; n < 20; n++)
            {
                sLedsFlush(configDriver);
                osSleep(100);
            }
        }


        // render next frame..
        static uint32_t sTick;
        sLedsClear();
        for (uint16_t ix = 0; ix < NUMOF(sLedsStates); ix++)
        {
            LEDS_STATE_t *pState = &sLedsStates[ix];
            if (pState->dVal != 0)
            {
                if (pState->val < pState->minVal)
                {
                    pState->val = pState->minVal;
                }
                else if (pState->val > pState->maxVal)
                {
                    pState->val = pState->maxVal;
                }
                if (pState->dVal > 0)
                {
                    if ( ((int)pState->val + (int)pState->dVal) <= (int)pState->maxVal )
                    {
                        pState->val += pState->dVal;
                    }
                    else
                    {
                        pState->dVal = -pState->dVal;
                    }
                }
                else if (pState->dVal < 0)
                {
                    if ( ((int)pState->val + (int)pState->dVal) >= (int)pState->minVal )
                    {
                        pState->val += pState->dVal;
                    }
                    else
                    {
                        pState->dVal = -pState->dVal;
                    }
                }
            }
            sLedsSetHSV(ix, pState->hue, pState->sat, pState->val);
        }

        vTaskDelayUntil(&sTick, MS2TICKS(1000 / LEDS_FPS));
        sLedsFlush(configDriver);
    }

}


/* *********************************************************************************************** */

void ledsInit(void)
{
    DEBUG("leds: init (%ux3=%u / %u, %u / %u*4=%u)",
        LEDS_NUM, sizeof(sLedsData),
        LEDS_WS2801_BUFSIZE, LEDS_SK9822_BUFSIZE,
        NUMOF(sLedsSpiBuf), sizeof(sLedsSpiBuf));

    memset(&sLedsStates, 0, sizeof(sLedsStates));

    static const spi_settings_t skSpiSettings =
    {
        .mode = SPI_MODE0,
        .freq_divider = SPI_FREQ_DIV_2M,
        .msb = true,
        .endianness = SPI_LITTLE_ENDIAN,
        .minimal_pins = true
    };
    spi_set_settings(LEDS_SPI, &skSpiSettings);

    //const uint8_t buf[] = { 0xff, 0x00, 0x00,  0x00, 0xff, 0x00,  0x00, 0x00, 0xff };
    //spi_transfer(LEDS_SPI, buf, NULL, sizeof(buf), SPI_8BIT);

    _xt_isr_mask(BIT(INUM_SPI));
    _xt_isr_attach(INUM_SPI, sLedsSpiIsr, NULL);

    sLedsClear();
    sLedsFlush(CONFIG_DRIVER_SK9822);
    osSleep(100);
    sLedsFlush(CONFIG_DRIVER_WS2801);
    osSleep(100);
}

void ledsStart(void)
{
    DEBUG("leds: start");
    xTaskCreate(sLedsTask, "ff_leds", 512, NULL, 4, NULL);
}


/* *********************************************************************************************** */
//@}
// eof
