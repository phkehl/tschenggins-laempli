
/* ***** status update stuff (polled) ************************************************************ */

#if (APP_STATUS_REALTIME == 0)

//! timeout for status update request [ms]
#define APP_WGET_TIMEOUT 5000

//! maximum length for each string (server and job name)
#define APP_WGET_REQ_STRLEN 20

static void sAppJsonResponseToState(char *resp, const int respLen);

typedef enum UPDATE_STATE_e
{
    UPDATE_CHECK, UPDATE_GET, UPDATE_WAIT, UPDATE_DONE, UPDATE_FAIL,
} UPDATE_STATE_t;

static const char skUpdateStateStrs[][8] PROGMEM =
{
    { "CHECK\0" }, { "GET\0" }, { "WAIT\0" }, { "DONE\0" }, { "FAIL\0" }
};

static os_timer_t sUpdateTimer;
static UPDATE_STATE_t sUpdateState;
#define TRIGGER_UPDATE(state, dt) do { sUpdateState = state; \
        os_timer_disarm(&sUpdateTimer); \
        os_timer_arm(&sUpdateTimer, (dt), 0); } while (0)

static uint32_t sLastUpdate;
static uint32_t sNextUpdate;

// forward declaration
static void sAppWgetResponse(const WGET_RESPONSE_t *pkResp, void *pUser);

//! maximum status response length
#define APP_WGET_RESP_LEN ( (APP_NUM_LEDS * ((2 * (APP_WGET_REQ_STRLEN + 10)) + (2 * 15) + 10)) + 250 )

#define UPDATE_CHECK_INTERVAL 250

static bool sForceUpdate;

bool ICACHE_FLASH_ATTR appForceUpdate(void)
{
    if (sUpdateState == UPDATE_CHECK)
    {
        sSetChannelsUnknown();
        PRINT("app: force update");
        sForceUpdate = true;
    }
    else
    {
        WARNING("app: ignoring manual update (state=%s)", skUpdateStateStrs[sUpdateState]);
    }
    return sForceUpdate;
}

