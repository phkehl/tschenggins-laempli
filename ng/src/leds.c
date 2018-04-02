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
#include "leds.h"

#define LEDS_SPI 1
#define LEDS_NUM JENKINS_MAX_CH

/* *********************************************************************************************** */

// LED frame buffer
static uint8_t sLedsData[LEDS_NUM][3];

static void sLedsClear(void)
{
    memset(&sLedsData, 0, sizeof(sLedsData));
}


/* *********************************************************************************************** */

// copy of working frame buffer for transferring to SPI
static uint32_t sLedsSpiBuf[ sizeof(sLedsData) / 4 + 1 ];
static uint32_t sLedsSpiBufIx;

// load next words into SPI and send
IRAM static void sLedsSpiBufLoad(void)
{
    // fill SPI buffer
    uint32_t nBits = 0;
    uint32_t ix = 0;
    while ( (sLedsSpiBufIx < NUMOF(sLedsSpiBuf)) && (ix < NUMOF(SPI(LEDS_SPI).W)) )
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
static void sLedsFlush(void)
{
    // it seems to be crucial to clear and disable all interrupts on _both_ SPIs
    // (some enabled by default?!), similar to the UART IRQs (see user_stuff.c)
    CLEAR_MASK_BITS(SPI(0).SLAVE0, SPI_SLAVE0_ALL_DONE | SPI_SLAVE0_ALL_DONE_EN);
    CLEAR_MASK_BITS(SPI(1).SLAVE0, SPI_SLAVE0_ALL_DONE | SPI_SLAVE0_ALL_DONE_EN);

    // copy framebuffer
    memset(sLedsSpiBuf, 0, sizeof(sLedsSpiBuf));
    memcpy(sLedsSpiBuf, sLedsData, sizeof(sLedsData));
    sLedsSpiBufIx = 0;

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


void ledsInit(void)
{
    DEBUG("leds: init");

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

     sLedsClear();

     _xt_isr_mask(BIT(INUM_SPI));
     _xt_isr_attach(INUM_SPI, sLedsSpiIsr, NULL);

     osSleep(500);
     sLedsFlush();
}


/* *********************************************************************************************** */
//@}
// eof
