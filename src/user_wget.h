/*!
    \file
    \brief flipflip's Tschenggins Lämpli: HTTP requests (see \ref USER_WGET)

    - Copyright (c) 2017 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

    \defgroup USER_WGET WGET
    \ingroup USER

    This implements HTTP requests (currently only GET requests) with hostname lookup, query
    parameters, http and https support, response status handling, timeout handling, following
    redirections and optional authentication.

    @{
*/
#ifndef __USER_WGET_H__
#define __USER_WGET_H__

#include "user_stuff.h"
#include "user_config.h"


//! initialise http request stuff
void wgetInit(void);

//! http request status
typedef enum WGET_ERROR_e
{
    WGET_OK,      //!< all okay, response is valid
    WGET_WARNING, //!< not all okay, response only partially valid (status code valid, but body may be missing or incomplete)
    WGET_ERROR    //!< request failed (see debug output for details)

} WGET_ERROR_t;

//! http request response callback data
typedef struct WGET_RESPONSE_s
{
    int   status;         //!< status code (typically 200 if everything is okay, see e.g. https://en.wikipedia.org/wiki/List_of_HTTP_status_codes)
    char *contentType;    //!< content type of resonse if the server provided it (e.g. "text/json", "image/png", etc.)
    char *body;           //!< response body data
    int   bodyLen;        //!< length of response body data
    WGET_ERROR_t error;   //!< error status
#if (USER_WGET_MAX_REDIRECTS == 0)
    char *location;       //!< location to redirect to (goes with a 3xx \c status)
#endif
} WGET_RESPONSE_t;

//! http request response callback function signature
typedef void (WGET_REQCB_t)(const WGET_RESPONSE_t *pkResp, void *pUser);

//! internal http request state structure
typedef struct WGET_STATE_s
{
    struct espconn  conn; // must be first!
    struct _esp_tcp tcp;
    ip_addr_t       hostip;
    WGET_RESPONSE_t resp;
    WGET_REQCB_t   *respCb;
    char           *buf;
    int             bufLen;
    int             bufMax;
    const char     *pkHost;
    const char     *pkPath;
    const char     *pkQuery;
    const char     *pkAuth;
    uint16_t        port;
    bool            https;
    uint8_t         redirCnt;
    bool            complete;
    bool            cbDone;
    uint16_t        timeout;
    void *pUser;
} WGET_STATE_t;

//! http GET request on url with given parameters
/*!
    \param[in] pState      state memory
    \param[in] url         URL to request (see wgetReqParamsFromUrl() for details)
    \param[in] respCb      response callback function
    \param[in] pUser       optional user argument, passed-through to response callback function
    \param[in] respMaxLen  maximum response length to handle (translates to amount of heap memory allocated)
    \param[in] timeout     response timeout [ms] (0 for #USER_WGET_DEFAULT_TIMEOUT)

    \returns true if parameters were acceptable and the request has been successfully initiated, false otherwise

    Example:

\code{.c}
// forward declaration
static void sWgetTestCb(const WGET_RESPONSE_t *pkResp, void *pUser);

void ICACHE_FLASH_ATTR someFunction(void)
{
    static WGET_STATE_t sWgetState; // make sure this is static or malloc()ed, it won't work if it is on the stack
    const char *url = "http://user:pass@foo.bar/tschenggins-status.pl?cmd=hello";
    if (wgetRequest(&wgetState, url, sWgetTestCb, NULL, 2048, 0))
    {
        PRINT("HTTP GET request intitiated... waiting for response callback to be called");
    }
    else
    {
        ERROR("failed initiating HTTP GET request");
    }
}

// response handler
static void ICACHE_FLASH_ATTR sWgetTestCb(const WGET_RESPONSE_t *pkResp, void *pUser)
{
    DEBUG("sWgetTestCb(%p, %p) error=%d status=%d contentType=%s bodyLen=%d",
        pkResp, pUser, pkResp->error, pkResp->status, pkResp->contentType, pkResp->bodyLen);
    UNUSED(pUser);

    // do something with pkResp->body
    if (pkResp->error == WGET_OK)
    {
        PRINT("body=%s", pkResp->body);
    }
    // handle errors
    else
    {
        ERROR("request fail!");
    }
}
\endcode
*/
bool wgetRequest(WGET_STATE_t *pState, const char *url, WGET_REQCB_t respCb, void *pUser, const int respMaxLen, const uint16_t timeout);

//! verify URL for consistency and split into parts
/*!
    Takes an URL, copies it to the output buffer and adds NULs ('\0') appropriately to split it into
    the individual parts. Optional login credentials are converted to a HTTP basic auth token, which
    is placed at the end of the buffer. See examples below.

    Suported URLs have the format "prot://user:pass@host:port/path?query" where "prot" can be http or
    https and the "user:pass@", ":port", "/path" and "?query" parts are optional.

    \param[in]  url       the URL to check, can be ROM string (see PSTR())
    \param[out] buf       buffer to put the parts into (may be same as \c url)
    \param[out] bufSize   buffer size
    \param[out] host      pointer to hostname part
    \param[out] path      pointer to query path part
    \param[out] query     pointer to query parameters part
    \param[out] auth      pointer to HTTP basic auth token
    \param[out] https     boolean to indicate https (true) or http (false) protocol
    \param[out] port      port number
    \returns 0 on error, and otherwise the length of the buffer used (not including the auth token)

Examples:
\code{.c}
const char *url = PSTR("http://user:pass@foo.com/path?foo=bar");
char buf[70];
const char *host;
const char *path;
const char *query;
const char *auth;
bool https;
uint16_t port;
const int res = wgetReqParamsFromUrl(url, buf, sizeof(buf), &host, &path, &query, &auth, &https, &port);
if (res > 0)
{
    // now buf looks like this (where $ indicates NUL string terminators and . indicate unused parts)
    // 0123456789012345678901234567890123456789012345678901234567890123456789
    // .................foo.com$path$foo=bar$...................dXNlcjpwYXNz$
    //                  ^       ^    ^                          ^
    // -->            *host   *path *query                    *auth      and res = 36, port = 80, https = false
}
// else handle errors
\endcode
*/
int wgetReqParamsFromUrl(const char *url, char *buf, const int bufSize,
    const char **host, const char **path, const char **query, const char **auth, bool *https, uint16_t *port);


void wgetTest(void);


#endif // __USER_WGET_H__
//@}
// eof
