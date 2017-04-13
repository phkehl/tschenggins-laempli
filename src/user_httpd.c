// by Philippe Kehl <flipflip at oinkzwurgl dot org>

#include "user_httpd.h"
#include "user_stuff.h"
#include "user_status.h"
#include "user_html.h"
#include "user_cfg.h"
#include "base64.h"
#include "html_gen.h"
#include "captdns.h"


/* ***** httpd ********************************************************************************** */

typedef struct HTTPD_REQCB_ENTRY_s
{
    char                path[32];
    HTTPD_AUTH_LEVEL_t  auth;
    HTTPD_REQCB_FUNC_t *func;

} HTTPD_REQCB_ENTRY_t;

typedef struct HTTPD_CONN_DATA_STORE_s
{
    uint32_t             remote_ip;
    int                  remote_port;
    HTTPD_CONNCB_FUNC_t *connCb;
    HTTPD_CONN_DATA_t    data;
} HTTPD_CONN_DATA_STORE_t;

// forward declarations
static void sHttpdConnectCb(void *pArg);
static bool sHttpdConnDataSet(struct espconn *pConn);
static HTTPD_CONN_DATA_STORE_t *sHttpdConnDataGet(struct espconn *pConn);
static void sHttpdConnDataDel(struct espconn *pConn);
static void sHttpdRecvCb(void *pArg, char *data, uint16_t size);
static void sHttpdSentCb(void *pArg);
static void sHttpdReconCb(void *pArg, int8_t err);
static void sHttpdDisconCb(void *pArg);
static bool sHttpdHandleRequest(struct espconn *pConn, char *data, const uint16_t size);
static int sHttpdSplitQueryString(char *str, const char *keys[], const char *vals[], const int num);
static HTTPD_REQCB_ENTRY_t *sHttpdGetRequestCb(const char *path);

// statistics
static int32_t  sActiveConnCnt;
static uint32_t sHttpdConnectCnt;
static uint32_t sHttpdRecvCnt;
static uint32_t sHttpdRecvSize;
static uint32_t sHttpdReconCnt;
static uint32_t sHttpdDisconCnt;
static uint32_t sHttpdSendCnt;
static uint32_t sHttpdSendSize;
static uint32_t sHttpdHandleCnt;
static uint32_t sHttpdHandleGetCnt;
static uint32_t sHttpdHandlePostCnt;

// user and admin authentication ("auth basic" style, base64)
#define HTTPD_AUTHLEN(ulen, plen) BASE64_ENCLEN((ulen) + 1 + (plen))
#define HTTPD_AUTHLEN_MAX HTTPD_AUTHLEN(HTTPD_USER_LEN_MAX, HTTPD_PASS_LEN_MAX)
static char sHttpdAuthUser[HTTPD_AUTHLEN_MAX];
static char sHttpdAuthAdmin[HTTPD_AUTHLEN_MAX];

static const char *skHttpdAuthLevelStrs[] = { "PUBLIC", "USER", "ADMIN" };

HTTPD_REQCB_ENTRY_t sHttpdReqCbs[HTTPD_REQUESTCB_NUM];

HTTPD_CONN_DATA_t sHttpdConns[HTTPD_CONN_NUM];

// http request debugging
#if 0
#  warning REQ_DEBUG is on
#  define REQ_DEBUG(...) DEBUG(__VA_ARGS__)
#  define IF_REQ_DEBUG(...) __VA_ARGS__
#else
#  define REQ_DEBUG(...) /* nothing */
#  define IF_REQ_DEBUG(...) /* nothing */
#endif

// -------------------------------------------------------------------------------------------------

// http server FIXME: add error handling
void ICACHE_FLASH_ATTR httpdStart(void)
{
    static struct espconn   sHttpdConn;
    static struct _esp_tcp  sHttpdTcp;
    //static        ip_addr_t sHttpdAddr;
    //static        char      sHttpdResponse[1024];
    //static        uint16_t  sHttpdRespLen;

    espconn_disconnect(&sHttpdConn);
    os_memset(&sHttpdConn, 0, sizeof(sHttpdConn));
    os_memset(&sHttpdTcp, 0, sizeof(sHttpdTcp));
    sHttpdConn.type      = ESPCONN_TCP;
    sHttpdConn.state     = ESPCONN_NONE;
    sHttpdConn.proto.tcp = &sHttpdTcp;
    sHttpdTcp.local_port = 80;

    espconn_regist_connectcb(&sHttpdConn, sHttpdConnectCb);

    //espconn_secure_set_default_certificate(default_certificate, default_certificate_len);
    //espconn_secure_set_default_private_key(default_private_key, default_private_key_len);

    //espconn_secure_accept(&esp_conn);
    espconn_accept(&sHttpdConn);

    PRINT("httpd: listen "IPSTR":%u",
        IP2STR(&sHttpdTcp.local_ip), sHttpdTcp.local_port);

    REQ_DEBUG("httpdStart(%p) "IPSTR":%u", &sHttpdConn,
        IP2STR(&sHttpdTcp.local_ip), sHttpdTcp.local_port);

    // setup DNS that points everything to us
    captdnsInit();
}

// -------------------------------------------------------------------------------------------------

HTTPD_CONN_DATA_STORE_t sHttpdConnData[HTTPD_CONN_NUM];

static bool ICACHE_FLASH_ATTR sHttpdConnDataSet(struct espconn *pConn)
{
    const struct _esp_tcp *pkTcp = pConn->proto.tcp;
    int ix = (int)NUMOF(sHttpdConnData);
    bool res = false;
    while (ix--)
    {
        // empty slot?
        if (sHttpdConnData[ix].remote_ip == 0)
        {
            os_memset(&sHttpdConnData[ix], 0, sizeof(sHttpdConnData[ix]));
            sHttpdConnData[ix].remote_ip   = ipaddr_addr((const char *)pkTcp->remote_ip);
            sHttpdConnData[ix].remote_port =                           pkTcp->remote_port;
            res = true;
            break;
        }
    }
    REQ_DEBUG("sHttpdConnDataSet(%p) "IPSTR":%u %s (%p)", pConn,
        IP2STR(&pkTcp->remote_ip), pkTcp->remote_port,
        res ? PSTR("okay") : PSTR("no slot"), res ? &sHttpdConnData[ix] : NULL);

    return res;
}

