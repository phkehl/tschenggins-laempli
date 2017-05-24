/*!
    \file
    \brief flipflip's Tschenggins Lämpli: filesystem (see \ref USER_FS)

    - Copyright (c) 2017 Philippe Kehl <flipflip at oinkzwurgl dot org>,
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

    \addtogroup USER_FS

    @{
*/

#include "user_fs.h"
#include "user_httpd.h"
#include "version_gen.h"

typedef struct FS_TOC_ENTRY_s
{
    char     name[32];
    char     type[32];
    uint32_t size;
    uint32_t offset;
    uint32_t magic;
    __PAD(4);
} FS_TOC_ENTRY_t;

#define FS_MAGIC 0xb5006bb1 // 0xb16b00b5


#define FS_ADDR FF_FSADDR
#define FS_SECTOR (FS_ADDR / SPI_FLASH_SEC_SIZE)

static const char skSpiOpResStr[][8] PROGMEM =
{
    { "OK\0" }, { "ERROR\0" }, { "TIMEOUT\0" }
};

// forward declaration
static bool sFsRequestCb(struct espconn *pConn, const HTTPD_REQCB_INFO_t *pkInfo);

void ICACHE_FLASH_ATTR fsInit(void)
{
    DEBUG("fs: init (%u, 0x%08x)", FS_SECTOR, FS_ADDR);

    uint32_t *pBuf = memAlloc(SPI_FLASH_SEC_SIZE);
    if (pBuf == NULL)
    {
        ERROR("fs: alloc fail");
        return;
    }

    // get filesystem toc and register callback to serve the files
    const SpiFlashOpResult res = spi_flash_read(FS_ADDR, pBuf, SPI_FLASH_SEC_SIZE);
    if (res == SPI_FLASH_RESULT_OK)
    {
        //hexdump(pBuf, 200);
        const FS_TOC_ENTRY_t *pkToc = (const FS_TOC_ENTRY_t *)pBuf;
        while ( (pkToc->magic == FS_MAGIC) && ((const uint8_t *)pkToc < ((const uint8_t *)pBuf + SPI_FLASH_SEC_SIZE)) )
        {
            DEBUG("fs: %s (%s, %u) @ 0x%08x (%u)",
                pkToc->name, pkToc->type, pkToc->size, FS_ADDR + pkToc->offset,
                FS_SECTOR + 1 + (pkToc->offset / SPI_FLASH_SEC_SIZE));

            char path[sizeof(pkToc->name) + 2];
            os_sprintf(path, "/%s", pkToc->name);
            httpdRegisterRequestCb(path, HTTPD_AUTH_PUBLIC, sFsRequestCb);

            pkToc++;
        }

    }
    else
    {
        ERROR("fs: toc fail (%s)",
            res < NUMOF(skSpiOpResStr) ? skSpiOpResStr[res] : PSTR("???"));
    }

    memFree(pBuf);
}



#if 0
#  warning REQ_DEBUG is on
#  define REQ_DEBUG(...) DEBUG(__VA_ARGS__)
#  define IF_REQ_DEBUG(...) __VA_ARGS__
#else
#  define REQ_DEBUG(...) /* nothing */
#  define IF_REQ_DEBUG(...) /* nothing */
#endif


static bool sFsConnCb(struct espconn *pConn, HTTPD_CONN_DATA_t *pData, const HTTPD_CONNCB_t reason);

