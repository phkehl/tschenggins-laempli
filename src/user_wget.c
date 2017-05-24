/*!
    \file
    \brief flipflip's Tschenggins Lämpli: HTTP requests (see \ref USER_WGET)

    - Copyright (c) 2017 Philippe Kehl <flipflip at oinkzwurgl dot org>,
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

    \addtogroup USER_WGET

    @{
*/

#include "user_stuff.h"
#include "user_wget.h"
#include "user_wifi.h"
#include "user_config.h"
#include "version_gen.h"
#include "base64.h"


/* ***** init *********************************************************************************** */

void ICACHE_FLASH_ATTR wgetInit(void)
{
    DEBUG("wget: init (%u)", sizeof(WGET_STATE_t));

}

/* ***** GET request **************************************************************************** */

#if 0
#  warning REQ_DEBUG is on
#  define REQ_DEBUG(...) DEBUG(__VA_ARGS__)
#  define IF_REQ_DEBUG(...) __VA_ARGS__
#else
#  define REQ_DEBUG(...) /* nothing */
#  define IF_REQ_DEBUG(...) /* nothing */
#endif

bool sWgetDoRequest(WGET_STATE_t *pState, const char *url);
static void sWgetHostlookupCb(const char *name, ip_addr_t *pAddr, void *arg);
static void sWgetDoConnect(WGET_STATE_t *pState);
static void sWgetDoCallback(WGET_STATE_t *pState);
static void sWgetConnectCb(void *arg);
static void sWgetDisconnectCb(void *arg);
static void sWgetReconnectCb(void *arg, sint8 err);
static void sWgetTcpSentCb(void *arg);
static void sWgetTcpRecvCb(void *arg, char *data, uint16_t size);
static os_timer_t sWgetCbTimer;

// do HTTP GET request
bool ICACHE_FLASH_ATTR wgetRequest(
    WGET_STATE_t *pState, const char *url, WGET_REQCB_t respCb, void *pUser, const int respMaxLen, const uint16_t timeout)
{
    REQ_DEBUG("wgetRequest(%p) %s", pState, url);
    os_memset(pState, 0, sizeof(*pState));
    pState->bufMax = respMaxLen;
    pState->timeout = timeout;

    // we can only handle one request at a time
    if (sWgetCbTimer.timer_arg != NULL) // "mutex"
    {
        WARNING("wget: previous operation still in progress");
        return false;
    }

    // we need at least some space
    if (pState->bufMax < 1024)
    {
        WARNING("wget: buffer too small (%d < 1024)", respMaxLen);
        return false;
    }

    // allocate temp buffer and response space
    pState->buf = memAlloc(pState->bufMax);
    if (pState->buf == NULL)
    {
        WARNING("wget: malloc fail");
        return false;
    }

    pState->respCb = respCb;
    pState->pUser  = pUser;

    return sWgetDoRequest(pState, url);
}

