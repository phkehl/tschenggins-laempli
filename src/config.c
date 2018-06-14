/*!
    \file
    \brief flipflip's Tschenggins Lämpli: system config (noises and LED) (see \ref FF_CONFIG)

    - Copyright (c) 2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli
*/

#include "stdinc.h"

#include "stuff.h"
#include "debug.h"
#include "status.h"
#include "config.h"
#include "json.h"
#include "cfg_gen.h"

CONFIG_MODEL_t  sConfigModel;
CONFIG_DRIVER_t sConfigDriver;
CONFIG_ORDER_t  sConfigOrder;
CONFIG_BRIGHT_t sConfigBright;
CONFIG_NOISE_t  sConfigNoise;

void configInit(void)
{
    DEBUG("config: init");
    sConfigModel  = CONFIG_MODEL_UNKNOWN;
    sConfigDriver = CONFIG_DRIVER_UNKNOWN;
    sConfigOrder  = CONFIG_ORDER_UNKNOWN;
    sConfigBright = CONFIG_BRIGHT_UNKNOWN;
    sConfigNoise  = CONFIG_NOISE_SOME;
}

__INLINE CONFIG_MODEL_t  configGetModel(void)  { return sConfigModel; }
__INLINE CONFIG_DRIVER_t configGetDriver(void) { return sConfigDriver; }
__INLINE CONFIG_ORDER_t  configGetOrder(void)  { return sConfigOrder; }
__INLINE CONFIG_BRIGHT_t configGetBright(void) { return sConfigBright; }
__INLINE CONFIG_NOISE_t  configGetNoise(void)  { return sConfigNoise; }

static const char * const skConfigModelStrs[] =
{
    [CONFIG_MODEL_UNKNOWN]  = "unknown",
    [CONFIG_MODEL_STANDARD] = "standard",
    [CONFIG_MODEL_HELLO]    = "hello",
};

static const char * const skConfigDriverStrs[] =
{
    [CONFIG_DRIVER_UNKNOWN] = "unknown",
    [CONFIG_DRIVER_WS2801]  = "WS2801",
    //[CONFIG_DRIVER_WS2812]  = "WS2812",
    [CONFIG_DRIVER_SK9822]  = "SK9822",
};

static const char * const skConfigOrderStrs[] =
{
    [CONFIG_ORDER_UNKNOWN] = "unknown",
    [CONFIG_ORDER_RGB]     = "RGB",
    [CONFIG_ORDER_RBG]     = "RBG",
    [CONFIG_ORDER_GRB]     = "GRB",
    [CONFIG_ORDER_GBR]     = "GBR",
    [CONFIG_ORDER_BRG]     = "BRG",
    [CONFIG_ORDER_BGR]     = "BGR",
};

static const char * const skConfigBrightStrs[] =
{
    [CONFIG_BRIGHT_UNKNOWN] = "unknown",
    [CONFIG_BRIGHT_LOW]     = "low",
    [CONFIG_BRIGHT_MEDIUM]  = "medium",
    [CONFIG_BRIGHT_HIGH]    = "high",
    [CONFIG_BRIGHT_FULL]    = "full",
};

static const char * const skConfigNoiseStrs[] =
{
    [CONFIG_NOISE_UNKNOWN] = "unknown",
    [CONFIG_NOISE_NONE]    = "none",
    [CONFIG_NOISE_SOME]    = "some",
    [CONFIG_NOISE_MORE]    = "more",
    [CONFIG_NOISE_MOST]    = "most",
};

void configMonStatus(void)
{
    DEBUG("mon: config: model=%s driver=%s order=%s bright=%s noise=%s",
        skConfigModelStrs[sConfigModel], skConfigDriverStrs[sConfigDriver],
        skConfigOrderStrs[sConfigOrder], skConfigBrightStrs[sConfigBright],
        skConfigNoiseStrs[sConfigNoise]);
}

static CONFIG_MODEL_t sConfigStrToModel(const char *str)
{
    if      (strcmp("standard", str) == 0) { return CONFIG_MODEL_STANDARD; }
    else if (strcmp("hello",    str) == 0) { return CONFIG_MODEL_HELLO; }
    else                                   { return CONFIG_MODEL_UNKNOWN; }
}

static CONFIG_DRIVER_t sConfigStrToDriver(const char *str)
{
    if      (strcmp("WS2801", str) == 0) { return CONFIG_DRIVER_WS2801; }
    //else if (strcmp("WS2812", str) == 0) { return CONFIG_DRIVER_WS2812; }
    else if (strcmp("SK9822", str) == 0) { return CONFIG_DRIVER_SK9822; }
    else                                 { return CONFIG_DRIVER_UNKNOWN; }
}

static CONFIG_ORDER_t sConfigStrToOrder(const char *str)
{
    if      (strcmp("RGB",    str) == 0) { return CONFIG_ORDER_RGB; }
    else if (strcmp("RBG",    str) == 0) { return CONFIG_ORDER_RBG; }
    else if (strcmp("GRB",    str) == 0) { return CONFIG_ORDER_GRB; }
    else if (strcmp("GBR",    str) == 0) { return CONFIG_ORDER_GBR; }
    else if (strcmp("BRG",    str) == 0) { return CONFIG_ORDER_BRG; }
    else if (strcmp("BGR",    str) == 0) { return CONFIG_ORDER_BGR; }
    else                                 { return CONFIG_ORDER_UNKNOWN; }
}