static HTTPD_CONN_DATA_STORE_t * ICACHE_FLASH_ATTR sHttpdConnDataGet(struct espconn *pConn)
{
    const struct _esp_tcp *pkTcp = pConn->proto.tcp;
    int ix = (int)NUMOF(sHttpdConnData);
    HTTPD_CONN_DATA_STORE_t *pRes = NULL;
    const uint32_t remote_ip   = ipaddr_addr((const char *)pkTcp->remote_ip);
    const int      remote_port =                           pkTcp->remote_port;
    while (ix--)
    {
        // found slot?
        if ( (sHttpdConnData[ix].remote_ip   == remote_ip) &&
             (sHttpdConnData[ix].remote_port == remote_port) )
        {
            pRes = &sHttpdConnData[ix];
            break;
        }
    }

    REQ_DEBUG("sHttpdConnDataGet(%p) "IPSTR":%u %s (%p)", pConn,
        IP2STR(&pkTcp->remote_ip), pkTcp->remote_port,
        pRes != NULL ? PSTR("found") : PSTR("not found"),
        pRes != NULL ? pRes : NULL);

    // should not happen...
    if (pRes == NULL)
    {
        ERROR("sHttpdConnDataGet() WTF?!");
        espconn_abort(pConn);
    }

    return pRes;
}

static void ICACHE_FLASH_ATTR sHttpdConnDataDel(struct espconn *pConn)
{
    IF_REQ_DEBUG( const struct _esp_tcp *pkTcp = pConn->proto.tcp );

    HTTPD_CONN_DATA_STORE_t *pData = sHttpdConnDataGet(pConn);
    if (pData != NULL)
    {
        REQ_DEBUG("sHttpdConnDataDel(%p) "IPSTR":%u (%p)", pConn,
            IP2STR(&pkTcp->remote_ip), pkTcp->remote_port, pData);
        os_memset(pData, 0, sizeof(*pData));
    }
    else
    {
        ERROR("sHttpdConnDataDel() WTF?!");
    }

#if 0
    const struct _esp_tcp *pkTcp = pConn->proto.tcp;
    int ix = (int)NUMOF(sHttpdConnData);
    bool res = false;

    const uint32_t remote_ip   = ipaddr_addr((const char *)pkTcp->remote_ip);
    const int      remote_port =                           pkTcp->remote_port;
    while (ix--)
    {
        if ( (sHttpdConnData[ix].remote_ip   == remote_ip) &&
             (sHttpdConnData[ix].remote_port == remote_port) )
        {
            os_memset(&sHttpdConnData[ix], 0, sizeof(sHttpdConnData[ix]));
            res = true;
            break;
        }
    }

    REQ_DEBUG("sHttpdConnDataDel(%p) "IPSTR":%u %s (%p)", pConn,
        IP2STR(&pkTcp->remote_ip), pkTcp->remote_port,
        res ? PSTR("okay") : PSTR("no slot"), res ? &sHttpdConnData[ix] : NULL);

    if (!res)
    {
        ERROR("sHttpdConnDataDel() WTF?!");
    }
#endif
}

void httpdRegisterConnCb(
    struct espconn *pConn, const HTTPD_CONN_DATA_t *pkTempl, HTTPD_CONNCB_FUNC_t connCb)
{
    HTTPD_CONN_DATA_STORE_t *pDataStore = sHttpdConnDataGet(pConn);
    REQ_DEBUG("httpdRegisterConnCb(%p) connCb=%p pDataStore=%p", pConn, connCb, pDataStore);
    if (pDataStore == NULL)
    {
        return;
    }
    pDataStore->connCb = connCb;
    os_memcpy(&pDataStore->data, pkTempl, sizeof(pDataStore->data));
}



// -------------------------------------------------------------------------------------------------

static void ICACHE_FLASH_ATTR sHttpdConnectCb(void *pArg)
{
    sHttpdConnectCnt++;
    sActiveConnCnt++;

    struct espconn *pConn = pArg; // not the same as &sHttpdConn
    IF_REQ_DEBUG( const struct _esp_tcp *pkTcp = pConn->proto.tcp );
    REQ_DEBUG("sHttpdConnectCb(%p) "IPSTR":%u -> "IPSTR":%u", pConn,
        IP2STR(&pkTcp->remote_ip), pkTcp->remote_port,
        IP2STR(&pkTcp->local_ip), pkTcp->local_port);

    if (!sHttpdConnDataSet(pConn))
    {
        ERROR("httpd: no connection slot");
        espconn_abort(pConn);
        return;
    }

    espconn_regist_sentcb(pConn, sHttpdSentCb);
    espconn_regist_recvcb(pConn, sHttpdRecvCb);
    espconn_regist_reconcb(pConn, sHttpdReconCb);
    espconn_regist_disconcb(pConn, sHttpdDisconCb);

    // call user callback
    HTTPD_CONN_DATA_STORE_t *pDataStore = sHttpdConnDataGet(pConn);
    if ( (pDataStore != NULL) && (pDataStore->connCb != NULL) )
    {
        if (!pDataStore->connCb(pConn, &pDataStore->data, HTTPD_CONNCB_CONNECT))
        {
            ERROR("httpd: conncb connect fail");
            espconn_abort(pConn);
        }
        return;
    }
}

static void ICACHE_FLASH_ATTR sHttpdRecvCb(void *pArg, char *data, uint16_t size)
{
    sHttpdRecvCnt++;
    sHttpdRecvSize += size;

    struct espconn *pConn = pArg;
    const struct _esp_tcp *pkTcp = pConn->proto.tcp;

    //struct _remot_info *pkRemote;
    //(void)espconn_get_connection_info(pConn, &pkRemote, 0x00);
    //const char *skStateStrs[] = { "NONE", "WAIT", "LISTEN", "CONNECT", "WRITE", "READ", "CLOSE" };
    //const char *stateStr = pkRemote->state < NUMOF(skStateStrs) ? skStateStrs[pkRemote->state] : "???";

    REQ_DEBUG("sHttpdRecvCb(%p) "IPSTR":%u size=%u data=%s",
        pConn, IP2STR(&pkTcp->remote_ip), pkTcp->remote_port, size, data);

    // dispatch
    if (size)
    {
        REQ_DEBUG("sHttpdRecvCb(%p) "IPSTR":%u size=%u",
            pConn, IP2STR(&pkTcp->remote_ip), pkTcp->remote_port, size);
        if (!sHttpdHandleRequest(pConn, data, size))
        {
            ERROR("httpd: fail handle request");
            espconn_abort(pConn);
        }
    }
    else
    {
        WARNING("http: empty request "IPSTR":%u",
            IP2STR(&pkTcp->remote_ip), pkTcp->remote_port);
    }

    // call user callback
    HTTPD_CONN_DATA_STORE_t *pDataStore = sHttpdConnDataGet(pConn);
    if ( (pDataStore != NULL) && (pDataStore->connCb != NULL) )
    {
        if (!pDataStore->connCb(pConn, &pDataStore->data, HTTPD_CONNCB_RECEIVED))
        {
            ERROR("httpd: conncb received fail");
            espconn_abort(pConn);
        }
    }
}