bool ICACHE_FLASH_ATTR sWgetDoRequest(WGET_STATE_t *pState, const char *url)
{
    // initialise state
    pState->conn.proto.tcp = &pState->tcp;
    pState->conn.type      = ESPCONN_TCP;
    pState->conn.state     = ESPCONN_NONE;

    pState->hostip.addr    = 0;

    os_memset(&pState->resp, 0, sizeof(pState->resp));

    // determine protocol, hostname and query
    // we temporarily use the response buffer for that, e.g.:
    pState->bufLen = wgetReqParamsFromUrl(url, pState->buf, pState->bufMax,
        &pState->pkHost, &pState->pkPath, &pState->pkQuery, &pState->pkAuth, &pState->https, &pState->port);
    if (pState->bufLen == 0)
    {
        WARNING("wget: illegal url: %s", url);
        return false;
    }

    PRINT("wget: %s %u %s / %s ? %s (%s)",
        pState->https ? PSTR("https") : PSTR("http"), pState->port, pState->pkHost,
        pState->pkPath, pState->pkQuery, pState->pkAuth ? PSTR("auth") : PSTR(""));


    // wgetReqParamsFromUrl() puts the auth token at the end of the buffer, which we remove
    // from the available response buffer size so that we can reuse the auth token in
    // case we handle a redirect
    if (pState->pkAuth)
    {
        const int newBufMax = pState->pkAuth - pState->buf - 1;
        if (pState->bufMax != newBufMax)
        {
            IF_REQ_DEBUG(const int authLen = os_strlen(pState->pkAuth));
            REQ_DEBUG("wgetRequest(%p) authLen=%d bufMax=%d newBufMax=%d",
                pState, authLen, pState->bufMax, newBufMax);
            pState->bufMax = newBufMax;
        }
    }

    // prepare timer for calling the user response callback
    os_timer_disarm(&sWgetCbTimer);
    os_timer_setfn(&sWgetCbTimer, (os_timer_func_t *)sWgetDoCallback, pState);
    pState->complete = false;
    pState->cbDone = false;

    // request timeout
    os_timer_arm(&sWgetCbTimer, pState->timeout ? pState->timeout : USER_WGET_DEFAULT_TIMEOUT, 0);

    // trigger hostlookup
    const int8_t res = espconn_gethostbyname(
        &pState->conn, pState->pkHost, &pState->hostip, sWgetHostlookupCb);
    // need to call the callback ourselves it seems, compare comment in sWgetHostlookupCb()
    if (res == ESPCONN_OK)
    {
        sWgetHostlookupCb(pState->pkHost, &pState->hostip, &pState->conn);
        return true;
    }
    // give up
    else if (res != ESPCONN_INPROGRESS)
    {
        WARNING("wget: hostlookup failed: %s", espconnErrStr(res));
        memFree(pState->buf);
        return false;
    }
    // else:
    // ESPCONN_INPROGRESS is the normal result
    REQ_DEBUG("wgetRequest(%p) gethostbyname fired", pState);

    // next step: wait for lookup has complete and sWgetHostlookupCb() being called

    return true;
}

