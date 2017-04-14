// by Philippe Kehl <flipflip at oinkzwurgl dot org>

#include "user_cfg.h"
#include "user_httpd.h"
#include "user_wifi.h"
#include "user_app.h"
#include "html_gen.h"
#include "cfg_gen.h"
#include "version_gen.h"


// -------------------------------------------------------------------------------------------------

// forward declaration
static bool sCfgRequestCb(struct espconn *pConn, const HTTPD_REQCB_INFO_t *pkInfo);

// active configuration
static USER_CFG_t sUserCfg;

// forward declarations
static uint16_t sCfgChecksum(const uint8_t *pkData, int size);

typedef struct USER_CFG_STORE_s
{
    uint32_t   magic;
    USER_CFG_t cfg;
    uint16_t   checksum;
    __PAD(2);
} USER_CFG_STORE_t;


#define CFG_ADDR FF_CFGADDR
#define CFG_SECTOR (CFG_ADDR / SPI_FLASH_SEC_SIZE)
#define CFG_MAGIC ((uint32_t)0 | (CFG_SECTOR << 24) | (APP_NUM_LEDS << 16) | (HTTPD_PASS_LEN_MAX << 8) | USER_CFG_VERSION)

// initialise, load from flash or set defaults
void ICACHE_FLASH_ATTR cfgInit(const bool reset)
{
    DEBUG("cfg: init (%u)", sizeof(sUserCfg));

    // load config
    bool cfgOk = false;
    if (!reset)
    {
        USER_CFG_STORE_t store;
        if (system_param_load(CFG_SECTOR, 0, &store, sizeof(store)))
        {
            // verify checksum
            const uint16_t checksum = sCfgChecksum((const uint8_t *)&store.cfg, sizeof(store.cfg));
            if (store.checksum == checksum)
            {
                // verify magic
                if (store.magic == CFG_MAGIC)
                {
                    PRINT("cfg: load (%d@0x%06x, 0x%08x, 0x%04x)",
                        CFG_SECTOR, CFG_ADDR, store.magic, store.checksum);

                    // set as current config
                    CS_ENTER;
                    os_memcpy(&sUserCfg, &store.cfg, sizeof(sUserCfg));
                    CS_LEAVE;

                    cfgOk = true;
                }
                else
                {
                    DEBUG("cfgInit() magic fail (0x%08x != 0x%08x)", store.magic, CFG_MAGIC);
                }
            }
            else
            {
                DEBUG("cfgInit() checksum fail (0x%04x != 0x%04x)", store.checksum, checksum);
            }
        }
        else
        {
            ERROR("cfg: config load fail");
        }
    }
    else
    {
        PRINT("cfg: reset");
    }

    // set defaults
    if (!cfgOk)
    {
        PRINT("cfg: default");
        USER_CFG_t cfg;
        cfgDefault(&cfg);
        cfgSet(&cfg);
    }

    cfgDebug(&sUserCfg);

    httpdRegisterRequestCb(PSTR("/config"), HTTPD_AUTH_ADMIN, sCfgRequestCb);
}