static CONFIG_BRIGHT_t sConfigStrToBright(const char *str)
{
    if      (strcmp("low",    str) == 0) { return CONFIG_BRIGHT_LOW; }
    else if (strcmp("medium", str) == 0) { return CONFIG_BRIGHT_MEDIUM; }
    else if (strcmp("high",   str) == 0) { return CONFIG_BRIGHT_HIGH; }
    else if (strcmp("full",   str) == 0) { return CONFIG_BRIGHT_FULL; }
    else                                 { return CONFIG_BRIGHT_UNKNOWN; }
}

static CONFIG_NOISE_t sConfigStrToNoise(const char *str)
{
    if      (strcmp("none", str) == 0) { return CONFIG_NOISE_NONE; }
    else if (strcmp("some", str) == 0) { return CONFIG_NOISE_SOME; }
    else if (strcmp("more", str) == 0) { return CONFIG_NOISE_MORE; }
    else if (strcmp("most", str) == 0) { return CONFIG_NOISE_MOST; }
    else                               { return CONFIG_NOISE_UNKNOWN; }
}

bool configParseJson(char *resp, const int respLen)
{
    DEBUG("config: [%d] %s", respLen, resp);

    const int maxTokens = (4 * 2) + 10;
    jsmntok_t *pTokens = jsmnAllocTokens(maxTokens);
    if (pTokens == NULL)
    {
        ERROR("config: json malloc fail");
        return false;
    }

    // parse JSON response
    const int numTokens = jsmnParse(resp, respLen, pTokens, maxTokens);
    bool okay = numTokens > 0 ? true : false;

    //jsmnDumpTokens(resp, pTokens, numTokens);
    /*
      004.999 D: json 00: 1 obj   000..067 4 <-1 {"driver":"WS2801","model":"standard","noise":"some","order":"RGB"}
      004.999 D: json 01: 3 str   002..008 1 < 0 driver
      004.999 D: json 02: 3 str   011..017 0 < 1 WS2801
      005.010 D: json 03: 3 str   020..025 1 < 0 model
      005.010 D: json 04: 3 str   028..036 0 < 3 standard
      005.020 D: json 05: 3 str   039..044 1 < 0 noise
      005.020 D: json 06: 3 str   047..051 0 < 5 some
      005.020 D: json 07: 3 str   054..059 1 < 0 order
      005.031 D: json 08: 3 str   062..065 0 < 7 RGB
    */

    // process JSON data
    if ( okay && (pTokens[0].type != JSMN_OBJECT) )
    {
        WARNING("backend: json not obj");
        okay = false;
    }

    // look for config key value pairs
    if (okay)
    {
        CONFIG_MODEL_t  configModel  = CONFIG_MODEL_UNKNOWN;
        CONFIG_DRIVER_t configDriver = CONFIG_DRIVER_UNKNOWN;;
        CONFIG_ORDER_t  configOrder  = CONFIG_ORDER_UNKNOWN;
        CONFIG_BRIGHT_t configBright = CONFIG_BRIGHT_UNKNOWN;
        CONFIG_NOISE_t  configNoise  = CONFIG_NOISE_UNKNOWN;

        for (int ix = 0; ix < (numTokens - 1); ix++)
        {
            const jsmntok_t *pkTok = &pTokens[ix];

            // top-level key
            if (pkTok->parent == 0)
            {
                resp[ pkTok->end ] = '\0';
                const char *key = &resp[ pkTok->start ];
                const jsmntok_t *pkNext = &pTokens[ix + 1];
                if ( (pkNext->parent == ix) && (pkNext->type == JSMN_STRING) )
                {
                    resp[ pkNext->end ] = '\0';
                    const char *val = &resp[ pkNext->start ];
                    //DEBUG("config: %s=%s", key, val);
                    if      (strcmp("model",  key) == 0) { configModel  = sConfigStrToModel(val); }
                    else if (strcmp("driver", key) == 0) { configDriver = sConfigStrToDriver(val); }
                    else if (strcmp("order",  key) == 0) { configOrder  = sConfigStrToOrder(val); }
                    else if (strcmp("bright", key) == 0) { configBright = sConfigStrToBright(val); }
                    else if (strcmp("noise",  key) == 0) { configNoise  = sConfigStrToNoise(val); }
                }
            }
        }

        if ( (configModel != CONFIG_MODEL_UNKNOWN)   &&
             (configDriver != CONFIG_DRIVER_UNKNOWN) &&
             (configOrder != CONFIG_ORDER_UNKNOWN)   &&
             (configBright != CONFIG_BRIGHT_UNKNOWN)   &&
             (configNoise != CONFIG_NOISE_UNKNOWN) )
        {
            CS_ENTER;
            sConfigModel  = configModel;
            sConfigDriver = configDriver;
            sConfigOrder  = configOrder;
            sConfigBright = configBright;
            sConfigNoise  = configNoise;
            CS_LEAVE;
        }
        else
        {
            ERROR("config: incomplete");
            okay = false;
        }
    }

    // cleanup
    free(pTokens);

    return okay;
}


// eof