static void ICACHE_FLASH_ATTR sWgetDoCallback(WGET_STATE_t *pState)
{
    pState->cbDone = true;

    WGET_RESPONSE_t *pResp = &pState->resp;
    REQ_DEBUG("sWgetDoCallback(%p) error=%d bufLen=%d", pState, pResp->error, pState->bufLen);

    // timeout?
    if (!pState->complete)
    {
        ERROR("wget: user timeout");
        espconn_abort(&pState->conn);
        pResp->error = WGET_ERROR;
    }

    // check first line (response status code)
    char *pParse = pState->buf;
    if (pResp->error == WGET_OK)
    {
        // first line
        char *firstLineStart = pParse;
        char *statusCode = &pParse[9]; // "HTTP/1.1 "
        char *firstLineEnd = strstr_P(firstLineStart, PSTR("\r\n"));
        if ( (strncmp_PP(firstLineStart, PSTR("HTTP/1.1 "), 8) != 0) || (firstLineEnd == NULL) ||
            !isdigit((int)statusCode[0]) )
        {
            ERROR("wget: response is not HTTP/1.1");
            pResp->error = WGET_ERROR;
        }
        else
        {
            pResp->status = atoi(statusCode);
            *firstLineEnd = '\0';
            REQ_DEBUG("sWgetDoCallback(%p) status=%s code=%d", pState, firstLineStart, pResp->status);
            pParse = firstLineEnd + 2;
        }
    }
    REQ_DEBUG("pParse=%s", pParse);

    // parse response
    if (pResp->error == WGET_OK)
    {
        char *body = strstr_P(pParse, PSTR("\r\n\r\n"));

        // no data or malformed response
        if ( (pState->bufLen == 0) || (body == NULL) )
        {
            ERROR("wget: no response");
            pResp->error = WGET_ERROR;
        }

        // separate response headers, strip trailing \r\n
        else
        {
            body[2] = '\0';
            body += 4;
            const int bodyLen = pState->bufLen - (body - pState->buf);
            if (bodyLen > 0)
            {
                pResp->body = body;
                pResp->bodyLen = bodyLen;
            }

            // clear remaining buffer so that the user callback can use the body as a string
            os_memset(&pState->buf[pState->bufLen], 0, pState->bufMax - pState->bufLen);
        }
    }

    // check status code
    if (pResp->error == WGET_OK)
    {
        // handle redirection
        if ( (pResp->status >= 300) && (pResp->status <= 399) )
        {
            char *locationStart = strcasestr_P(pParse, PSTR("Location: "));
            char *locationCont  = locationStart != NULL ? locationStart + 10                    : NULL;
            char *locationEnd   = locationStart != NULL ? strstr_P(locationStart, PSTR("\r\n")) : NULL;
            if ( (locationStart != NULL) && (locationCont != NULL) && (locationEnd != NULL) )
            {
                *locationEnd = '\0';
                REQ_DEBUG("sWgetDoCallback(%p) location=%s", pState, locationCont);

#if (USER_WGET_MAX_REDIRECTS > 0)
                // handle redirects
                if (pState->redirCnt++ >= USER_WGET_MAX_REDIRECTS)
                {
                    ERROR("wget: maximum redirects ("STRINGIFY(USER_WGET_MAX_REDIRECTS)") exceeded");
                    pResp->error = WGET_ERROR;
                }
                // fire request again with new url
                else
                {
                    sWgetDoRequest(pState, locationCont);
                    return;
                }
#else
                // we don't handle redirects
                WARNING("wget: redirect to %s", locationCont);
                pResp->error = WGET_WARNING;
                pResp->location = locationCont;
#endif
            }
            else
            {
                ERROR("wget: missing location header in response status %d", pResp->status);
                pResp->error = WGET_ERROR;
            }
        }
        // besides 3xx we only like "200 OK"
        else if (pResp->status != 200)
        {
            ERROR("wget: unhandled status %d (%s)", pResp->status, pState->buf);
            pResp->error = WGET_ERROR;
        }
    }

    // make sure that we have a Content-Length header because we won't handle
    // https://en.wikipedia.org/wiki/Chunked_transfer_encoding markers
    if ( (pResp->error == WGET_OK) && (strcasestr_P(pParse, PSTR("Transfer-Encoding")) != NULL) )
    {
        ERROR("wget: cannot handle Transfer-Encoding in response");
        pResp->error = WGET_ERROR;
        // (the user callback will still receive the body and bodyLen correct)
    }

    // check headers
    if (pResp->error == WGET_OK)
    {
        // headers
        char *contentLengthStart = strcasestr_P(pParse, PSTR("Content-Length: "));
        char *contentLengthCont  = contentLengthStart != NULL ? contentLengthStart + 16                    : NULL;
        char *contentLengthEnd   = contentLengthStart != NULL ? strstr_P(contentLengthStart, PSTR("\r\n")) : NULL;
        char *contentTypeStart   = strcasestr_P(pParse, PSTR("Content-Type: "));
        char *contentTypeCont    = contentTypeStart != NULL ? contentTypeStart + 14                    : NULL;
        char *contentTypeEnd     = contentTypeStart != NULL ? strstr_P(contentTypeStart, PSTR("\r\n")) : NULL;

        if ( (contentLengthStart != NULL) && (contentLengthEnd != NULL) && (contentLengthCont != NULL) )
        {
            *contentLengthEnd = '\0';
            const int len = atoi(contentLengthCont);
            REQ_DEBUG("sWgetDoCallback(%p) contentLength=%s (%s, %d)",
                pState, contentLengthStart, contentLengthCont, len);
            if (len != pResp->bodyLen)
            {
                WARNING("wget: response length mismatch (%d != %d)", pResp->bodyLen, len);
                pResp->error = WGET_WARNING;
            }
        }
        else
        {
            WARNING("wget: missing Content-Length header in response");
            pResp->error = WGET_WARNING;
        }
        if ( (contentTypeStart != NULL) && (contentTypeEnd != NULL) && (contentTypeCont != NULL) )
        {
            *contentTypeEnd = '\0';
            REQ_DEBUG("sWgetDoCallback(%p) contentType=%s (%s)",
                pState, contentTypeStart, contentTypeCont);
            pResp->contentType = contentTypeCont;
        }
        else
        {
            WARNING("wget: missing Content-Type header in response");
            pResp->error = WGET_WARNING;
        }
    }


    // call user callback
    pState->respCb(pResp, pState->pUser);

    // cleanup
    memFree(pState->buf);
    os_timer_disarm(&sWgetCbTimer);
    sWgetCbTimer.timer_arg = NULL; // release mutex
}