// -------------------------------------------------------------------------------------------------
void ICACHE_FLASH_ATTR cfgDefault(USER_CFG_t *pCfg)
{
    os_memset(pCfg, 0, sizeof(*pCfg));

    // station hostname
#ifdef DEF_CFG_STANAME
    os_strncpy(pCfg->staName, DEF_CFG_STANAME, sizeof(pCfg->staName) - 1);
#else
    getSystemName(pCfg->staName, sizeof(pCfg->staName));
#endif

    // station SSID
#ifdef DEF_CFG_STASSID
    os_strncpy(pCfg->staSsid, DEF_CFG_STASSID, sizeof(pCfg->staSsid) - 1);
#endif

    // station password
#ifdef DEF_CFG_STAPASS
    os_strncpy(pCfg->staPass, DEF_CFG_STAPASS, sizeof(pCfg->staPass) - 1);
#endif

    // access point SSID
#ifdef DEF_CFG_APSSID
    os_strncpy(pCfg->apSsid, DEF_CFG_APSSID, sizeof(pCfg->apPass) - 1);
#else
    getSystemName(pCfg->apSsid, sizeof(pCfg->apSsid));
#endif

    // access point password
#ifdef DEF_CFG_APPASS
    os_strncpy(pCfg->apPass, DEF_CFG_APPASS, sizeof(pCfg->apPass) - 1);
#endif

    // user password
#ifdef DEF_CFG_USERPASS
    os_strncpy(pCfg->userPass, DEF_CFG_USERPASS, sizeof(pCfg->userPass) - 1);
#endif

    // admin password
#ifdef DEF_CFG_ADMINPASS
    os_strncpy(pCfg->adminPass, DEF_CFG_ADMINPASS, sizeof(pCfg->adminPass) - 1);
#endif

    // Jenkins status backend URL
#ifdef DEF_CFG_STATUSURL
    os_strncpy(pCfg->statusUrl, DEF_CFG_STATUSURL, sizeof(pCfg->statusUrl) - 1);
#endif

    // have Chewbacca?
#if (defined DEF_CFG_HAVECHEWIE)
#  if (DEF_CFG_HAVECHEWIE > 0)
    pCfg->haveChewie = true;
#  else
    pCfg->haveChewie = false;
#  endif
#else
    pCfg->haveChewie = true;
#endif

    // LEDs
#if (defined DEF_CFG_LED01)
    pCfg->leds[0] = DEF_CFG_LED01;
#endif
#if (defined DEF_CFG_LED01)
    pCfg->leds[1] = DEF_CFG_LED02;
#endif
#if (defined DEF_CFG_LED01)
    pCfg->leds[2] = DEF_CFG_LED03;
#endif
#if (defined DEF_CFG_LED01)
    pCfg->leds[3] = DEF_CFG_LED04;
#endif
#if (defined DEF_CFG_LED01)
    pCfg->leds[4] = DEF_CFG_LED05;
#endif
#if (defined DEF_CFG_LED01)
    pCfg->leds[5] = DEF_CFG_LED06;
#endif
#if (defined DEF_CFG_LED01)
    pCfg->leds[6] = DEF_CFG_LED07;
#endif
#if (defined DEF_CFG_LED01)
    pCfg->leds[7] = DEF_CFG_LED08;
#endif
}

// -------------------------------------------------------------------------------------------------

bool ICACHE_FLASH_ATTR cfgSet(const USER_CFG_t *pkCfg)
{
    USER_CFG_STORE_t store;
    os_memset(&store, 0, sizeof(store));
    store.magic = CFG_MAGIC;
    os_memcpy(&store.cfg, pkCfg, sizeof(store.cfg));
    store.checksum = sCfgChecksum((const uint8_t *)&store.cfg, sizeof(store.cfg));

    const bool res = system_param_save_with_protect(CFG_SECTOR, &store, sizeof(store));
    if (res)
    {
        CS_ENTER;
        os_memcpy(&sUserCfg, &store.cfg, sizeof(sUserCfg));
        CS_LEAVE;
        PRINT("cfg: store (%d@0x%06x, 0x%08x, 0x%04x)",
            CFG_SECTOR, CFG_ADDR, store.magic, store.checksum);
    }
    else
    {
        ERROR("cfg: store fail");
    }
    return res;
}

void ICACHE_FLASH_ATTR cfgGet(USER_CFG_t *pCfg)
{
    CS_ENTER;
    os_memcpy(pCfg, &sUserCfg, sizeof(sUserCfg));
    CS_LEAVE;
}

__INLINE const USER_CFG_t * ICACHE_FLASH_ATTR cfgGetPtr(void)
{
    return &sUserCfg;
}


// -------------------------------------------------------------------------------------------------

void ICACHE_FLASH_ATTR cfgDebug(const USER_CFG_t *pkCfg)
{
    DEBUG("cfgDebug() staName=%s staSsid=%s staPass=%d apSsid=%s apPass=%d userPass=%d adminPass=%d",
        pkCfg->staName,
        pkCfg->staSsid, os_strlen(pkCfg->staPass),
        pkCfg->apSsid, os_strlen(pkCfg->apPass),
        os_strlen(pkCfg->userPass), os_strlen(pkCfg->adminPass));
    DEBUG("cfgDebug() statusUrl=%s",
        pkCfg->statusUrl);
#if (APP_NUM_LEDS == 8)
    DEBUG("cfgDebug() ch00=%08x ch01=%08x ch02=%08x ch03=%08x",
        pkCfg->leds[0], pkCfg->leds[1], pkCfg->leds[2], pkCfg->leds[3]);
    DEBUG("cfgDebug() ch04=%08x ch05=%08x ch06=%08x ch07=%08x",
        pkCfg->leds[4], pkCfg->leds[5], pkCfg->leds[6], pkCfg->leds[7]);
#elif (APP_NUM_LEDS == 3)
    DEBUG("cfgDebug() ch00=%08x ch01=%08x ch02=%08x",
        pkCfg->leds[0], pkCfg->leds[1], pkCfg->leds[2]);
#else
#  warning please implement me
#endif
}