static void ICACHE_FLASH_ATTR sHttpdSentCb(void *pArg)
{
    struct espconn *pConn = pArg; // not the same as &sHttpdConn
    IF_REQ_DEBUG( const struct _esp_tcp *pkTcp = pConn->proto.tcp );

    REQ_DEBUG("sHttpdSentCb(%p) "IPSTR":%u -> "IPSTR":%u",
        pConn,
        IP2STR(&pkTcp->local_ip), pkTcp->local_port,
        IP2STR(&pkTcp->remote_ip), pkTcp->remote_port);

    // call user callback
    HTTPD_CONN_DATA_STORE_t *pDataStore = sHttpdConnDataGet(pConn);
    if ( (pDataStore != NULL) && (pDataStore->connCb != NULL) )
    {
        if (!pDataStore->connCb(pConn, &pDataStore->data, HTTPD_CONNCB_SENT))
        {
            ERROR("httpd: conncb sent fail");
            espconn_abort(pConn);
        }
        return;
    }
}

static void ICACHE_FLASH_ATTR sHttpdReconCb(void *pArg, int8_t err)
{
    sHttpdReconCnt++;
    sActiveConnCnt--;

    struct espconn *pConn = pArg;
    const struct _esp_tcp *pkTcp = pConn->proto.tcp;
    WARNING("sHttpdReconCb(%p) "IPSTR":%u %s", pConn,
        IP2STR(&pkTcp->remote_ip), pkTcp->remote_port, espconnErrStr(err));

    // call user callback
    HTTPD_CONN_DATA_STORE_t *pDataStore = sHttpdConnDataGet(pConn);
    if ( (pDataStore != NULL) && (pDataStore->connCb != NULL) )
    {
        pDataStore->connCb(pConn, &pDataStore->data, HTTPD_CONNCB_ABORT);
    }

    sHttpdConnDataDel(pConn);
}

static void ICACHE_FLASH_ATTR sHttpdDisconCb(void *pArg)
{
    sHttpdDisconCnt++;
    sActiveConnCnt--;

    struct espconn *pConn = pArg;
    /*IF_REQ_DEBUG(*/ const struct _esp_tcp *pkTcp = pConn->proto.tcp /*)*/;
    /*REQ_*/DEBUG("sHttpdDisconCb(%p) "IPSTR":%u", pConn,
        IP2STR(&pkTcp->remote_ip), pkTcp->remote_port);

    // call user callback
    HTTPD_CONN_DATA_STORE_t *pDataStore = sHttpdConnDataGet(pConn);
    if ( (pDataStore != NULL) && (pDataStore->connCb != NULL) )
    {
        pDataStore->connCb(pConn, &pDataStore->data, HTTPD_CONNCB_CLOSE);
    }

    sHttpdConnDataDel(pConn);
}


// -------------------------------------------------------------------------------------------------

bool httpdSetAuth(const HTTPD_AUTH_LEVEL_t authLevel, const char *username, const char *password)
{
    char *authStr = NULL;
    switch (authLevel)
    {
        case HTTPD_AUTH_USER:  authStr = sHttpdAuthUser;  break;
        case HTTPD_AUTH_ADMIN: authStr = sHttpdAuthAdmin; break;
        case HTTPD_AUTH_PUBLIC:
        default: break;
    }

    const int ulen = strlen_P(username);
    const int plen = strlen_P(password);
    if ( (authStr != NULL) && (ulen == 0) && (plen == 0) )
    {
        REQ_DEBUG("httpdSetAuth() clear %s", skHttpdAuthLevelStrs[authLevel]);
        authStr[0] = '\0';
        return true;
    }
    else if ( (authStr != NULL) && (ulen > 0) && (plen > 0) &&
        (ulen <= HTTPD_USER_LEN_MAX) && (plen <= HTTPD_PASS_LEN_MAX) &&
        (HTTPD_AUTHLEN(ulen, plen) <= HTTPD_AUTHLEN_MAX) )
    {
        char tmp[HTTPD_USER_LEN_MAX + 1 + HTTPD_PASS_LEN_MAX + 1];
        sprintf_PP(tmp, PSTR("%s:%s"), username, password);
        base64enc(tmp, authStr, HTTPD_AUTHLEN_MAX);
        REQ_DEBUG("httpdSetAuth() %s %s -> %s", skHttpdAuthLevelStrs[authLevel], tmp, authStr);
        return true;
    }
    else
    {
        ERROR("http: set auth (password and/or username too long or too short)");
        return false;
    }
}


// -------------------------------------------------------------------------------------------------

static bool ICACHE_FLASH_ATTR sHttpdHandleRequest(
    struct espconn *pConn, char *data, const uint16_t size)
{
    const struct _esp_tcp *pkTcp = pConn->proto.tcp;

    sHttpdHandleCnt++;
    // GET / HTTP/1.1\r\n
    // Host: 10.1.1.205\r\n
    // User-Agent: Mozilla/5.0 (X11; Ubuntu; Linux x86_64; rv:51.0) Gecko/20100101 Firefox/51.0\r\n
    // Accept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\n
    // Accept-Language: en-US,en;q=0.5\r\n
    // Accept-Encoding: gzip, deflate\r\n
    // DNT: 1\r\n
    // Connection: keep-alive\r\n
    // Upgrade-Insecure-Requests: 1\r\n
    // \r\n\r\n

    // curl -o- -D- --raw http://10.1.1.205
    // curl --data "param1=value1&param2=value2" https://10.1.1.205/meier

    // curl --user user:password --basic -o- -D- --raw http://10.1.1.205/test
    // Authorization: Basic dXNlcjpwYXNzd29yZA==\r\n

    // ***** parse HTTP request *****