static void ICACHE_FLASH_ATTR sWgetHostlookupCb(const char *name, ip_addr_t *pAddr, void *arg)
{
    struct espconn *pConn = (struct espconn *)arg;
    WGET_STATE_t *pState = (WGET_STATE_t *)pConn;

    // give up, hostname lookup failed
    if (pAddr == NULL)
    {
        ERROR("wget: hostname lookup for %s failed", pState->pkHost);
        pState->resp.error = WGET_ERROR;
        pState->complete = true;
        os_timer_disarm(&sWgetCbTimer);
        os_timer_arm(&sWgetCbTimer, 5, 0); // fire once in 5ms
        return;
    }

    // pAddr is NOT what third argument we passed to espconn_gethostbyname()
    // http://bbs.espressif.com/viewtopic.php?t=2095: «The third parameter can be used to store the
    // IP address which got by DNS, so that if users call espconn_gethostbyname again, it won't run
    // DNS again, but just use the IP address it already got.»

    pState->hostip.addr = pAddr->addr;
    REQ_DEBUG("sWgetHostlookupCb(%p) %s -> "IPSTR,
        pState, pState->pkHost, IP2STR(&pState->hostip));

    // next step: connect
    sWgetDoConnect(pState);
}

static void ICACHE_FLASH_ATTR sWgetDoConnect(WGET_STATE_t *pState)
{
    struct espconn *pConn = &pState->conn;

    espconn_disconnect(pConn);

    // prepare connection handle
    os_memset(&pState->conn, 0, sizeof(pState->conn));
    os_memset(&pState->tcp, 0, sizeof(pState->tcp));
    pConn->type      = ESPCONN_TCP;
    pConn->state     = ESPCONN_NONE;
    pConn->proto.tcp = &pState->tcp;

    pConn->proto.tcp->local_port = espconn_port();
    pConn->proto.tcp->remote_port = pState->port;
    os_memcpy(pConn->proto.tcp->remote_ip, &pState->hostip, sizeof(pConn->proto.tcp->remote_ip));

    // register connection callbacks
    espconn_regist_connectcb(pConn, sWgetConnectCb);
    espconn_regist_reconcb(pConn, sWgetReconnectCb);
    espconn_regist_disconcb(pConn, sWgetDisconnectCb);
    espconn_regist_recvcb(pConn, sWgetTcpRecvCb);
    espconn_regist_sentcb(pConn, sWgetTcpSentCb);

    REQ_DEBUG("sWgetDoConnect(%p) "IPSTR":%u (%s)", &pConn,
        IP2STR(&pConn->proto.tcp->remote_ip), pState->port,
        pState->https ? PSTR("https") : PSTR("http"));

    // establish connection
    const int8_t res = pState->https ?
        espconn_secure_connect(pConn) :
        espconn_connect(pConn);
    if (res != ESPCONN_OK)
    {
        ERROR("wget: connect to %s://"IPSTR":%u failed: %s",
            pState->https ? PSTR("https") : PSTR("http"), IP2STR(&pState->hostip),
            pState->port, espconnErrStr(res));
        pState->resp.error = WGET_ERROR;
        os_timer_disarm(&sWgetCbTimer);
        os_timer_arm(&sWgetCbTimer, 5, 0); // fire once in 5ms
        return;
    }

    // next step: sWgetConnectCb()
}

