#include "stdinc.h"

#include <esp/spi.h>

#include "stuff.h"
#include "debug.h"
#include "spitest.h"

#define STATUS_GPIO 2 // D4, built-in LED
#define LEDS_SPI 1

static uint32_t sLedsSpiBuf[50];
static int sLedsSpiBufIx;
static int sLedsSpiBufNum;

static volatile uint32_t svNbits[10];
static volatile uint32_t svNbitsIx = 0;

// someone else has the same problem: https://bbs.espressif.com/viewtopic.php?t=360

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
        svNbits[svNbitsIx++] = nBits;
        // set number of bits to send, only MOSI, all other = 0
        SPI(LEDS_SPI).USER1 = VAL2FIELD_M(SPI_USER1_MOSI_BITLEN, nBits - 1);
        //SET_MASK_BITS(SPI(LEDS_SPI).USER1, VAL2FIELD_M(SPI_USER1_MOSI_BITLEN, nBits - 1));

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

static volatile int nIrq1 = 0;
static volatile int nIrq2 = 0;
static volatile int nIrq3 = 0;

// SPI interrupt handler
IRAM static void sLedsSpiIsr(void *pArg)
{
//    monIsrEnter();
    gpio_write(STATUS_GPIO, false);
    nIrq1++;

    // this must be read first (_before_ reading the status or clearing the interrupts)
    const uint32_t isrStatus = DPORT.SPI_INT_STATUS;

    /*if ((isrStatus & DPORT_SPI_INT_STATUS_SPI0) != 0)
    {
    }
    else */
    if ((isrStatus & DPORT_SPI_INT_STATUS_SPI1) != 0)
    {
        nIrq2++;
        const uint32_t hspiStatus = SPI(LEDS_SPI).SLAVE0;
        if ((hspiStatus & SPI_SLAVE0_TRANS_DONE) != 0)
        {
            nIrq3++;
            // load more data into SPI FIFO
            sLedsSpiBufLoad();
        }
    }

    // clear all interrupts (must be done _after_ reading the status registers)
    CLEAR_MASK_BITS(SPI(LEDS_SPI).SLAVE0, SPI_SLAVE0_ALL_DONE);

    gpio_write(STATUS_GPIO, true);

//    monIsrLeave();
}


static uint32_t times[10];

void spitest(void)
{
    times[0] = RTC.COUNTER;
    PRINT("spitest...");

    gpio_enable(STATUS_GPIO, GPIO_OUTPUT);
    gpio_write(STATUS_GPIO, true); // off, LED logic is inverted


    // config SPI interface
    static const spi_settings_t skSpiSettings =
    {
        .mode = SPI_MODE0,
        .freq_divider = SPI_FREQ_DIV_8M,
        .msb = true,
        .endianness = SPI_LITTLE_ENDIAN,
        .minimal_pins = true
    };
    spi_set_settings(LEDS_SPI, &skSpiSettings);
    _xt_isr_mask(BIT(INUM_SPI));
    _xt_isr_attach(INUM_SPI, sLedsSpiIsr, NULL);

    // it seems to be crucial to clear and disable all interrupts on _both_ SPIs
    // (some enabled by default?!), similar to the UART IRQs (see user_stuff.c)
    CLEAR_MASK_BITS(SPI(0).SLAVE0, SPI_SLAVE0_ALL_DONE | SPI_SLAVE0_ALL_DONE_EN);
    CLEAR_MASK_BITS(SPI(1).SLAVE0, SPI_SLAVE0_ALL_DONE | SPI_SLAVE0_ALL_DONE_EN);

    SPI(LEDS_SPI).CTRL1 = 0;
    SPI(LEDS_SPI).CTRL2 = 0;
    SPI(LEDS_SPI).USER2 = 0;
    SPI(LEDS_SPI).PIN = SPI_PIN_CS0_DISABLE | SPI_PIN_CS1_DISABLE | SPI_PIN_CS2_DISABLE;
    SPI(LEDS_SPI).SLAVE1 = 0;
    SPI(LEDS_SPI).SLAVE2 = 0;
    SPI(LEDS_SPI).SLAVE3 = 0;

    // fill buffer with data
    uint8_t *pBuf = (uint8_t *)sLedsSpiBuf;
    const int bufSize = sizeof(sLedsSpiBuf);
    for (int ix = 0; ix < bufSize; ix++)
    {
        pBuf[ix] = ix;
    }
    for (int ix = 0; ix < NUMOF(sLedsSpiBuf); ix++)
    {
        DEBUG("%02d: %02x %02x %02x %02x", ix,
            (sLedsSpiBuf[ix] >>  0) & 0xff,
            (sLedsSpiBuf[ix] >>  8) & 0xff,
            (sLedsSpiBuf[ix] >> 16) & 0xff,
            (sLedsSpiBuf[ix] >> 24) & 0xff);
    }

    times[1] = RTC.COUNTER;


    // prepare for send
    const int nBytesToSend = sizeof(sLedsSpiBuf);
    const int nWordsToSend =  (nBytesToSend / 4) + ((nBytesToSend % 4) != 0 ? 1 : 0);
    DEBUG("nWords=%d nBytesToSend=%d nWordsToSend=%d TRANS_COUNT=%u",
        (int)NUMOF(sLedsSpiBuf), nBytesToSend, nWordsToSend, FIELD2VAL(SPI_SLAVE0_TRANS_COUNT, SPI(LEDS_SPI).SLAVE0));

    sLedsSpiBufIx = 0;
    sLedsSpiBufNum = nWordsToSend;

    // enable transfer done interrupt source
    SET_MASK_BITS(SPI(LEDS_SPI).SLAVE0, SPI_SLAVE0_TRANS_DONE_EN);

    // disable MOSI, MISO, ADDR, COMMAND, DUMMY in case previously set
    CLEAR_MASK_BITS(SPI(LEDS_SPI).USER0, SPI_USER0_COMMAND | SPI_USER0_ADDR | SPI_USER0_DUMMY | SPI_USER0_MISO | SPI_USER0_MOSI);

    // enable only MOSI part of SPI transaction
    SET_MASK_BITS(SPI(LEDS_SPI).USER0, SPI_USER0_MOSI);

    // unmask SPI interrupt
    _xt_isr_unmask(BIT(INUM_SPI));

    times[2] = RTC.COUNTER;

    sLedsSpiBufLoad();

    times[3] = RTC.COUNTER;


    osSleep(200);
    DEBUG("nIrq1=%d nIrq2=%d nIrq3=%d", nIrq1, nIrq2, nIrq3);
    DEBUG("sbNbits: %u %u %u %u %u", svNbits[0], svNbits[1], svNbits[2], svNbits[3], svNbits[4]);
    DEBUG("TRANS_COUNT=%u", FIELD2VAL(SPI_SLAVE0_TRANS_COUNT, SPI(LEDS_SPI).SLAVE0));

    PRINT("done");

    for (int ix = 0; ix < NUMOF(times) - 1; ix++)
    {
        DEBUG("times[%02d] = %8u %8u", ix, times[ix], times[ix + 1] - times[ix]);
    }
}

/* *********************************************************************************************** */
//@}
// eof