    HTTPD_REQCB_INFO_t cbInfo;
    os_memset(&cbInfo, 0, sizeof(cbInfo));
    char staName[32];
    const char *auth = NULL;
    {
        // get first line
        char *endOfFirstLine = os_strstr(data, "\r\n");
        if (endOfFirstLine == NULL)
        {
            WARNING("sHttpdHandleRequest(%p) incomplete request", pConn);
            return false;
        }
        *endOfFirstLine = '\0';
        const char *firstLine = data;
        const int firstLineLen = firstLine ? os_strlen(firstLine) : 0;
        REQ_DEBUG("sHttpdHandleRequest(%p) firstLine=%s (%d)",
            pConn, firstLine, firstLineLen);

        // we want to see "HTTP/1.1" at the end of the line
        char *httpCheck = os_strstr(firstLine, "HTTP/1.1");
        if ( (httpCheck == NULL) || ((firstLineLen - (httpCheck - firstLine)) != 8) )
        {
            WARNING("sHttpdHandleRequest(%p) not HTTP/1.1 (%s, %d)",
                pConn, firstLine, firstLineLen - (httpCheck - firstLine));
        }
        *(httpCheck - 1) = '\0';

        // extract path and query strings
        char *firstSpace = os_strstr(firstLine, " ");
        if (firstSpace == NULL)
        {
            WARNING("sHttpdHandleRequest(%p) malformed request (%s)",
                pConn, firstLine);
            return false;
        }
        *firstSpace = '\0';
        cbInfo.path = firstSpace + 1;
        cbInfo.method = firstLine;
        char *questionMark = os_strstr(cbInfo.path, "?");
        char *query = NULL;
        if (questionMark != NULL)
        {
            *questionMark = '\0';
            query = questionMark + 1;
        }
        REQ_DEBUG("sHttpdHandleRequest(%p) cbInfo.method=%s query=%s (%d)",
            pConn, cbInfo.method, query, query ? os_strlen(query) : 0);

        // first word GET or POST
        bool isPOST = false;
        if (os_strcmp("GET", cbInfo.method) == 0)
        {
            sHttpdHandleGetCnt++;
        }
        else if (os_strcmp("POST", cbInfo.method) == 0)
        {
            isPOST = true;
            sHttpdHandlePostCnt++;
        }
        else
        {
            WARNING("sHttpdHandleRequest(%p) illegal method %s",
                pConn, cbInfo.method);
            return false;
        }

        // get request headers and body
        char *restOfData = &endOfFirstLine[2];
        char *body = NULL;
        const char *headers = NULL;
        if (os_strlen(restOfData) > 4)
        {
            char *tmp = os_strstr(restOfData, "\r\n\r\n");
            if (tmp != NULL)
            {
                *tmp = '\0';
                body = tmp + 4;
                headers = restOfData;
            }
        }
        REQ_DEBUG("sHttpdHandleRequest(%p) body=%s (%d)", pConn, body, os_strlen(body));
        REQ_DEBUG("sHttpdHandleRequest(%p) headers=%s (%d)", pConn, headers, os_strlen(headers));

        // extract headers (authorization, host)
        if (headers != NULL)
        {
            const char *host = strcasestr_P(headers, PSTR("Host: "));
            auth = strcasestr_P(headers, PSTR("Authorization: Basic "));
            if (auth != NULL)
            {
                auth += 21; // "Authorization: Basic "
                char *authEnd = os_strstr(auth, "\r\n");
                if (authEnd != NULL)
                {
                    *authEnd = '\0';
                    if (os_strlen(auth) > 3)
                    {
                        REQ_DEBUG("sHttpdHandleRequest(%p) auth=%s", pConn, auth);
                    }
                    else
                    {
                        WARNING("sHttpdHandleRequest(%p) fishy authentication (%s)",
                            pConn, auth);
                        auth = NULL;
                    }
                }
            }

            if (host != NULL)
            {
                char *hostEnd = os_strstr(host, "\r\n");
                if (hostEnd != NULL)
                {
                    *hostEnd = '\0';
                    host += 6; // "Host: "
                    cbInfo.host = host;
                }
            }
        }

        // redirect to correct hostname on ap network (with fake DNS, but not for the captive portal path thingy)
        if (os_strcmp(cbInfo.path, "/generate_204") != 0)
        {
            USER_CFG_t userCfg;
            cfgGet(&userCfg);
            const char *hostFound = os_strstr(cbInfo.host, userCfg.apSsid);
            if ( wifiIsApNet(pkTcp->remote_ip) && (hostFound == NULL) )
            {
                char location[100];
                sprintf_PP(location, PSTR("Location: http://%s.local/\r\n"), userCfg.apSsid);
                REQ_DEBUG("sHttpdHandleRequest(%p) %s != %s --> %s",
                    pConn, cbInfo.host, userCfg.apSsid, location);
                return httpdSendError(pConn, cbInfo.path, 302, location, NULL);
            }
        }

        // parse query string
        int numKV = 0;
        if ((query != NULL) /*&& !isPOST*/)
        {
            numKV += sHttpdSplitQueryString(query, &cbInfo.keys[numKV], &cbInfo.vals[numKV], HTTP_NUMPARAM - numKV);
        }
        if ((body != NULL) && isPOST)
        {
            numKV += sHttpdSplitQueryString(body, &cbInfo.keys[numKV], &cbInfo.vals[numKV], HTTP_NUMPARAM - numKV);
        }
        cbInfo.numKV = numKV;

        // assert that cbInfo.host is not empty
        if (cbInfo.host == NULL)
        {
            USER_CFG_t userCfg;
            cfgGet(&userCfg);
            os_strncpy(staName, userCfg.staName, sizeof(staName));
            staName[sizeof(staName)-1] = '\0';
            cbInfo.host = staName;
        }
    }

    // what we know so far
    REQ_DEBUG("sHttpdHandleRequest(%p) method=%s path=%s numKV=%d/%d",
        pConn, cbInfo.method, cbInfo.path, cbInfo.numKV, HTTP_NUMPARAM);


    // ***** dispatch request *****

    // check if we have a callback registered for this path
    const HTTPD_REQCB_ENTRY_t *pkEntry = sHttpdGetRequestCb(cbInfo.path);

    if (pkEntry != NULL)
    {
        // check authentication
        cbInfo.authRequired = pkEntry->auth;
        cbInfo.authProvided = HTTPD_AUTH_PUBLIC;
        if ( (pkEntry->auth != HTTPD_AUTH_PUBLIC) || (auth != NULL) )
        {
            // no auth configured or right auth given?
            const bool isUser  = (sHttpdAuthUser[0]  == '\0') ||
                ( (auth != NULL) && (os_strcmp(sHttpdAuthUser,  auth) == 0) ) ? true : false;
            const bool isAdmin = (sHttpdAuthAdmin[0] == '\0') ||
                ( (auth != NULL) && (os_strcmp(sHttpdAuthAdmin, auth) == 0) ) ? true : false;
            REQ_DEBUG("sHttpdDispatchRequest(%p) need %s auth (have %s auth)",
                pConn, skHttpdAuthLevelStrs[pkEntry->auth], isAdmin ? "admin" : (isUser ? "user" : "wrong"));

            bool authOk = false;
            //bool reqAuth = false;
            switch (pkEntry->auth)
            {
                case HTTPD_AUTH_PUBLIC:
                    authOk = true;
                    break;
                case HTTPD_AUTH_USER:
                    authOk = isUser || isAdmin;
                    //reqAuth = auth == NULL ? true : false;
                    break;
                case HTTPD_AUTH_ADMIN:
                    authOk = isAdmin;
                    //reqAuth = auth == NULL ? true : false;
                    break;
            }
            if (isAdmin)
            {
                cbInfo.authProvided = HTTPD_AUTH_ADMIN;
            }
            else if (isUser)
            {
                cbInfo.authProvided = HTTPD_AUTH_USER;
            }

            if ( (auth == NULL) && (pkEntry->auth != HTTPD_AUTH_PUBLIC) )
            {
                WARNING("http: %s no %s password set",
                    cbInfo.path, skHttpdAuthLevelStrs[cbInfo.authRequired]);
            }

            // wrong auth given
            /*if (!authOk && !reqAuth)
            {
                const char *status = HTTP_STATUS_403_STR;
                REQ_DEBUG("sHttpdDispatchRequest(%p) %s", pConn, status);
                httpdSendResponse(pConn, 403, skHttpContentTypeTextPlain, status, sizeof(HTTP_STATUS_403_STR) - 1);
                return;
            }
            // no auth given, but required -> request authorisation from user
            else*/ if (!authOk)
            {
                USER_CFG_t userCfg;
                cfgGet(&userCfg);

                char headers[100];
                os_sprintf(headers, "WWW-Authenticate: Basic realm=\"%s %s login\"\r\n",
                    userCfg.staName, skHttpdAuthLevelStrs[pkEntry->auth]);
                return httpdSendError(pConn, cbInfo.path, 401, headers, NULL);
            }
        }
    }