static void ICACHE_FLASH_ATTR sWgetConnectCb(void *arg)
{
    struct espconn *pConn = (struct espconn *)arg;
    WGET_STATE_t *pState = (WGET_STATE_t *)pConn;
    REQ_DEBUG("sWgetConnectCb(%p)", pConn);

    // make HTTP GET request, use part of response buf after "url"
    if ( (pState->bufMax - pState->bufLen) < (pState->bufLen + 100) )
    {
        ERROR("wget: buf too small for request");
        pState->resp.error = WGET_ERROR;
        os_timer_disarm(&sWgetCbTimer);
        os_timer_arm(&sWgetCbTimer, 5, 0); // fire once in 5ms
        return;
    }
    char *req = &pState->buf[pState->bufLen + 1];
    sprintf_PP(req,
        PSTR("GET /%s?%s HTTP/1.1\r\n"  // HTTP GET request
            "Host: %s\r\n"              // provide host name for virtual host setups
            "Connection: close\r\n"     // ask server to close the connection after it sent the data
            "Authorization: Basic %s\r\n" // okay to provide empty one?
            "User-Agent: %s/%s\r\n"     // be nice
            "\r\n"),                    // end of request headers
        pState->pkPath, pState->pkQuery, pState->pkHost,
        pState->pkAuth != NULL ? pState->pkAuth : PSTR(""),
        PSTR(FF_PROJECT), PSTR(FF_BUILDVER));

    const uint16_t reqLen = os_strlen(req);
    REQ_DEBUG("req[%u]=%s", reqLen, req);

    pState->bufLen = 0; // we'll now use this for the response

    // send request
    const int8_t res2 = pState->https ?
        espconn_secure_send(pConn, (uint8_t *)req, reqLen) :
        espconn_send(pConn, (uint8_t *)req, reqLen);

    // clear buffer
    os_memset(pState->buf, 0, pState->bufMax);

    if (res2 != ESPCONN_OK)
    {
        ERROR("wget: request fail: %s", espconnErrStr(res2));
        pState->resp.error = WGET_ERROR;
        os_timer_disarm(&sWgetCbTimer);
        os_timer_arm(&sWgetCbTimer, 5, 0); // fire once in 5ms
        return;
    }

    // next step: sWgetTcpSentCb()
}

static void ICACHE_FLASH_ATTR sWgetReconnectCb(void *arg, sint8 err)
{
    struct espconn *pConn = (struct espconn *)arg;
    WGET_STATE_t *pState = (WGET_STATE_t *)pConn;
    ERROR("wget: connect to "IPSTR":%u error %d",
        IP2STR(&pState->hostip), pState->port, err);
    espconn_disconnect(pConn); // yeah?
    pState->resp.error = WGET_ERROR;
    os_timer_disarm(&sWgetCbTimer);
    os_timer_arm(&sWgetCbTimer, 5, 0); // fire once in 5ms
}

static void ICACHE_FLASH_ATTR sWgetTcpSentCb(void *arg)
{
    struct espconn *pConn = (struct espconn *)arg;
    WGET_STATE_t *pState = (WGET_STATE_t *)pConn;
    UNUSED(pState);
    REQ_DEBUG("sWgetTcpSentCb(%p)", pConn);

    // next step: one or more sWgetTcpRecvCb()
}

static void ICACHE_FLASH_ATTR sWgetTcpRecvCb(void *arg, char *data, uint16_t size)
{
    struct espconn *pConn = (struct espconn *)arg;
    WGET_STATE_t *pState = (WGET_STATE_t *)pConn;
    REQ_DEBUG("sWgetTcpRecvCb(%p) size=%u data=%s", pConn, size, data);

    // copy received data to response buffer
    const int tail = pState->bufLen + (int)size;
    if (tail > pState->bufMax)
    {
        ERROR("wget: receive buffer overflow (%d > %d)",
            tail, pState->bufMax);
        pState->resp.error = WGET_ERROR;
        os_timer_disarm(&sWgetCbTimer);
        os_timer_arm(&sWgetCbTimer, 5, 0); // fire once in 5ms
    }
    else
    {
        os_memcpy(&pState->buf[pState->bufLen], data, size);
        pState->bufLen += (int)size;
        REQ_DEBUG("sWgetTcpRecvCb(%p) size=%u, total=%d",
            pConn, size, pState->bufLen);
    }

    // next step: more data in sWgetTcpRecvCb(), or sWgetDisconnectCb()
}