static bool ICACHE_FLASH_ATTR sFsRequestCb(struct espconn *pConn, const HTTPD_REQCB_INFO_t *pkInfo)
{
    uint32_t *pBuf = memAlloc(SPI_FLASH_SEC_SIZE);
    if (pBuf == NULL)
    {
        return httpdSendError(pConn, pkInfo->path, 500, NULL, PSTR("fs: malloc fail"));
    }
    REQ_DEBUG("sFsRequestCb(%p) pBuf=%p", pConn, pBuf);

    const char *errorMsg = NULL;
    bool error = false;

    // find toc entry
    REQ_DEBUG("sFsRequestCb(%p) read toc addr=%u (%u)", pConn, FS_ADDR, FS_ADDR / SPI_FLASH_SEC_SIZE);
    const SpiFlashOpResult res = spi_flash_read(FS_ADDR, pBuf, SPI_FLASH_SEC_SIZE);
    if (res == SPI_FLASH_RESULT_OK)
    {
        //hexdump(pBuf, 200);
        const FS_TOC_ENTRY_t *pkToc = (const FS_TOC_ENTRY_t *)pBuf;
        while ( (pkToc->magic == FS_MAGIC) && ((const uint8_t *)pkToc < ((const uint8_t *)pBuf + SPI_FLASH_SEC_SIZE)) )
        {
            if (os_strcmp(pkToc->name, &pkInfo->path[1]) == 0)
            {
                break;
            }
            pkToc++;
        }

        // file found?
        if (pkToc->magic != FS_MAGIC)
        {
            errorMsg = PSTR("fs: toc find fail");
        }
        while (pkToc->magic == FS_MAGIC)
        {
            REQ_DEBUG("fs: %s (%s, %u) @ 0x%08x (%u)",
                pkToc->name, pkToc->type, pkToc->size, pkToc->offset,
                FS_SECTOR + 1 + (pkToc->offset / SPI_FLASH_SEC_SIZE));

            // send header
            char head[256];
            sprintf_PP(head, PSTR("HTTP/1.1 200 OK\r\n"
                    "Content-Length: %u\r\n"
                    "Content-Type: %s; charset=UTF-8\r\n"
                    "Cache-Control: max-age=86400\r\n"
                    "Connection: close\r\n" // or we'll get ourselves in malloc()/free() trouble
                    "\r\n"), pkToc->size, pkToc->type);
            if (!httpdSendData(pConn, (uint8_t *)head, os_strlen(head)))
            {
                error = true;
                break;
            }

            // prepare for sending the file contents (we must wait until the system has finished
            // sending the headers before we can send more on this connection)
            HTTPD_CONN_DATA_t templ; // httpdRegisterSentCb() will copy templ, so it's okay on the stack
            templ.p = pBuf;
            templ.i = (int)pkToc->size;
            templ.u = FS_ADDR +  SPI_FLASH_SEC_SIZE + pkToc->offset;
            httpdRegisterConnCb(pConn, &templ, sFsConnCb);
            break;
        }
    }
    else
    {
        ERROR("fs: toc read fail (%s)",
            res < NUMOF(skSpiOpResStr) ? skSpiOpResStr[res] : PSTR("???"));
        errorMsg = PSTR("fs read fail");
    }

    if (error || (errorMsg != NULL) )
    {
        memFree(pBuf);
        // we've already sent headers and maybe some data, so all we cannot send an error message
        // and all we can do is abort the connection
        return errorMsg == NULL ? false : httpdSendError(pConn, pkInfo->path, 500, NULL, errorMsg);
    }
    else
    {
        return true;
    }
}

static bool sFsConnCb(struct espconn *pConn, HTTPD_CONN_DATA_t *pData, const HTTPD_CONNCB_t reason)
{
    switch (reason)
    {
        // not interested
        case HTTPD_CONNCB_CONNECT:
        case HTTPD_CONNCB_RECEIVED:
            return true;
            break;

        // free memory
        case HTTPD_CONNCB_ABORT:
        case HTTPD_CONNCB_CLOSE:
            REQ_DEBUG("sFsSentCb(%p, %p) abort/disconnect", pConn, pData);
            memFree(pData->p);
            return true;

        // continue below
        case HTTPD_CONNCB_SENT:
            break;
    }

    REQ_DEBUG("sFsSentCb(%p, %p) read size=%d addr=0x%08x (%u)",
        pConn, pData, pData->i, pData->u, pData->u / SPI_FLASH_SEC_SIZE);

    // done
    if (pData->i <= 0)
    {
        REQ_DEBUG("sFsSentCb(%p) done", pConn);
        return true;
    }

    // load data from flash
    const SpiFlashOpResult readRes = spi_flash_read(pData->u, pData->p, SPI_FLASH_SEC_SIZE);
    if (readRes != SPI_FLASH_RESULT_OK)
    {
        ERROR("fs: read fail 0x%08x (%s)", pData->u,
            readRes < NUMOF(skSpiOpResStr) ? skSpiOpResStr[readRes] : PSTR("???"));
        return false;
    }

    // send data
    const int sendSize = pData->i > SPI_FLASH_SEC_SIZE ? SPI_FLASH_SEC_SIZE : pData->i;
    REQ_DEBUG("sFsSentCb(%p) sendSize=%d", pConn, sendSize);
    if (!httpdSendData(pConn, (uint8_t *)pData->p, sendSize))
    {
        ERROR("fs: send fail");
        pData->i = 0;
        return false;
    }

    // prepare for next chunk
    pData->i -= sendSize;
    pData->u += SPI_FLASH_SEC_SIZE;

    return true;
}


/* *********************************************************************************************** */
//@}
// eof