    // call the registered callback
    if (pkEntry != NULL)
    {
        PRINT("httpd: "IPSTR":%u %s (%s, %s, %s, %d)",
            IP2STR(&pkTcp->remote_ip), pkTcp->remote_port, cbInfo.path, cbInfo.host,
            skHttpdAuthLevelStrs[cbInfo.authProvided], skHttpdAuthLevelStrs[cbInfo.authRequired], cbInfo.numKV);
        if (!pkEntry->func(pConn, &cbInfo))
        {
            return false;
        }
    }

    // default response
    else
    {
        return httpdSendError(pConn, cbInfo.path, 404, NULL, PSTR("Zut alors!"));
    }

    return true;
}

// split query string and add to keys/vals arrays up to num items
static int ICACHE_FLASH_ATTR sHttpdSplitQueryString(char *str, const char *keys[], const char *vals[], const int num)
{
    //DEBUG("sHttpdSplitQueryString() %s", str);
    char *saveptr = NULL;
    int ix = 0;
    while (ix < num)
    {
        // FIXME: use strsep()
        char *key = strtok_r(saveptr == NULL ? str : NULL, "&;", &saveptr);
        if (key != NULL)
        {
            char *val = os_strstr(key, "=");
            if (val)
            {
                *val = '\0';
                val++;
            }
            if (os_strlen(key))
            {
                urlDecode(key);
                urlDecode(val);
                keys[ix] = key;
                vals[ix] = val;
                //DEBUG("sHttpdSplitQueryString() ix=%d key=%s val=%s", ix, key, val);//keys[numKV], vals[numKV]);
                ix++;
            }
        }
        else
        {
            break;
        }
        if (saveptr == NULL)
        {
            break;
        }
    }
    if (saveptr != NULL)
    {
        WARNING("sHttpdSplitQueryString() too many parameters!");
    }
    return ix;
}


// -------------------------------------------------------------------------------------------------

// low-level send
bool ICACHE_FLASH_ATTR httpdSendData(struct espconn *pConn, uint8_t *data, uint16_t size)
{
    const int8_t res = espconn_send(pConn, data, size);
    if (res != ESPCONN_OK)
    {
        ERROR("http: send fail (%s)", espconnErrStr(res));
        return false;
    }
    else
    {
        REQ_DEBUG("httpdSendData(%p) OK (%p, %u)", pConn, data, size);
        sHttpdSendCnt++;
        sHttpdSendSize += size;
        return true;
    }
}


// send generic http response
bool ICACHE_FLASH_ATTR httpdSendResponse(
    struct espconn *pConn, const char *status, const char *headers,
    const uint8_t *body, const int bodySize)
{
    const struct _esp_tcp *pkTcp = pConn->proto.tcp;

    const uint16_t allocSize =
        8 + 1 + strlen_P(status) + 2  // HTTP/1.1 200 OK\r\n
        + 16 + 5 + 2                  // Content-Length: .....\r\n
        + 17 + 2                      // Connection: close\r\n
        + strlen_P(headers)           // Content-Type: text/plain; charset=UTF-8\r\n
        + 2                           // \r\n
        + bodySize;                   // response body data

    uint8_t *pResp = memAlloc(allocSize);
    if (pResp == NULL)
    {
        ERROR("httpdSendResponse(%p) "IPSTR":%u malloc %u fail",
            pConn, IP2STR(&pkTcp->remote_ip), pkTcp->remote_port, allocSize);
        return false;
    }

    // add HTTP status and headers
    sprintf_PP((char *)pResp, PSTR("HTTP/1.1 %s\r\nConnection: close\r\nContent-Length: %u\r\n%s\r\n"),
        status, bodySize, headers);
    uint16_t respSize = os_strlen(pResp);

    // add response body data
    memcpy_P(&pResp[respSize], body, bodySize); // can copy from RAM and ROM
    respSize += bodySize;

    // debug
    REQ_DEBUG("httpdSendResponse(%p) "IPSTR":%u %s %u+%u (%u)",
        pConn, IP2STR(&pkTcp->remote_ip), pkTcp->remote_port,
        status, respSize - bodySize, bodySize, respSize);

    // send data
    const bool res = httpdSendData(pConn, pResp, respSize);
    if (!res)
    {
        ERROR("httpdSendResponse(%p) "IPSTR":%u send %u fail",
            pConn, IP2STR(&pkTcp->remote_ip), pkTcp->remote_port, respSize);
    }

    // clean up
    memFree(pResp);

    return res;
}


// send error response
bool ICACHE_FLASH_ATTR httpdSendError(struct espconn *pConn,
    const char *path, const int status, const char *xtraHeaders, const char *xtraMessage)
{
    const struct _esp_tcp *pkTcp = pConn->proto.tcp;

    const char *pkStatus = PSTR("500 Internal Server Error");
    switch (status)
    {
        case 204: pkStatus = PSTR("204 No Contents");  break;
        case 302: pkStatus = PSTR("302 Found");        break;
        case 400: pkStatus = PSTR("400 Bad Request");  break;
        case 401: pkStatus = PSTR("401 Unauthorized"); break;
        case 403: pkStatus = PSTR("403 Forbidden");    break;
        case 404: pkStatus = PSTR("404 Not Found");    break;
        case 503: pkStatus = PSTR("503 Service Unavailable"); break;
        default: break;
    }

    char resp[512];
    {
        char body[512];
        sprintf_PP(body, PSTR("<html><head><title>%s</title></head><body><h1>%s</h1>%s</body></html>"),
            pkStatus, pkStatus, xtraMessage != NULL ? xtraMessage : "");
        const int bodyLen = os_strlen(body);

        if (xtraHeaders == NULL)
        {
            xtraHeaders = "";
        }

        char head[256];
        sprintf_PP(head, PSTR("HTTP/1.1 %s\r\n"
            "Content-Length: %d\r\n"
            "Content-Type: text/html; charset=UTF-8\r\n"
            "%s\r\n"), pkStatus, bodyLen, xtraHeaders);

        os_strcpy(resp, head);
        os_strcat(resp, body);
    }
    const uint16_t respSize = os_strlen(resp);

    // debug
    REQ_DEBUG("httpdSendError(%p) "IPSTR":%u %s %u",
        pConn, IP2STR(&pkTcp->remote_ip), pkTcp->remote_port,
        pkStatus, respSize);

    // send data
    if (status >= 400)
    {
        WARNING("httpd: "IPSTR":%u %s %s",
            IP2STR(&pkTcp->remote_ip), pkTcp->remote_port, path, pkStatus);
    }
    else
    {
        PRINT("httpd: "IPSTR":%u %s %s",
            IP2STR(&pkTcp->remote_ip), pkTcp->remote_port, path, pkStatus);
    }

    return httpdSendData(pConn, (uint8_t *)resp, respSize);
}

