/*!
    \file
    \brief flipflip's Tschenggins Lämpli: system config (noises and LED) (see \ref FF_CONFIG)

    - Copyright (c) 2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli
*/

#include "stdinc.h"

#include <jsmn.h>

#include "stuff.h"
#include "debug.h"
#include "status.h"
#include "config.h"
#include "cfg_gen.h"

CONFIG_MODEL_t  sConfigModel;
CONFIG_DRIVER_t sConfigDriver;
CONFIG_ORDER_t  sConfigOrder;
CONFIG_NOISE_t  sConfigNoise;

void configInit(void)
{
    DEBUG("configInit()");
    sConfigModel  = CONFIG_MODEL_UNKNOWN;
    sConfigDriver = CONFIG_DRIVER_UNKNOWN;
    sConfigOrder  = CONFIG_ORDER_UNKNOWN;
    sConfigNoise  = CONFIG_NOISE_SOME;
}

__INLINE CONFIG_MODEL_t  configGetModel(void)  { return sConfigModel; }
__INLINE CONFIG_DRIVER_t configGetDriver(void) { return sConfigDriver; }
__INLINE CONFIG_ORDER_t  configGetOrder(void)  { return sConfigOrder; }
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
    [CONFIG_DRIVER_WS2812]  = "WS2812",
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

static const char * const skConfigNoiseStrs[] =
{
    [CONFIG_NOISE_UNKNOWN] = "unknown",
    [CONFIG_NOISE_NONE]    = "none",
    [CONFIG_NOISE_SOME]    = "some",
    [CONFIG_NOISE_MORE]    = "more",
};

void configMonStatus(void)
{
    DEBUG("mon: config: model=%s driver=%s order=%s noise=%s",
        skConfigModelStrs[sConfigModel], skConfigDriverStrs[sConfigDriver],
        skConfigOrderStrs[sConfigOrder], skConfigNoiseStrs[sConfigNoise]);
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
    else if (strcmp("WS2812", str) == 0) { return CONFIG_DRIVER_WS2812; }
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

static CONFIG_NOISE_t sConfigStrToNoise(const char *str)
{
    if      (strcmp("none", str) == 0) { return CONFIG_NOISE_NONE; }
    else if (strcmp("some", str) == 0) { return CONFIG_NOISE_SOME; }
    else if (strcmp("more", str) == 0) { return CONFIG_NOISE_MORE; }
    else                               { return CONFIG_NOISE_UNKNOWN; }
}

#define JSON_STREQ(json, pkTok, str) (    \
        ((pkTok)->type == JSMN_STRING) && \
        (strlen(str) == ( (pkTok)->end - (pkTok)->start ) ) && \
        (strncmp(&json[(pkTok)->start], str, (pkTok)->end - (pkTok)->start) == 0) )

bool configParseJson(char *resp, const int respLen)
{
    DEBUG("config: [%d] %s", respLen, resp);

    // memory for JSON parser
    const int maxTokens = (4 * 3) + 10; // FIXME: ???
    const int tokensSize = maxTokens * sizeof(jsmntok_t);
    jsmntok_t *pTokens = malloc(tokensSize);
    if (pTokens == NULL)
    {
        ERROR("backend: json malloc fail");
        return false;
    }
    memset(pTokens, 0, tokensSize);

    // parse JSON response
    jsmn_parser parser;
    jsmn_init(&parser);
    const int numTokens = jsmn_parse(&parser, resp, respLen, pTokens, maxTokens);
    bool okay = true;
    if (numTokens < 1)
    {
        switch (numTokens)
        {
            case JSMN_ERROR_NOMEM: WARNING("json: no mem");                    break;
            case JSMN_ERROR_INVAL: WARNING("json: invalid");                   break;
            case JSMN_ERROR_PART:  WARNING("json: partial");                   break;
            default:               WARNING("json: too short (%d)", numTokens); break;
        }
        okay = false;
    }
    DEBUG("config: json %d/%d tokens, alloc %d",
        numTokens, maxTokens, tokensSize);

#if 0
    // debug json tokens
    for (int ix = 0; ix < numTokens; ix++)
    {
        static const char * const skTypeStrs[] =
        {
            [JSMN_UNDEFINED] = "undef", [JSMN_OBJECT] = "obj", [JSMN_ARRAY] = "arr",
            [JSMN_STRING] = "str", [JSMN_PRIMITIVE] = "prim"
        };
        const jsmntok_t *pkTok = &pTokens[ix];
        char buf[200];
        int sz = pkTok->end - pkTok->start;
        if ( (sz > 0) && (sz < (int)sizeof(buf)))
        {
            memcpy(buf, &resp[pkTok->start], sz);
            buf[sz] = '\0';
        }
        else
        {
            buf[0] = '\0';
        }
        char str[10];
        strncpy(str, pkTok->type < NUMOF(skTypeStrs) ? skTypeStrs[pkTok->type] : "???", sizeof(str));
        DEBUG("json %02u: %d %-5s %03d..%03d %d <%2d %s",
            ix, pkTok->type, str,
            pkTok->start, pkTok->end, pkTok->size, pkTok->parent, buf);
    }
#endif
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
                    else if (strcmp("noise",  key) == 0) { configNoise  = sConfigStrToNoise(val); }
                }
            }
        }

        if ( (configModel != CONFIG_MODEL_UNKNOWN)   &&
             (configDriver != CONFIG_DRIVER_UNKNOWN) &&
             (configOrder != CONFIG_ORDER_UNKNOWN)   &&
             (configNoise != CONFIG_NOISE_UNKNOWN) )
        {
            sConfigModel  = configModel;
            sConfigDriver = configDriver;
            sConfigOrder  = configOrder;
            sConfigNoise  = configNoise;
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
