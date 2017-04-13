/*!
    \file
    \brief flipflip's Tschenggins Lämpli: xxx (see \ref USER_HTTPD)

    - Copyright (c) 2017 Philippe Kehl <flipflip at oinkzwurgl dot org>,
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

    \defgroup USER_HTTPD HTTPD
    \ingroup USER

    This implements a HTTP server.

    @{
*/
#ifndef __USER_HTTPD_H__
#define __USER_HTTPD_H__

#include "user_stuff.h"


//! initialise HTTP server
void httpdInit(void);

//! start HTTP server (start listening on port 80)
void httpdStart(void);

//! print httpd status via DEBUG()
void httpdStatus(void);

//! http server authentication levels
typedef enum HTTPD_AUTH_LEVEL_e
{
    HTTPD_AUTH_PUBLIC,  //!< public
    HTTPD_AUTH_USER,    //!< requires user (or admin) login (if a user password is set)
    HTTPD_AUTH_ADMIN,   //!< requires admin login (if a admin password is set)

} HTTPD_AUTH_LEVEL_t;

//! the maximum number of GET/POST parameters that are handled
#define HTTP_NUMPARAM 25

//! http server request callback information
typedef struct HTTPD_REQCB_INFO_s
{
    const char        *path;                 //!< the path of the request (e.g. "/foobar" for http://.../foobar?query)
    const char        *host;                 //!< the hostname being requested
    const char        *method;               //!< request method ("GET" or "POST")
    HTTPD_AUTH_LEVEL_t authRequired;         //!< required authentication level
    HTTPD_AUTH_LEVEL_t authProvided;         //!< provided authentication level (always >= \c authRequired)

    // query parameters
    int                numKV;                //!< number of query parameters in request
    const char        *keys[HTTP_NUMPARAM];  //!< list of query parameter names (length \c numKV)
    const char        *vals[HTTP_NUMPARAM];  //!< list of query parameter values (length \c numKV)

} HTTPD_REQCB_INFO_t;

//! http server request callback function signature
typedef bool (HTTPD_REQCB_FUNC_t)(struct espconn *pConn, const HTTPD_REQCB_INFO_t *pkInfo);

//! register http server request callback
/*
    \param[in] path       path to register (e.g. "/foobar" for http://.../foobar?query requests)
    \param[in] authLevel  required authentication level
    \param[in] reqCb      callback function to handle the request
    \returns true on success, false otherwise (list full, see #HTTPD_REQUESTCB)
*/
bool httpdRegisterRequestCb(const char *path, const HTTPD_AUTH_LEVEL_t authLevel, HTTPD_REQCB_FUNC_t reqCb);

//! maximum number of http server paths that can be registered
#define HTTPD_REQUESTCB_NUM 20

//! http server connection callback user data
typedef struct HTTPD_CONN_DATA_s
{
    void    *p;   //!< a pointer
    int32_t  i;   //!< an unsigned integet
    uint32_t u;   //!< a signed integer
} HTTPD_CONN_DATA_t;

//! http server connection callback states
typedef enum HTTPD_CONNCB_e
{
    HTTPD_CONNCB_CONNECT,  //!< called after client connected
    HTTPD_CONNCB_RECEIVED, //!< called (every time) client data has been received
    HTTPD_CONNCB_SENT,     //!< called (every time) data has been sent to the client
    HTTPD_CONNCB_ABORT,    //!< called on connection abort
    HTTPD_CONNCB_CLOSE     //!< called on connection termination
} HTTPD_CONNCB_t;

//! http request data sent (send complete) callback signature
typedef bool (HTTPD_CONNCB_FUNC_t)(struct espconn *pConn, HTTPD_CONN_DATA_t *pData, const HTTPD_CONNCB_t reason);

//! reqister http server connection callback
/*!
    \param[in,out] pConn    network connection handle
    \param[in]     pkTempl  template for the callback user data
    \param[in]     connCb   callback function

    See \ref USER_FS for an example application where this is used to send large files from flash in
    chunks (i.e. send next chunk once sending the previous chunk has completed).
*/
void httpdRegisterConnCb(struct espconn *pConn, const HTTPD_CONN_DATA_t *pkTempl, HTTPD_CONNCB_FUNC_t connCb);

//! maximum number of connections we can maintain in parallel
#define HTTPD_CONN_NUM 10

//! set authentication information for the given level (public or admin)
/*!
    \param[in] authLevel  authentication level to set credentials for (#HTTPD_AUTH_USER or #HTTPD_AUTH_ADMIN)
    \param[in] username   username (see #HTTPD_USER_LEN_MAX)
    \param[in] password   password (see #HTTPD_PASS_LEN_MAX)

*/
bool httpdSetAuth(const HTTPD_AUTH_LEVEL_t authLevel, const char *username, const char *password);

//! maximum username length
#define HTTPD_USER_LEN_MAX 16
//! maximum password length
#define HTTPD_PASS_LEN_MAX 16


//! send generic http response
/*!
    \param[in,out] pConn     network connection handle
    \param[in]     status    status code (e.g. "200 OK", can be ROM data)
    \param[in]     headers   (extra) headers (e.g. "Content-Type: image/png\r\n", can be ROM data)
    \param[in]     body      the body (can be ROM data)
    \param[in]     bodySize  size of the body data
    \returns true on success
*/
bool ICACHE_FLASH_ATTR httpdSendResponse(
    struct espconn *pConn, const char *status, const char *headers,
    const uint8_t *body, const int bodySize);

//! send HTML page using \ref USER_HTML template
/*
    \param[in,out] pConn     network connection handle
    \param[in]     content   content for HTML page (can be a ROM string)
    \param[in]     noMenu    set to true to remove menu from template
    \returns true on success
*/
bool httpSendHtmlPage(struct espconn *pConn, const char *content, const bool noMenu);

//! send generic HTTP error
/*
    This sends a generic http error, such as "404 Not Found", as HTTP status and corresponding
    minimal HTML content that will display in the browser.

    \param[in,out] pConn        network connection handle
    \param[in]     path         path (only used for debugging)
    \param[in]     status       HTTP status code (see below)
    \param[in]     xtraHeaders  extra headers (e.g. "Location: http://foo.bar\r\n", can be ROM string) or #NULL
    \param[in]     xtraMessage  extra message (e.g. "Zut alors!", can be ROM string) or #NULL

    The following status codes are supported. All others translate to "500 Internal Server Error".
    - 204 No Contents
    - 302 Found
    - 400 Bad Request
    - 401 Unauthorized
    - 403 Forbidden
    - 404 Not Found
    - 503 Service Unavailable
*/
bool httpdSendError(struct espconn *pConn, const char *path, const int status, const char *xtraHeaders, const char *xtraMessage);

//! send data to client
/*
    \param[in,out] pConn  network connection handle
    \param[in]     data   data to send
    \param[in]     size   size of data
*/
bool httpdSendData(struct espconn *pConn, uint8_t *data, uint16_t size);


#endif // __USER_HTTPD_H__
//@}
// eof