// -------------------------------------------------------------------------------------------------

// find callback for a path
static HTTPD_REQCB_ENTRY_t * ICACHE_FLASH_ATTR sHttpdGetRequestCb(const char *path)
{
    // check if we have a callback registered for this path
    HTTPD_REQCB_ENTRY_t *pEntry = NULL;
    for (int ix = 0; ix < (int)NUMOF(sHttpdReqCbs); ix++)
    {
        if (sHttpdReqCbs[ix].path[0] && (os_strcmp(sHttpdReqCbs[ix].path, path) == 0))
        {
            pEntry = &sHttpdReqCbs[ix];
            break;
        }
    }
    return pEntry;
}

// register callback for a path
bool ICACHE_FLASH_ATTR httpdRegisterRequestCb(
    const char *path, const HTTPD_AUTH_LEVEL_t authLevel, HTTPD_REQCB_FUNC_t reqCb)
{
    const int pathLen = strlen_P(path);

    // check parameters
    if (pathLen > (int)(sizeof(sHttpdReqCbs[0].path) - 1))
    {
        ERROR("httpdRegisterRequestCb() path=%s too long (> %u)",
            path, sizeof(sHttpdReqCbs[0].path) - 1);
        return false;
    }
    switch (authLevel)
    {
        case HTTPD_AUTH_PUBLIC:
        case HTTPD_AUTH_USER:
        case HTTPD_AUTH_ADMIN:
            break;
        default:
            ERROR("httpdRegisterRequestCb() illegal authLevel!");
            return false;
    }

    char pathCopy[pathLen + 1];
    strcpy_P(pathCopy, path);

    if (sHttpdGetRequestCb(pathCopy) != NULL)
    {
        ERROR("httpdRegisterRequestCb() path=%s already registered", pathCopy);
        return false;
    }

    // find empty slot
    bool res = false;
    for (int ix = 0; ix < (int)NUMOF(sHttpdReqCbs); ix++)
    {
        HTTPD_REQCB_ENTRY_t *pEntry = &sHttpdReqCbs[ix];
        if (os_strlen(pEntry->path) > 0)
        {
            continue;
        }
        os_strcpy(pEntry->path, pathCopy);
        pEntry->auth = authLevel;
        pEntry->func = reqCb;
        DEBUG("httpdRegisterRequestCb() [%d] %s (%s, %p)",
            ix, pEntry->path, skHttpdAuthLevelStrs[pEntry->auth], pEntry->func);
        res = true;
        break;
    }
    if (!res)
    {
        ERROR("httpdRegisterRequestCb() no space for path=%s", pathCopy);
    }

    return res;
}


// -------------------------------------------------------------------------------------------------

// send favicon
static bool ICACHE_FLASH_ATTR sHttpdFaviconRequestPngCb(struct espconn *pConn, const HTTPD_REQCB_INFO_t *pkInfo)
{
    UNUSED(pkInfo);
    static const uint8_t favicon[] PROGMEM =
    {
        0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d, 0x49, 0x48, 0x44, 0x52,
        0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x10, 0x01, 0x03, 0x00, 0x00, 0x00, 0x25, 0x3d, 0x6d,
        0x22, 0x00, 0x00, 0x00, 0x06, 0x50, 0x4c, 0x54, 0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xa5,
        0x67, 0xb9, 0xcf, 0x00, 0x00, 0x00, 0x01, 0x74, 0x52, 0x4e, 0x53, 0x00, 0x40, 0xe6, 0xd8, 0x66,
        0x00, 0x00, 0x00, 0x30, 0x49, 0x44, 0x41, 0x54, 0x08, 0xd7, 0x63, 0xf8, 0x3c, 0x8f, 0xa1, 0x25,
        0x90, 0xa1, 0x45, 0x90, 0xe1, 0xf1, 0x3c, 0x86, 0x86, 0x00, 0x86, 0x96, 0x00, 0x86, 0xcf, 0x13,
        0x18, 0x80, 0xe0, 0xdd, 0x3b, 0x86, 0x45, 0x1d, 0x20, 0x04, 0x64, 0xac, 0x58, 0x05, 0x42, 0x40,
        0x06, 0x03, 0x03, 0x00, 0xac, 0x88, 0x12, 0x30, 0xa2, 0x80, 0xaf, 0x39, 0x00, 0x00, 0x00, 0x00,
        0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
    };
    return httpdSendResponse(pConn, PSTR("200 OK"), PSTR("Content-Type: image/png\r\nCache-Control: max-age=86400\r\n"),
        favicon, sizeof(favicon));;
}

static bool ICACHE_FLASH_ATTR sHttpdFaviconRequestIcoCb(struct espconn *pConn, const HTTPD_REQCB_INFO_t *pkInfo)
{
    UNUSED(pkInfo);
    static const uint8_t favicon[] PROGMEM =
    {
        0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x10, 0x10, 0x02, 0x00, 0x01, 0x00, 0x01, 0x00, 0xb0, 0x00,
        0x00, 0x00, 0x16, 0x00, 0x00, 0x00, 0x28, 0x00, 0x00, 0x00, 0x10, 0x00, 0x00, 0x00, 0x20, 0x00,
        0x00, 0x00, 0x01, 0x00, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
        0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x11, 0x11, 0x00, 0x00, 0x57, 0x55,
        0x00, 0x00, 0x57, 0x55, 0x00, 0x00, 0x11, 0x11, 0x00, 0x00, 0x5d, 0x77, 0x00, 0x00, 0x5d, 0x77,
        0x00, 0x00, 0x11, 0x11, 0x00, 0x00, 0xff, 0xff, 0x00, 0x00, 0x0c, 0x6f, 0x00, 0x00, 0x7b, 0xaf,
        0x00, 0x00, 0x7f, 0xaf, 0x00, 0x00, 0x1c, 0x61, 0x00, 0x00, 0x7b, 0xee, 0x00, 0x00, 0x7b, 0xae,
        0x00, 0x00, 0x0c, 0x61, 0x00, 0x00
    };
    return httpdSendResponse(pConn, PSTR("200 OK"), PSTR("Content-Type: image/x-icon\r\nCache-Control: max-age=86400\r\n"),
        favicon, sizeof(favicon));;
}