static uint16_t ICACHE_FLASH_ATTR sCfgChecksum(const uint8_t *pkData, int size)
{
    //DEBUG("sCfgChecksum() %p %d", pkData, size);
    uint8_t ckA = 0;
    uint8_t ckB = 0;

    while (size--)
    {
        ckA += *pkData++;
        ckB += ckA;
    }

    return (ckA << 8) | ckB;
}

// -------------------------------------------------------------------------------------------------


static const char skCfgFormHtml[] PROGMEM = USER_CFG_FORM_HTML_STR;

#define CFG_FORM_SIZE (sizeof(USER_CFG_FORM_HTML_STR) + 512)

static os_timer_t sWifiChangeTimer;
static void ICACHE_FLASH_ATTR sWifiChangeTimerFunc(void *pArg)
{
#if 0 // lots of mem leaks... :-(
    const uint32_t arg = (uint32_t)pArg;
    const bool sta = arg & 0x01 ? true : false;
    const bool ap  = arg & 0x02 ? true : false;
    wifiStart(sta, ap);
#else
    UNUSED(pArg);
    system_restart();
#endif
}

// wifi config web interface (/cfg)
static bool ICACHE_FLASH_ATTR sCfgRequestCb(struct espconn *pConn, const HTTPD_REQCB_INFO_t *pkInfo)
{
    const struct _esp_tcp *pkTcp = pConn->proto.tcp;

    USER_CFG_t userCfg;
    cfgGet(&userCfg);
    cfgDebug(&userCfg);

    const char *dummyPass = PSTR("3l173h4xx0rp455w0rd");
    //const char *dummyPass = PSTR("canihazcheesburger");

    // handle config change
    if (pkInfo->numKV && (strcmp_PP(pkInfo->keys[0], PSTR("debug")) != 0))
    {
        USER_CFG_t defaultCfg;
        const char *errorMsg = NULL;
        cfgDefault(&defaultCfg);
        cfgDebug(&defaultCfg);
        bool staChanged      = false;
        bool apChanged       = false;
        bool otherChanged    = false;
        bool ledsChanged     = false;
        bool redirHostname   = os_strcmp(pkInfo->host, userCfg.staName) == 0 ? true : false;
        bool haveChewie      = false;
        for (int ix = 0; ix < pkInfo->numKV; ix++)
        {
            const char *key = pkInfo->keys[ix];
            const char *val = pkInfo->vals[ix];
            const int valLen = os_strlen(val);
            //DEBUG("sCfgRequestCb() ix=%d key=%s val=%s (%d)", ix, key, val, valLen);

            if (strcmp_PP(key, PSTR("stassid")) == 0)
            {
                if (valLen == 0)
                {
                    DEBUG("sCfgRequestCb() stassid default");
                    os_strcpy(userCfg.staSsid, defaultCfg.staSsid);
                }
                else if ( (valLen > (int)(sizeof(userCfg.staSsid) - 1)) /*|| (valLen < 3)*/ )
                {
                    DEBUG("sCfgRequestCb() stassid length");
                    errorMsg = PSTR("station SSID too long");
                    break;
                }
                else if (os_strcmp(val, userCfg.staSsid) != 0)
                {
                    DEBUG("sCfgRequestCb() stassid change (%s)", val);
                    os_strncpy(userCfg.staSsid, val, sizeof(userCfg.staSsid));
                    userCfg.staSsid[sizeof(userCfg.staSsid)-1] = '\0';
                    staChanged = true;
                }
            }
            else if (strcmp_PP(key, PSTR("stapass")) == 0)
            {
                if (valLen == 0)
                {
                    DEBUG("sCfgRequestCb() stapass default");
                    os_strcpy(userCfg.staPass, defaultCfg.staPass);
                }
                else if (strcmp_PP(val, dummyPass) == 0)
                {
                    // no change
                }
                else if ( (valLen > (int)(sizeof(userCfg.staPass) - 1)) /*|| (valLen < 3)*/ )
                {
                    DEBUG("sCfgRequestCb() stapass length");
                    errorMsg = PSTR("station password too long");
                    break;
                }
                else if (os_strcmp(val, userCfg.staPass) != 0)
                {
                    DEBUG("sCfgRequestCb() stapass change (%d)", valLen);
                    os_strncpy(userCfg.staPass, val, sizeof(userCfg.staPass));
                    userCfg.staPass[sizeof(userCfg.staPass)-1] = '\0';
                    staChanged = true;
                }
            }
            else if (strcmp_PP(key, PSTR("staname")) == 0)
            {
                if (valLen == 0)
                {
                    DEBUG("sCfgRequestCb() staname default");
                    os_strcpy(userCfg.staName, defaultCfg.staName);
                }
                else if ( (valLen > (int)(sizeof(userCfg.staName) - 1)) || (valLen < 5) )
                {
                    DEBUG("sCfgRequestCb() staname length");
                    errorMsg = PSTR("station name too long or too short");
                    break;
                }
                else if (os_strcmp(val, userCfg.staName) != 0)
                {
                    DEBUG("sCfgRequestCb() staname change (%s)", val);
                    os_strncpy(userCfg.staName, val, sizeof(userCfg.staName));
                    userCfg.staName[sizeof(userCfg.staName)-1] = '\0';
                    staChanged = true;
                }
            }
            else if (strcmp_PP(key, PSTR("apssid")) == 0)
            {
                if (valLen == 0)
                {
                    DEBUG("sCfgRequestCb() apssid default");
                    os_strcpy(userCfg.apSsid, defaultCfg.apSsid);
                }
                else if ( (valLen > (int)(sizeof(userCfg.apSsid) - 1)) || (valLen < 5) )
                {
                    DEBUG("sCfgRequestCb() apssid length");
                    errorMsg = PSTR("ap SSID too long or too short");
                    break;
                }
                else if (os_strcmp(val, userCfg.apSsid) != 0)
                {
                    DEBUG("sCfgRequestCb() apssid change (%s)", val);
                    os_strncpy(userCfg.apSsid, val, sizeof(userCfg.apSsid));
                    userCfg.apSsid[sizeof(userCfg.apSsid)-1] = '\0';
                    apChanged = true;
                }
            }
            else if (strcmp_PP(key, PSTR("appass")) == 0)
            {
                if (valLen == 0)
                {
                    DEBUG("sCfgRequestCb() appass default");
                    os_strcpy(userCfg.apPass, defaultCfg.apPass);
                }
                else if (strcmp_PP(val, dummyPass) == 0)
                {
                    // no change
                }
                else if ( (valLen > (int)(sizeof(userCfg.apPass) - 1)) || (valLen < 5) )
                {
                    DEBUG("sCfgRequestCb() appass length");
                    errorMsg = PSTR("ap password too long or too short");
                    break;
                }
                else if (os_strcmp(val, userCfg.apPass) != 0)
                {
                    DEBUG("sCfgRequestCb() appass change (%d)", valLen);
                    os_strncpy(userCfg.apPass, val, sizeof(userCfg.apPass));
                    userCfg.apPass[sizeof(userCfg.apPass)-1] = '\0';
                    apChanged = true;
                }
            }
            else if (strcmp_PP(key, PSTR("userpw")) == 0)
            {
                if (valLen == 0)
                {
                    DEBUG("sCfgRequestCb() userpw default");
                    os_strcpy(userCfg.userPass, defaultCfg.userPass);
                }
                else if (strcmp_PP(val, dummyPass) == 0)
                {
                    // no change
                }
                else if ( (valLen > (int)(sizeof(userCfg.userPass) - 1)) || (valLen < 5) )
                {
                    DEBUG("sCfgRequestCb() userpw length (%d)", valLen);
                    errorMsg = PSTR("user password too long or too short");
                    break;
                }
                else if (os_strcmp(val, userCfg.userPass) != 0)
                {
                    DEBUG("sCfgRequestCb() userpw change (%d)", valLen);
                    os_strncpy(userCfg.userPass, val, sizeof(userCfg.userPass));
                    userCfg.userPass[sizeof(userCfg.userPass)-1] = '\0';
                    otherChanged = true;
                    if (!httpdSetAuth(HTTPD_AUTH_USER, userCfg.userPass[0] ? "user" : "", userCfg.userPass))
                    {
                        errorMsg = PSTR("illegal user password");
                        break;
                    }
                }
            }
            else if (strcmp_PP(key, PSTR("adminpw")) == 0)
            {
                if (valLen == 0)
                {
                    DEBUG("sCfgRequestCb() adminpw default");
                    os_strcpy(userCfg.adminPass, defaultCfg.adminPass);
                }
                else if (strcmp_PP(val, dummyPass) == 0)
                {
                    // no change
                }
                else if ( (valLen > (int)(sizeof(userCfg.adminPass) - 1)) || (valLen < 5) )
                {
                    DEBUG("sCfgRequestCb() adminpw length (%d)", valLen);
                    errorMsg = PSTR("admin password too long or too short");
                    break;
                }
                else if (os_strcmp(val, userCfg.adminPass) != 0)
                {
                    DEBUG("sCfgRequestCb() adminpw change (%d)", valLen);
                    os_strncpy(userCfg.adminPass, val, sizeof(userCfg.adminPass));
                    userCfg.adminPass[sizeof(userCfg.adminPass)-1] = '\0';
                    otherChanged = true;
                    if (!httpdSetAuth(HTTPD_AUTH_ADMIN, userCfg.adminPass[0] ? "admin" : "", userCfg.adminPass))
                    {
                        errorMsg = PSTR("illegal admin password");
                        break;
                    }
                }
            }
            else if (strcmp_PP(key, PSTR("statusurl")) == 0)
            {
                char buf[256];
                const char *host, *path, *query, *auth;
                bool https;
                uint16_t port;
                if ( (valLen > (int)(sizeof(userCfg.statusUrl)-1)) ||
                    ( (valLen > 0) &&
                        (wgetReqParamsFromUrl(val, buf, sizeof(buf), &host, &path, &query, &auth, &https, &port) == 0) ) )
                {
                    DEBUG("sCfgRequestCb() statusurl error: %s / %s ? %s / %s / %d / %u",
                        host, path, query, auth, https, port);
                    errorMsg = PSTR("illegal status url");
                    break;
                }
                else if (os_strcmp(val, userCfg.statusUrl) != 0)
                {
                    DEBUG("sCfgRequestCb() statusurl change (%s)", val);
                    os_strncpy(userCfg.statusUrl, val, sizeof(userCfg.statusUrl));
                    otherChanged = true;
                }
            }
            else if (strcmp_PP(key, PSTR("havechewie")) == 0)
            {
                haveChewie = val[0] && (val[0] == '1') ? true : false;
            }
            else if ( (key[0] == 'l') && (key[1] == 'e') && (key[2] == 'd') &&
                isdigit((int)key[3]) && isdigit((int)key[4]) && (valLen == 8) )
            {
                const int ledIx = atoi(&key[3]);
                uint32_t jobId;
                if ( (ledIx >= 0) && (ledIx < (int)NUMOF(userCfg.leds)) && parseHex(val, &jobId) )
                {
                    if (userCfg.leds[ledIx] != jobId)
                    {
                        DEBUG("sCfgRequestCb() %s change (%08x)", key, jobId);
                        userCfg.leds[ledIx] = jobId;
                        ledsChanged = true;
                    }
                }
            }
        }

        if (haveChewie != userCfg.haveChewie)
        {
            DEBUG("sCfgRequestCb() havechewie change (%d)", haveChewie);
            userCfg.haveChewie = haveChewie;
            otherChanged = true;
        }

        if (errorMsg != NULL)
        {
            return httpdSendError(pConn, pkInfo->path, 400, NULL, errorMsg);
        }

        if (apChanged || staChanged || otherChanged || ledsChanged)
        {
            cfgSet(&userCfg);
            cfgDebug(&userCfg);
        }

        if (ledsChanged)
        {
            appForceUpdate();
        }

        if (apChanged || staChanged)
        {
            const uint32_t arg = (apChanged ? 0x2 : 0x0) | (staChanged ? 0x1 : 0x0);
            os_timer_disarm(&sWifiChangeTimer);
            os_timer_setfn(&sWifiChangeTimer, (os_timer_func_t *)sWifiChangeTimerFunc, arg);
            os_timer_arm(&sWifiChangeTimer, 1000, 0); // 1000ms, once
        }

        char location[256];
        sprintf_PP(location, PSTR("Location: http://%s%s\r\n"),
            redirHostname ? userCfg.staName : pkInfo->host, pkInfo->path);
        return httpdSendError(pConn, pkInfo->path, 302, location, NULL);
    }


    // render config form
    char *pResp = memAlloc(CFG_FORM_SIZE);
    if (pResp == NULL)
    {
        ERROR("sCfgRequestCb(%p) malloc %u fail", pConn, CFG_FORM_SIZE);
        return false;
    }

    strcpy_P(pResp, skCfgFormHtml);


    // get current config
    const char *emptyStr = PSTR("");
    const char *staSsid    = userCfg.staSsid;
    const char *staPass    = userCfg.staPass[0]    ? dummyPass : emptyStr;
    const char *staName    = userCfg.staName;
    const char *userPw     = userCfg.userPass[0]   ? dummyPass : emptyStr;
    const char *adminPw    = userCfg.adminPass[0]  ? dummyPass : emptyStr;
    const char *apSsid     = userCfg.apSsid;
    const char *apPass     = userCfg.apPass[0]     ? dummyPass : emptyStr;
    const char *statusUrl  = userCfg.statusUrl;
    const char *haveChewie = userCfg.haveChewie    ? PSTR("checked") : emptyStr;

    char staIp[16];
    struct ip_info ipinfo;
    wifi_get_ip_info(STATION_IF, &ipinfo);
    os_sprintf(staIp, IPSTR, IP2STR(&ipinfo.ip));

    const char *sysId = getSystemId();
    const char *onStaNet = wifiIsApNet(pkTcp->remote_ip) ? PSTR("0") : PSTR("1");
    const char *wifiOnline = wifiIsOnline() ? PSTR("1") : PSTR("0");

    char ledIds[APP_NUM_LEDS * 9 + 1];
    ledIds[0] = 0;
    for (int ledIx = 0; ledIx < APP_NUM_LEDS; ledIx++)
    {
        os_sprintf(&ledIds[ledIx * 9], "%08x ", userCfg.leds[ledIx]);
    }
    ledIds[APP_NUM_LEDS * 9 - 1] = '\0';

    // render html
    //static const char formKeys[][12] PROGMEM = // FIXME: why not?!
    //{
    //    { "STASSID\0" }, { "STAPASS\0" }, { "STANAME\0" }, { "USERPW\0" }, { "ADMINPW\0" },
    //    { "APSSID\0" }, { "APPASS\0" }, { "STATUSURL\0" }
    //};
    const char *formKeys[]  =
    {
        PSTR("STASSID"), PSTR("STAPASS"), PSTR("STANAME"), PSTR("STAIP"),
        PSTR("APSSID"), PSTR("APPASS"),
        PSTR("USERPW"), PSTR("ADMINPW"),
        PSTR("STATUSURL"),
        PSTR("HAVECHEWIE"),
        PSTR("SYSID"), PSTR("ONSTANET"), PSTR("WIFIONLINE"), PSTR("LEDIDS")
    };
    const char *formVals[] =
    {
        staSsid, staPass, staName, staIp,
        apSsid, apPass,
        userPw, adminPw,
        statusUrl,
        haveChewie,
        sysId, onStaNet, wifiOnline, ledIds
    };

    const int htmlLen = htmlRender(
        pResp, pResp, CFG_FORM_SIZE, formKeys, formVals, (int)NUMOF(formKeys), true);
    DEBUG("sCfgRequestCb() htmlLen=%d/%d", htmlLen, CFG_FORM_SIZE);

    const bool res = httpSendHtmlPage(pConn, pResp, false);

    memFree(pResp);

    return res;
}


// -------------------------------------------------------------------------------------------------

// eof