static void ICACHE_FLASH_ATTR sWgetDisconnectCb(void *arg)
{
    struct espconn *pConn = (struct espconn *)arg;
    WGET_STATE_t *pState = (WGET_STATE_t *)pConn;
    REQ_DEBUG("sWgetDisconnectCb(%p) %d bytes received", pConn, pState->bufLen);

    // we might be okay
    if (!pState->cbDone) // we end up here again if we on after user timeout
    {
        // we should be okay
        pState->complete = true;
        pState->resp.error = WGET_OK;
        os_timer_disarm(&sWgetCbTimer);
        os_timer_arm(&sWgetCbTimer, 5, 0); // fire once in 5ms
    }
}


/* ***** stuff ********************************************************************************** */

int ICACHE_FLASH_ATTR wgetReqParamsFromUrl(const char *url, char *buf, const int bufSize,
    const char **host, const char **path, const char **query, const char **auth, bool *https, uint16_t *port)
{
    // TODO: url encode query string!

    *host = NULL;
    *path = NULL;
    *query = NULL;
    // don't overwrite, we may reuse it on redirect
    if (!ESP_IS_IN_DRAM(auth))
    {
        *auth = NULL;
    }
    *https = false;
    *port = 0;

    // determine protocol, hostname and query
    const int urlLen = strlen_P(url);
    if (urlLen > (bufSize - 1))
    {
        WARNING("wget: url too long (%d > %d)", urlLen > (bufSize - 1));
        return 0;
    }

    if (url != buf)
    {
        strcpy_P(buf, url);
    }
    // buf is now something like
    // "http://foo.com"
    // "http://foo.com/"
    // "http://foo.com/query?with=parameters"
    // "http://user:pass@foo.com/query?with=parameters"
    char *pParse = buf;
    //DEBUG("pParse=%s", pParse);

    // protocol
    {
        char *endOfProt = strstr_P(pParse, PSTR("://"));
        if (endOfProt == NULL)
        {
            WARNING("wget: missing protocol:// in %s", url);
            return 0;
        }
        *endOfProt = '\0';
        //DEBUG("prot=%s", pParse);
        if (strcmp_PP(pParse, PSTR("http")) == 0)
        {
            *https = false;
            *port = 80;
        }
        else if (strcmp_PP(pParse, PSTR("https")) == 0)
        {
            *https = true;
            *port = 443;
        }
        else
        {
            WARNING("wget: illegal protocol %s://", buf);
            return false;
        }
        pParse = endOfProt + 3; // "://"
    }
    //DEBUG("pParse=%s", pParse);

    // user:pass@
    int authLen = 0;
    {
        char *end = strstr_P(pParse, PSTR("/"));
        if (end == NULL)
        {
            end = &buf[urlLen];
        }
        char *monkey = strstr_P(pParse, PSTR("@"));
        //DEBUG("pParse=%p monkey=%p (%d) end=%p (%d)",
        //    pParse, monkey, monkey - pParse, end, end - pParse);
        if ( (monkey != NULL) && (monkey < (end - 3)) )
        {
            *monkey = '\0';
            char *userpass = pParse;
            const int userpassLen = os_strlen(userpass);
            //DEBUG("userpass=%s (%d)", userpass, userpassLen);
            pParse += userpassLen + 1; // "@"

            // base64 encoded auth token
            authLen = BASE64_ENCLEN(userpassLen);

            // put it at the end of buffer, compare sWgetDoRequest()
            const int authPos = bufSize - authLen - 1;
            char *pAuth = &buf[authPos];
            if ( (authPos < (urlLen + 1)) || !base64enc(userpass, pAuth, authLen))
            {
                WARNING("wget: buf error (%d, %d)", authPos, urlLen + 1);
                return false;
            }
            *auth = pAuth;
            //DEBUG("auth=%s", *auth);
        }
    }
    //DEBUG("pParse=%s", pParse);

    // hostname
    {
        char *endOfHost = strstr_P(pParse, PSTR("/"));
        if (endOfHost == NULL)
        {
            endOfHost = &buf[urlLen];
        }
        *endOfHost = '\0';
        //DEBUG("host=%s", pParse);
        if (os_strlen(pParse) < 3)
        {
            WARNING("wget: illegal host '%s'", *host);
            return false;
        }
        *host = pParse;
        pParse = endOfHost + 1;
    }
    //DEBUG("pParse=%s", pParse);

    // path
    {
        char *endOfPath = strstr_P(pParse, PSTR("?"));
        if (endOfPath == NULL)
        {
            endOfPath = &buf[urlLen];
        }
        *endOfPath = '\0';
        *path = pParse;
        pParse = endOfPath + 1;
        //DEBUG("path=%s", *path);
    }

    // query
    {
        *query = pParse;
        //DEBUG("query=%s", *query);
    }

    return urlLen + 1; // + (authLen ? authLen + 1 : 0);
}


