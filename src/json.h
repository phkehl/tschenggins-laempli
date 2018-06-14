/*!
    \file
    \brief flipflip's Tschenggins Lämpli: JSON stuff (see \ref FF_JSON)

    - Copyright (c) 2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

    \defgroup FF_JSON JSON
    \ingroup FF

    @{
*/
#ifndef __JSON_H__
#define __JSON_H__

#include <jsmn.h>

#include "stdinc.h"

#define JSMN_STREQ(json, pkTok, str) (    \
        ((pkTok)->type == JSMN_STRING) && \
        (strlen(str) == ( (pkTok)->end - (pkTok)->start ) ) && \
        (strncmp(&json[(pkTok)->start], str, (pkTok)->end - (pkTok)->start) == 0) )

#define JSMN_ANYEQ(json, pkTok, str) (    \
        ( ((pkTok)->type == JSMN_STRING) || ((pkTok)->type == JSMN_PRIMITIVE) ) && \
        (strlen(str) == ( (pkTok)->end - (pkTok)->start ) ) && \
        (strncmp(&json[(pkTok)->start], str, (pkTok)->end - (pkTok)->start) == 0) )


//! memory for JSON parser
jsmntok_t *jsmnAllocTokens(const int maxTokens);

//! parse JSON into tokens
int jsmnParse(char *json, const int len, jsmntok_t *pTokens, const int maxTokens);

//! dump tokens
void jsmnDumpTokens(char *json, jsmntok_t *pTokens, const int numTokens);


#endif // __JSON_H__