#define HTTPD_USE_SIGNIN 1

#if (HTTPD_USE_SIGNIN > 0)
static bool sHttpdSignInState; // FIXME: need one of these per client and per <s>connection</s> session
#endif

static bool ICACHE_FLASH_ATTR sHttpdCaptivePortalRequestCb(struct espconn *pConn, const HTTPD_REQCB_INFO_t *pkInfo)
{
    const struct _esp_tcp *pkTcp = pConn->proto.tcp;

    // redirect if connecting from sta side
    if (!wifiIsApNet(pkTcp->remote_ip))
    {
        struct ip_info ipinfo;
        wifi_get_ip_info(STATION_IF, &ipinfo);
        char location[100];
        sprintf_PP(location, PSTR("Location: http://%s/\r\n"), pkInfo->host);
        return httpdSendError(pConn, pkInfo->path, 302, location, NULL);
    }

    // curl -D- --raw -o- https://www.gstatic.com/generate_204
    // HTTP/1.1 204 No Content
    // Content-Length: 0
    // Date: Tue, 28 Feb 2017 22:42:28 GMT
    // Alt-Svc: quic=":443"; ma=2592000; v="35,34"

#if (HTTPD_USE_SIGNIN > 0)

    if (sHttpdSignInState)
    {
        // this will make Android think there's internet (or so)
        return httpdSendError(pConn, pkInfo->path, 204, NULL, NULL);
    }
    else
    {
        // this will make Android show the sign-in page
        const char *hostname = wifi_station_get_hostname();
        char location[100];
        sprintf_PP(location, PSTR("Location: http://%s.local/signin\r\n"),
            hostname[0] ? hostname : "esp8266.local");
        return httpdSendError(pConn, pkInfo->path, 302, location, NULL);
    }

#else // (HTTPD_USE_SIGNIN > 0)

    return httpdSendError(pConn, path, 204, NULL, NULL);

#endif // (HTTPD_USE_SIGNIN > 0)
}

#if (HTTPD_USE_SIGNIN > 0)

static const char skHttpdSignInHtml[] PROGMEM = USER_HTTPD_SIGNIN_HTML_STR;

static bool ICACHE_FLASH_ATTR sHttpdSignInRequestCb(struct espconn *pConn, const HTTPD_REQCB_INFO_t *pkInfo)
{
    const struct _esp_tcp *pkTcp = pConn->proto.tcp;

    // redirect if connecting from sta side
    if (!wifiIsApNet(pkTcp->remote_ip))
    {
        struct ip_info ipinfo;
        wifi_get_ip_info(STATION_IF, &ipinfo);
        char location[100];
        sprintf_PP(location, PSTR("Location: http://%s/\r\n"), pkInfo->host);
        return httpdSendError(pConn, pkInfo->path, 302, location, NULL);
    }

    // sign-in page response
    if (pkInfo->numKV > 0)
    {
        sHttpdSignInState = true;
        const char *hostname = wifi_station_get_hostname();
        char location[100];
        sprintf_PP(location, ("Location: http://%s.local/\r\n"),
            hostname[0] ? hostname : "esp8266.local");
        return httpdSendError(pConn, pkInfo->path, 302, location, NULL);
    }
    else
    {
        char resp[1000];
        strcpy_P(resp, skHttpdSignInHtml);
        return httpSendHtmlPage(pConn, resp, true);
    }
}
#endif // (HTTPD_USE_SIGNIN > 0)


// -------------------------------------------------------------------------------------------------

static const uint16_t skHttpdHomeIcons[] PROGMEM =
{
    0x2620, 0x2603, 0x2741, 0x263a, 0x0040, 0x2708, 0x2600, 0x2716, 0x272a, 0x2744,
    0x2706, 0x2602, 0x262e, 0x262f, 0x2704, 0x2693, 0x2622, 0x267b, 0x26a0, 0x270c,
    0x265a, 0x2656, 0x2665,
    // unfortunately rarely anyone has these..
    // 0x1f640, 0x1f35f, 0x1f42a, 0x1f366, 0x1f383, 0x1f480, 0x1f3b6, 0x1f40c, 0x1f639,
    // 0x1f680, 0x1f419, 0x1f44f, 0x1f691, 0x1f375, 0x1f6bb, 0x1f682, 0x1f379, 0x1f4f5,
    // 0x1f4a9, 0x1f6c5, 0x1f6a1, 0x1f6ae, 0x1f6b1, 0x1f408, 0x9749,
};

// standard home (/)
static bool ICACHE_FLASH_ATTR sHttpdHomeRequestCb(struct espconn *pConn, const HTTPD_REQCB_INFO_t *pkInfo)
{
    char resp[256];
    //const int iconIx = os_random() % NUMOF(skHttpdHomeIcons);
    static int iconIx;
    iconIx++;
    iconIx %= NUMOF(skHttpdHomeIcons);

    UNUSED(pkInfo);

    sprintf_PP(resp, PSTR("<div class=\"home\"><span><span>&#x%x</span></span></div>"),
        pgm_read_uint16(&skHttpdHomeIcons[iconIx]));

    return httpSendHtmlPage(pConn, resp, false);
}

// -------------------------------------------------------------------------------------------------

static const char skHttpdAboutHtml[] PROGMEM = USER_HTTPD_ABOUT_HTML_STR;

// about (/about)
static bool ICACHE_FLASH_ATTR sHttpdAboutRequestCb(struct espconn *pConn, const HTTPD_REQCB_INFO_t *pkInfo)
{
    UNUSED(pkInfo);
    return httpSendHtmlPage(pConn, skHttpdAboutHtml, false);
}

// -------------------------------------------------------------------------------------------------

#define HTTP_HEAD_SIZE 256
#define HTTP_BODY_SIZE ((8 * 1024) - HTTP_HEAD_SIZE)