/* ***** test *********************************************************************************** */

#if 0
#  warning wget test compiled in
static void ICACHE_FLASH_ATTR sWgetTestCb(const WGET_RESPONSE_t *pkResp, void *pUser)
{
    DEBUG("sWgetTestCb(%p, %p) error=%d status=%d contentType=%s bodyLen=%d (%d) body=[%s]"
#if (USER_WGET_MAX_REDIRECTS == 0)
        " location=%s"
#endif
        , pkResp, pUser, pkResp->error, pkResp->status, pkResp->contentType, pkResp->bodyLen, pkResp->body ? os_strlen(pkResp->body) : 0, pkResp->body
#if (USER_WGET_MAX_REDIRECTS == 0)
        , pkResp->location
#endif
        );
    if (pkResp->error == WGET_OK)
    {
        PRINT("wgetTest() PASS");
    }
    else
    {
        ERROR("wgetTest() FAIL");
    }
}

void ICACHE_FLASH_ATTR wgetTest(void)
{
#  if 0
    char buf[1000];
    const char *host;
    const char *path;
    const char *query;
    const char *auth;
    bool https;
    uint16_t port;
    PRINT("------------");
    wgetReqParamsFromUrl(PSTR("http://foo.com/"),
        buf, sizeof(buf), &host, &path, &query, &auth, &https, &port);
    PRINT("--> host=%s path=%s query=%s auth=%s https=%d port=%d", host, path, query, auth, https, port);
    PRINT("------------");
    wgetReqParamsFromUrl(PSTR("http://user:pass@foo.com/path?foo=bar"),
        buf, sizeof(buf), &host, &path, &query, &auth, &https, &port);
    PRINT("--> host=%s path=%s query=%s auth=%s https=%d port=%d", host, path, query, auth, https, port);
    while (1) {}
#  endif

#  if 1
    const bool online = wifiIsOnline();
    DEBUG("wgetTest() online=%d", online);
    if (!online)
    {
        static os_timer_t timer;
        os_timer_disarm(&timer);
        os_timer_setfn(&timer, (os_timer_func_t *)wgetTest, NULL);
        os_timer_arm(&timer, 1000, 0); // 1s, once
        return;
    }

    static WGET_STATE_t wgetState;
    DEBUG("wgetTest() wgetState=%p", &wgetState);
    if (wgetRequest(&wgetState,
            "http://user:pass@foo.bar/tschenggins-status.pl?cmd=hello", sWgetTestCb, NULL, 2048, 0))
          //"http://user:pass@foo.bar/tschenggins-status.pl?cmd=hello;redirect=hello", sWgetTestCb, NULL, 2048, 0))
          //"http://foo.bar/tschenggins-status.pl?cmd=hello", sWgetTestCb, NULL, 2048, 0))
          //"http://foo.bar/tschenggins-status.pl?cmd=dfasdf", sWgetTestCb, NULL, 2048, 0))
          //"http://foo.bar/tschenggins-status.pl?cmd=rawdb;redirect=hello", sWgetTestCb, NULL, 2048, 0))
          //"http://foo.bar/tschenggins-status.pl?cmd=rawdb;debug=1", sWgetTestCb, NULL, 4096, 0))
    {
        PRINT("wgetTest() RUN");
    }
    else
    {
        ERROR("wgetTest() FAIL");
    }
#  endif
}
#endif


/* ********************************************************************************************** */
//@}
// eof