static void ICACHE_FLASH_ATTR sUpdateTimerFunc(void *pArg)
{
    UNUSED(pArg);
    const UPDATE_STATE_t state = *((UPDATE_STATE_t *)pArg);
    //const uint32_t msss = sAppTick * LED_TIMER_INTERVAL; // [ms]
    const uint32_t msss = system_get_time() / 1000;

#if 0
    DEBUG("sUpdateTimerFunc() state=%s msss=%u last=%u (%d) next=%u (%d)",
        skUpdateStateStrs[state], msss,
        sLastUpdate,
        (int32_t)(msss - sLastUpdate) /*msss > sLastUpdate ? msss - sLastUpdate : 0*/,
        sNextUpdate,
        (int32_t)(sNextUpdate - msss) /*sNextUpdate > msss ? sNextUpdate - msss : 0*/);
#endif

    static uint32_t sUpdateStart;
    static USER_CFG_t sUserCfg;

    switch (state)
    {
        // 1. check if an update is due and possible
        case UPDATE_CHECK:
        {
            // update config
            cfgGet(&sUserCfg);

            bool doUpdate = true;
            if (!wifiIsOnline())
            {
                //DEBUG("sUpdateTimerFunc() offline");
                statusSet(USER_STATUS_OFFLINE);
                doUpdate = false;
            }
            else if (!sUserCfg.statusUrl[0])
            {
                //DEBUG("sUpdateTimerFunc() no config");
                statusSet(USER_STATUS_OFFLINE);
                doUpdate = false;
            }
            else if (sForceUpdate)
            {
                DEBUG("sUpdateTimerFunc() force");
            }

            else if ( msss < sNextUpdate )
            {
                //DEBUG("sUpdateTimerFunc() update not yet (%u)", sNextUpdate - msss);
                doUpdate = false;
            }

            // check status info expiration
            if ( ( (msss - sLastUpdate) / UPDATE_CHECK_INTERVAL) == (uint32_t)(sUserCfg.statusExpire * (1000 / UPDATE_CHECK_INTERVAL)) )
            {
                WARNING("app: status info has expired (%u > %d)",
                    ((msss - sLastUpdate) + 500) / 1000, sUserCfg.statusExpire);
            }

            // system online and configured
            if (doUpdate)
            {
                DEBUG("sUpdateTimerFunc() trigger");
                sUpdateStart = msss;
                statusSet(USER_STATUS_UPDATE);
                sForceUpdate = false;
                TRIGGER_UPDATE(UPDATE_GET, 5);
            }
            // system offline, not configured or not yet time for an update
            else
            {
                TRIGGER_UPDATE(UPDATE_CHECK, UPDATE_CHECK_INTERVAL);
            }

            break;
        }

        // 2. get channels status from webserver
        case UPDATE_GET:
        {
            static WGET_STATE_t sWgetState;
            char url[ sizeof(sUserCfg.statusUrl) + sizeof(sUserCfg.staName) + 64 + 8 + (NUMOF(sUserCfg.channels) * 15) ];
            sprintf_PP(url, PSTR("%s?cmd=channels;ascii=1;client=%s;name=%s;strlen=" STRINGIFY(APP_WGET_REQ_STRLEN)),
                sUserCfg.statusUrl, getSystemId(), sUserCfg.staName);
            for (int chIx = 0; chIx < (int)NUMOF(sUserCfg.channels); chIx++)
            {
                char tmp[16];
                sprintf_PP(tmp, PSTR(";ch=%08x"), sUserCfg.channels[chIx]);
                os_strcat(url, tmp);
            }
            DEBUG("sUpdateTimerFunc() %s (%d, %d)", url, APP_WGET_RESP_LEN, APP_WGET_TIMEOUT);
            if (!wgetRequest(&sWgetState, url, sAppWgetResponse, NULL, APP_WGET_RESP_LEN, APP_WGET_TIMEOUT))
            {
                ERROR("app: wget req init fail");
                TRIGGER_UPDATE(UPDATE_FAIL, 20);
            }
            else
            {
                TRIGGER_UPDATE(UPDATE_WAIT, 20);
            }
            break;
        }

        // 3. wait for wget (http) request to complete
        case UPDATE_WAIT:
            TRIGGER_UPDATE(UPDATE_WAIT, 20);
            break;

        // 4. process response in sAppWgetResponse()

        // 5. a) update failed, schedule another attempt
        case UPDATE_FAIL:
        {
            ERROR("app: update fail (%ums)", msss - sUpdateStart);

            statusSet(USER_STATUS_FAIL);

            sNextUpdate = sLastUpdate + (sUserCfg.statusUpdate * 1000);

            TRIGGER_UPDATE(UPDATE_CHECK, UPDATE_CHECK_INTERVAL);
            break;
        }

        // 5. b) done, schedule next update
        case UPDATE_DONE:
        {
            PRINT("app: update done (%ums)", msss - sUpdateStart);

            statusSet(USER_STATUS_HEARTBEAT);

            sNextUpdate = sUpdateStart + (sUserCfg.statusRetry * 1000);
            sLastUpdate = sUpdateStart;

            TRIGGER_UPDATE(UPDATE_CHECK, UPDATE_CHECK_INTERVAL);
            break;
        }
    }
}

static void ICACHE_FLASH_ATTR sAppWgetResponse(const WGET_RESPONSE_t *pkResp, void *pUser)
{
    DEBUG("sAppWgetResponse() status=%d error=%d contentType=%s",
        pkResp->status, pkResp->error, pkResp->contentType);
    // sAppWgetResponse() status=200 error=0 contentType=text/json; charset=US-ASCII
    DEBUG("sAppWgetResponse() bodyLen=%d body=%s",
        pkResp->bodyLen, pkResp->body);
    // sAppWgetResponse() bodyLen=408 body={"channels":[["CI_foo_master","servername","idle","success",9199],["Mon_foo","servername","idle","success",207],["Mon_bar","servername","idle","success",207],["unknown","unknown","unknown","unknown",0],["unknown","unknown","unknown","unknown",0],["unknown","unknown","unknown","unknown",0],["unknown","unknown","unknown","unknown",0],["unknown","unknown","unknown","unknown",0]],"res":1}

    if ( (pkResp->error != WGET_OK) || (pkResp->contentType == NULL) ||
        (strncmp_PP(pkResp->contentType, PSTR("text/json"), 9) != 0) )
    {
        ERROR("app: fishy status response");
        TRIGGER_UPDATE(UPDATE_FAIL, 20);
        return;
    }

    sAppJsonResponseToState(pkResp->body, pkResp->bodyLen);
}

#endif // (APP_STATUS_REALTIME == 0)