// helper to send a full HTML page using the template with htmlRender() (user_html.c)
bool ICACHE_FLASH_ATTR httpSendHtmlPage(struct espconn *pConn, const char *content, const bool noMenu)
{
    const struct _esp_tcp *pkTcp = pConn->proto.tcp;

    uint8_t *resp = memAlloc(HTTP_HEAD_SIZE + HTTP_BODY_SIZE);
    if (resp == NULL)
    {
        ERROR("httpSendHtmlPage() alloc fail");
        return false;
    }

    // generate html
    USER_CFG_t userConfig;
    cfgGet(&userConfig);
    const char *sysId = getSystemId();
    const char *keys[] = { PSTR("CONTENT"), PSTR("SYSNAME"), PSTR("SYSID"), PSTR("MENUSTYLE") };
    const char *vals[] = {  content,   userConfig.staName,         sysId,   noMenu ? PSTR("display: none;") : NULL };
    const int htmlLen = htmlRender(
        NULL, (char *)&resp[HTTP_HEAD_SIZE], HTTP_BODY_SIZE, keys, vals, (int)NUMOF(keys), true);

    // generate HTTP status and headers
    char head[HTTP_HEAD_SIZE];
    sprintf_PP(head, PSTR("HTTP/1.1 200 OK\r\n"
        "Content-Length: %u\r\n"
        "Content-Type: text/html; charset=UTF-8\r\n"
        "Cache-Control: no-cache, no-store, must-revalidate\r\n"
        "Pragma: no-cache\r\n"
        "Expires: -1\r\n"
        "\r\n"), htmlLen);
    const int headLen = os_strlen(head);

    // add header to response
    const int headOffs = HTTP_HEAD_SIZE - headLen;
    memcpy(&resp[headOffs], head, headLen);

    // the response
    uint8_t *pResp = &resp[headOffs];
    const uint16_t respSize = headLen + htmlLen;

    // debug
    //DEBUG("httpSendHtmlPage() headLen=%d htmlLen=%d headOffs=%d respSize=%u",
    //    headLen, htmlLen, headOffs, respSize);
    REQ_DEBUG("httpSendHtmlPage(%p) "IPSTR":%u 200 OK %d+%d=%u",
        pConn, IP2STR(&pkTcp->remote_ip), pkTcp->remote_port,
        headLen, htmlLen, respSize);

    // send data
    const bool res = httpdSendData(pConn, pResp, respSize);
    if (!res)
    {
        ERROR("httpSendHtmlPage(%p) "IPSTR":%u send %u fail",
            pConn, IP2STR(&pkTcp->remote_ip), pkTcp->remote_port, respSize);
    }

    // clean up
    memFree(resp);

    return res;
}

// -------------------------------------------------------------------------------------------------
#define HTTPD_TEST_CALLBACKS 0

// some test callbacks with GET and POST parameters

#if (HTTPD_TEST_CALLBACKS > 0)
#  warning HTTPD_TEST_CALLBACKS enabled

static bool ICACHE_FLASH_ATTR sHttpdTestRequestCb(struct espconn *pConn, const HTTPD_REQCB_INFO_t *pkInfo)
{
    char resp[1000];
    sprintf_PP(resp, PSTR("<pre>Hoihoi! :-)\n\nauthProvided=%s authRequired=%s\n\n"),
        skHttpdAuthLevelStrs[pkInfo->authProvided], skHttpdAuthLevelStrs[pkInfo->authRequired]);
    for (int ix = 0; ix < pkInfo->numKV; ix++)
    {
        sprintf_PP(&resp[os_strlen(resp)], PSTR("%s=%s\n"), pkInfo->keys[ix], pkInfo->vals[ix]);
    }
    sprintf_PP(&resp[os_strlen(resp)],
        PSTR("</pre><form method=\"POST\" action=\"%s?foo=123;bar=456\">"
            "<input type=\"text\" name=\"gugus\"/>"
            "<input type=\"text\" name=\"gaggi\"/>"
            "<input type=\"submit\"/></form>"), pkInfo->path);
    return httpSendHtmlPage(pConn, resp, false);
}

static bool ICACHE_FLASH_ATTR sHttpdFailRequestCb(struct espconn *pConn, const HTTPD_REQCB_INFO_t *pkInfo)
{
    UNUSED(pkInfo);
    return false;
}

#endif // (HTTPD_TEST_CALLBACKS > 0)


/* ***** initialisation and status ************************************************************** */

// initialise http server
void ICACHE_FLASH_ATTR httpdInit(void)
{
    DEBUG("httpd: init");
    httpdRegisterRequestCb(PSTR("/"),             HTTPD_AUTH_PUBLIC, sHttpdHomeRequestCb);
    httpdRegisterRequestCb(PSTR("/favicon.png"),  HTTPD_AUTH_PUBLIC, sHttpdFaviconRequestPngCb);
    httpdRegisterRequestCb(PSTR("/favicon.ico"),  HTTPD_AUTH_PUBLIC, sHttpdFaviconRequestIcoCb);
    httpdRegisterRequestCb(PSTR("/generate_204"), HTTPD_AUTH_PUBLIC, sHttpdCaptivePortalRequestCb);
    httpdRegisterRequestCb(PSTR("/about"),        HTTPD_AUTH_PUBLIC, sHttpdAboutRequestCb);
#if (HTTPD_USE_SIGNIN > 0)
    httpdRegisterRequestCb(PSTR("/signin"),       HTTPD_AUTH_PUBLIC, sHttpdSignInRequestCb);
#endif

#if (HTTPD_TEST_CALLBACKS > 0)
    httpdRegisterRequestCb(PSTR("/test/public"), HTTPD_AUTH_PUBLIC, sHttpdTestRequestCb);
    httpdRegisterRequestCb(PSTR("/test/user"),   HTTPD_AUTH_USER,   sHttpdTestRequestCb);
    httpdRegisterRequestCb(PSTR("/test/admin"),  HTTPD_AUTH_ADMIN,  sHttpdTestRequestCb);
    httpdRegisterRequestCb(PSTR("/test/fail"),   HTTPD_AUTH_PUBLIC, sHttpdFailRequestCb);
#endif // (HTTPD_TEST_CALLBACKS > 0)

    USER_CFG_t userCfg;
    cfgGet(&userCfg);
    if (userCfg.userPass[0])
    {
        httpdSetAuth(HTTPD_AUTH_USER, PSTR("user"),  userCfg.userPass);
    }
    if (userCfg.adminPass[0])
    {
        httpdSetAuth(HTTPD_AUTH_ADMIN, PSTR("admin"), userCfg.adminPass);
    }
}

// -------------------------------------------------------------------------------------------------

// print httpd status information, see sMonitorTimerCb()
void ICACHE_FLASH_ATTR httpdStatus(void)
{
    int slots = 0;
    int ix = (int)NUMOF(sHttpdConnData);
    while (ix--)
    {
        if (sHttpdConnData[ix].remote_ip)
        {
            slots++;
        }
    }

    DEBUG("mon: httpd: slots=%d/%d active=%d connect=%u recon=%u discon=%u recv=%u (%u) send=%u (%u) req=%u get=%u post=%u",
        slots, (int)NUMOF(sHttpdConnData),
        sActiveConnCnt, sHttpdConnectCnt, sHttpdReconCnt, sHttpdDisconCnt,
        sHttpdRecvCnt, sHttpdRecvSize, sHttpdSendCnt, sHttpdSendSize,
        sHttpdHandleCnt, sHttpdHandleGetCnt, sHttpdHandlePostCnt);
}


// -------------------------------------------------------------------------------------------------

// eof
