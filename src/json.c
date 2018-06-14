/*!
    \file
    \brief flipflip's Tschenggins Lämpli: JSON stuff (see \ref FF_JSON)

    - Copyright (c) 2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli
*/

#include "stdinc.h"

#include <jsmn.h>

#include "debug.h"
#include "stuff.h"
#include "json.h"

jsmntok_t *jsmnAllocTokens(const int maxTokens)
{
    const int tokensSize = maxTokens * sizeof(jsmntok_t);
    jsmntok_t *pTokens = malloc(tokensSize);
    if (pTokens == NULL)
    {
        return NULL;
    }
    memset(pTokens, 0, tokensSize);
    return pTokens;
}

int jsmnParse(char *json, const int len, jsmntok_t *pTokens, const int maxTokens)
{
    jsmn_parser parser;
    jsmn_init(&parser);
    const int numTokens = jsmn_parse(&parser, json, len, pTokens, maxTokens);
    if (numTokens < 1)
    {
        switch (numTokens)
        {
            case JSMN_ERROR_NOMEM: WARNING("json: no mem");                    break;
            case JSMN_ERROR_INVAL: WARNING("json: invalid");                   break;
            case JSMN_ERROR_PART:  WARNING("json: partial");                   break;
            default:               WARNING("json: too short (%d)", numTokens); break;
        }
        return 0;
    }
    else
    {
        return numTokens;
    }
}

void jsmnDumpTokens(char *json, jsmntok_t *pTokens, const int numTokens)
{
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
            memcpy(buf, &json[pkTok->start], sz);
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
}



// eof
