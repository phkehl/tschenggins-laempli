// weird JSON API in ESP8266 SDK experiment

void ICACHE_FLASH_ATTR jsonTestWeirdSdkJson(void)
{
    const char *json = "{\"channels\":[[3,2],[3,2],[3,2],[3,2],[3,2],[3,1],[3,4],[0,0]],\"lut0\":{\"1\":\"unknown\",\"2\":\"running\",\"3\":\"idle\"},\"lut1\":{\"1\":\"unknown\",\"2\":\"success\",\"3\":\"unstable\",\"4\":\"failure\"},\"res\":1}";
    DEBUG("json=%s", json);

    struct jsonparse_state state;
    jsonparse_setup(&state, json, os_strlen(json));

    bool done = false;
    while (!done)
    {
        int res = jsonparse_next(&state);
        int len = jsonparse_get_len(&state);
        int type = jsonparse_get_type(&state);
        char str[100];
        int strres = -1;
        str[0] = '\0';
        if ( (res == JSON_TYPE_PAIR_NAME) || (res == JSON_TYPE_STRING) )
        {
            strres = jsonparse_copy_value(&state, str, sizeof(str));
        }
        int intval = -1;
        if (res == JSON_TYPE_NUMBER)
        {
            intval = jsonparse_get_value_as_int(&state);
        }
        bool ischannels = jsonparse_strcmp_value(&state, "channels") == 0 ? true : false;
        DEBUG("res=%3d (%c) len=%d type=%3d (%c) -- str=%10s (%3d, %c) -- val=%2d -- ischannels=%d",
            res, isprint(res) ? res : '?', len, type, isprint(type) ? type : '?',
            str, strres, isprint(strres) ? strres : '?', intval, ischannels);
        if (!res)
        {
            done = true;
        }
    }
}

/*
000.041 D: json={"channels":[[3,2],[3,2],[3,2],[3,2],[3,2],[3,1],[3,4],[0,0]],"lut0":{"1":"unknown","2":"running","3":"idle"},"lut1":{"1":"unknown","2":"success","3":"unstable","4":"failure"},"res":1}
000.051 D: res=123 ({) len=1073656576 type=123 ({) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.051 D: res= 78 (N) len=8 type=123 ({) -- str=  channels ( 78, N) -- val=-1 -- ischannels=1
000.062 D: res= 58 (:) len=8 type= 58 (:) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.072 D: res= 91 ([) len=8 type= 91 ([) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.082 D: res= 91 ([) len=8 type= 91 ([) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.082 D: res= 48 (0) len=1 type= 91 ([) -- str=           ( -1, ?) -- val= 3 -- ischannels=0
000.093 D: res= 44 (,) len=1 type= 91 ([) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.103 D: res= 48 (0) len=1 type= 91 ([) -- str=           ( -1, ?) -- val= 2 -- ischannels=0
000.113 D: res= 93 (]) len=1 type= 91 ([) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.113 D: res= 44 (,) len=1 type= 91 ([) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.123 D: res= 91 ([) len=1 type= 91 ([) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.134 D: res= 48 (0) len=1 type= 91 ([) -- str=           ( -1, ?) -- val= 3 -- ischannels=0
000.144 D: res= 44 (,) len=1 type= 91 ([) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.144 D: res= 48 (0) len=1 type= 91 ([) -- str=           ( -1, ?) -- val= 2 -- ischannels=0
000.154 D: res= 93 (]) len=1 type= 91 ([) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.165 D: res= 44 (,) len=1 type= 91 ([) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.175 D: res= 91 ([) len=1 type= 91 ([) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.175 D: res= 48 (0) len=1 type= 91 ([) -- str=           ( -1, ?) -- val= 3 -- ischannels=0
000.185 D: res= 44 (,) len=1 type= 91 ([) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.195 D: res= 48 (0) len=1 type= 91 ([) -- str=           ( -1, ?) -- val= 2 -- ischannels=0
000.206 D: res= 93 (]) len=1 type= 91 ([) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.206 D: res= 44 (,) len=1 type= 91 ([) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.216 D: res= 91 ([) len=1 type= 91 ([) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.226 D: res= 48 (0) len=1 type= 91 ([) -- str=           ( -1, ?) -- val= 3 -- ischannels=0
000.236 D: res= 44 (,) len=1 type= 91 ([) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.236 D: res= 48 (0) len=1 type= 91 ([) -- str=           ( -1, ?) -- val= 2 -- ischannels=0
000.247 D: res= 93 (]) len=1 type= 91 ([) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.257 D: res= 44 (,) len=1 type= 91 ([) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.267 D: res= 91 ([) len=1 type= 91 ([) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.267 D: res= 48 (0) len=1 type= 91 ([) -- str=           ( -1, ?) -- val= 3 -- ischannels=0
000.278 D: res= 44 (,) len=1 type= 91 ([) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.288 D: res= 48 (0) len=1 type= 91 ([) -- str=           ( -1, ?) -- val= 2 -- ischannels=0
000.298 D: res= 93 (]) len=1 type= 91 ([) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.298 D: res= 44 (,) len=1 type= 91 ([) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.308 D: res= 91 ([) len=1 type= 91 ([) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.319 D: res= 48 (0) len=1 type= 91 ([) -- str=           ( -1, ?) -- val= 3 -- ischannels=0
000.329 D: res= 44 (,) len=1 type= 91 ([) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.329 D: res= 48 (0) len=1 type= 91 ([) -- str=           ( -1, ?) -- val= 1 -- ischannels=0
000.339 D: res= 93 (]) len=1 type= 91 ([) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.349 D: res= 44 (,) len=1 type= 91 ([) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.360 D: res= 91 ([) len=1 type= 91 ([) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.360 D: res= 48 (0) len=1 type= 91 ([) -- str=           ( -1, ?) -- val= 3 -- ischannels=0
000.370 D: res= 44 (,) len=1 type= 91 ([) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.380 D: res= 48 (0) len=1 type= 91 ([) -- str=           ( -1, ?) -- val= 4 -- ischannels=0
000.380 D: res= 93 (]) len=1 type= 91 ([) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.391 D: res= 44 (,) len=1 type= 91 ([) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.401 D: res= 91 ([) len=1 type= 91 ([) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.411 D: res= 48 (0) len=1 type= 91 ([) -- str=           ( -1, ?) -- val= 0 -- ischannels=0
000.411 D: res= 44 (,) len=1 type= 91 ([) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.421 D: res= 48 (0) len=1 type= 91 ([) -- str=           ( -1, ?) -- val= 0 -- ischannels=0
000.432 D: res= 93 (]) len=1 type= 91 ([) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.442 D: res= 93 (]) len=1 type= 58 (:) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.442 D: res= 44 (,) len=1 type=123 ({) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.452 D: res= 78 (N) len=4 type=123 ({) -- str=      lut0 ( 78, N) -- val=-1 -- ischannels=0
000.462 D: res= 58 (:) len=4 type= 58 (:) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.473 D: res=123 ({) len=4 type=123 ({) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.473 D: res= 78 (N) len=1 type=123 ({) -- str=         1 ( 78, N) -- val=-1 -- ischannels=0
000.483 D: res= 58 (:) len=1 type= 58 (:) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.493 D: res= 34 (") len=7 type= 58 (:) -- str=   unknown ( 34, ") -- val=-1 -- ischannels=0
000.503 D: res= 44 (,) len=7 type=123 ({) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.504 D: res= 78 (N) len=1 type=123 ({) -- str=         2 ( 78, N) -- val=-1 -- ischannels=0
000.514 D: res= 58 (:) len=1 type= 58 (:) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.524 D: res= 34 (") len=7 type= 58 (:) -- str=   running ( 34, ") -- val=-1 -- ischannels=0
000.534 D: res= 44 (,) len=7 type=123 ({) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.534 D: res= 78 (N) len=1 type=123 ({) -- str=         3 ( 78, N) -- val=-1 -- ischannels=0
000.545 D: res= 58 (:) len=1 type= 58 (:) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.555 D: res= 34 (") len=4 type= 58 (:) -- str=      idle ( 34, ") -- val=-1 -- ischannels=0
000.565 D: res=125 (}) len=4 type= 58 (:) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.565 D: res= 44 (,) len=4 type=123 ({) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.575 D: res= 78 (N) len=4 type=123 ({) -- str=      lut1 ( 78, N) -- val=-1 -- ischannels=0
000.586 D: res= 58 (:) len=4 type= 58 (:) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.586 D: res=123 ({) len=4 type=123 ({) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.596 D: res= 78 (N) len=1 type=123 ({) -- str=         1 ( 78, N) -- val=-1 -- ischannels=0
000.606 D: res= 58 (:) len=1 type= 58 (:) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.617 D: res= 34 (") len=7 type= 58 (:) -- str=   unknown ( 34, ") -- val=-1 -- ischannels=0
000.617 D: res= 44 (,) len=7 type=123 ({) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.627 D: res= 78 (N) len=1 type=123 ({) -- str=         2 ( 78, N) -- val=-1 -- ischannels=0
000.637 D: res= 58 (:) len=1 type= 58 (:) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.648 D: res= 34 (") len=7 type= 58 (:) -- str=   success ( 34, ") -- val=-1 -- ischannels=0
000.648 D: res= 44 (,) len=7 type=123 ({) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.658 D: res= 78 (N) len=1 type=123 ({) -- str=         3 ( 78, N) -- val=-1 -- ischannels=0
000.668 D: res= 58 (:) len=1 type= 58 (:) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.678 D: res= 34 (") len=8 type= 58 (:) -- str=  unstable ( 34, ") -- val=-1 -- ischannels=0
000.678 D: res= 44 (,) len=8 type=123 ({) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.689 D: res= 78 (N) len=1 type=123 ({) -- str=         4 ( 78, N) -- val=-1 -- ischannels=0
000.699 D: res= 58 (:) len=1 type= 58 (:) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.709 D: res= 34 (") len=7 type= 58 (:) -- str=   failure ( 34, ") -- val=-1 -- ischannels=0
000.709 D: res=125 (}) len=7 type= 58 (:) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.719 D: res= 44 (,) len=7 type=123 ({) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.730 D: res= 78 (N) len=3 type=123 ({) -- str=       res ( 78, N) -- val=-1 -- ischannels=0
000.740 D: res= 58 (:) len=3 type= 58 (:) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.740 D: res= 48 (0) len=1 type= 58 (:) -- str=           ( -1, ?) -- val= 1 -- ischannels=0
000.750 D: res=125 (}) len=1 type=  0 (?) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
000.760 D: res=  0 (?) len=1 type=  0 (?) -- str=           ( -1, ?) -- val=-1 -- ischannels=0
*/

// using jsmn (https://github.com/zserge/jsmn)

void ICACHE_FLASH_ATTR loopJsonTest(void)
{
    //const char *json = "{\"channels\":[[3,2],[3,2],[3,2],[3,2],[3,2],[3,1],[3,4],[0,0]],\"lut0\":{\"1\":\"unknown\",\"2\":\"running\",\"3\":\"idle\"},\"lut1\":{\"1\":\"unknown\",\"2\":\"success\",\"3\":\"unstable\",\"4\":\"failure\"},\"res\":1}";
    const char *json = "{\"channels\":[[\"idle\",\"success\"],[\"idle\",\"success\"],[\"idle\",\"success\"],[\"idle\",\"success\"],[\"idle\",\"success\"],[\"idle\",\"success\"],[\"idle\",\"failure\"],[\"unknown\",\"unknown\"]],\"res\":1}";
    DEBUG("json=%s", json);

    jsmn_parser parser;
    jsmn_init(&parser);

    jsmntok_t tokens[50];
    os_memset(tokens, 0, sizeof(tokens));
    int res;

    res = jsmn_parse(&parser, json, os_strlen(json), tokens, NUMOF(tokens));
    switch (res)
    {
        // < 0
        case JSMN_ERROR_NOMEM: ERROR("JSMN_ERROR_NOMEM"); break;
        case JSMN_ERROR_INVAL: ERROR("JSMN_ERROR_INVAL"); break;
        case JSMN_ERROR_PART:  ERROR("JSMN_ERROR_PART");  break;
        // >= 0
        default:               DEBUG("res=%d", res); break;
    }

    static const char *skTypeStrs[] = { "undef", "obj", "arr", "str", "prim" };
    DEBUG("parser: pos=%u toknext=%u toksuper=%d", parser.pos, parser.toknext, parser.toksuper);


    for (unsigned int ix = 0; ix < NUMOF(tokens); ix++)
    {
        const jsmntok_t *tok = &tokens[ix];
        char buf[200];
        int sz = tok->end - tok->start;
        if ( (sz > 0) && (sz < (int)sizeof(buf)))
        {
            os_memcpy(buf, &json[tok->start], sz);
            buf[sz] = '\0';
        }
        else
        {
            buf[0] = '\0';
        }
        DEBUG("%02u: %-5s %03d..%03d %d <%2d %s",
            ix, tok->type < NUMOF(skTypeStrs) ? skTypeStrs[tok->type] : "???",
            tok->start, tok->end, tok->size, tok->parent, buf);
    }
}

/*
000.051 D: json={"channels":[[3,2],[3,2],[3,2],[3,2],[3,2],[3,1],[3,4],[0,0]],"lut0":{"1":"unknown","2":"running","3":"idle"},"lut1":{"1":"unknown","2":"success","3":"unstable","4":"failure"},"res":1}
000.051 D: res=47
000.051 D: parser: pos=184 toknext=47 toksuper=-1
000.072 D: 00: obj   000..184 004 {"channels":[[3,2],[3,2],[3,2],[3,2],[3,2],[3,1],[3,4],[0,0]],"lut0":{"1":"unknown","2":"running","3":"idle"},"lut1":{"1":"unknown","2":"success","3":"unstable","4":"failure"},"res":1}
000.072 D: 01: str   002..010 001 channels
000.082 D: 02: arr   012..061 008 [[3,2],[3,2],[3,2],[3,2],[3,2],[3,1],[3,4],[0,0]]
000.082 D: 03: arr   013..018 002 [3,2]
000.082 D: 04: prim  014..015 000 3
000.092 D: 05: prim  016..017 000 2
000.092 D: 06: arr   019..024 002 [3,2]
000.093 D: 07: prim  020..021 000 3
000.093 D: 08: prim  022..023 000 2
000.103 D: 09: arr   025..030 002 [3,2]
000.103 D: 10: prim  026..027 000 3
000.103 D: 11: prim  028..029 000 2
000.113 D: 12: arr   031..036 002 [3,2]
000.113 D: 13: prim  032..033 000 3
000.113 D: 14: prim  034..035 000 2
000.113 D: 15: arr   037..042 002 [3,2]
000.124 D: 16: prim  038..039 000 3
000.124 D: 17: prim  040..041 000 2
000.124 D: 18: arr   043..048 002 [3,1]
000.124 D: 19: prim  044..045 000 3
000.124 D: 20: prim  046..047 000 1
000.134 D: 21: arr   049..054 002 [3,4]
000.134 D: 22: prim  050..051 000 3
000.134 D: 23: prim  052..053 000 4
000.144 D: 24: arr   055..060 002 [0,0]
000.144 D: 25: prim  056..057 000 0
000.144 D: 26: prim  058..059 000 0
000.144 D: 27: str   063..067 001 lut0
000.155 D: 28: obj   069..109 003 {"1":"unknown","2":"running","3":"idle"}
000.155 D: 29: str   071..072 001 1
000.155 D: 30: str   075..082 000 unknown
000.165 D: 31: str   085..086 001 2
000.165 D: 32: str   089..096 000 running
000.165 D: 33: str   099..100 001 3
000.165 D: 34: str   103..107 000 idle
000.175 D: 35: str   111..115 001 lut1
000.186 D: 36: obj   117..175 004 {"1":"unknown","2":"success","3":"unstable","4":"failure"}
000.186 D: 37: str   119..120 001 1
000.186 D: 38: str   123..130 000 unknown
000.186 D: 39: str   133..134 001 2
000.196 D: 40: str   137..144 000 success
000.196 D: 41: str   147..148 001 3
000.196 D: 42: str   151..159 000 unstable
000.196 D: 43: str   162..163 001 4
000.206 D: 44: str   166..173 000 failure
000.206 D: 45: str   177..180 001 res
000.206 D: 46: prim  182..183 000 1
000.216 D: 47: undef 000..000 000 
000.216 D: 48: undef 000..000 000 
000.216 D: 49: undef 000..000 000 

000.041 D: json={"channels":[["idle","success"],["idle","success"],["idle","success"],["idle","success"],["idle","success"],["idle","success"],["idle","failure"],["unknown","unknown"]],"res":1}
000.041 D: res=29
000.051 D: parser: pos=177 toknext=29 toksuper=-1
000.061 D: 00: obj   000..177 2 <-1 {"channels":[["idle","success"],["idle","success"],["idle","success"],["idle","success"],["idle","success"],["idle","success"],["idle","failure"],["unknown","unknown"]],"res":1}
000.072 D: 01: str   002..010 1 < 0 channels
000.082 D: 02: arr   012..168 8 < 1 [["idle","success"],["idle","success"],["idle","success"],["idle","success"],["idle","success"],["idle","success"],["idle","failure"],["unknown","unknown"]]
000.092 D: 03: arr   013..031 2 < 2 ["idle","success"]
000.092 D: 04: str   015..019 0 < 3 idle
000.092 D: 05: str   022..029 0 < 3 success
000.103 D: 06: arr   032..050 2 < 2 ["idle","success"]
000.103 D: 07: str   034..038 0 < 6 idle
000.103 D: 08: str   041..048 0 < 6 success
000.113 D: 09: arr   051..069 2 < 2 ["idle","success"]
000.113 D: 10: str   053..057 0 < 9 idle
000.113 D: 11: str   060..067 0 < 9 success
000.123 D: 12: arr   070..088 2 < 2 ["idle","success"]
000.123 D: 13: str   072..076 0 <12 idle
000.123 D: 14: str   079..086 0 <12 success
000.134 D: 15: arr   089..107 2 < 2 ["idle","success"]
000.134 D: 16: str   091..095 0 <15 idle
000.134 D: 17: str   098..105 0 <15 success
000.144 D: 18: arr   108..126 2 < 2 ["idle","success"]
000.144 D: 19: str   110..114 0 <18 idle
000.144 D: 20: str   117..124 0 <18 success
000.154 D: 21: arr   127..145 2 < 2 ["idle","failure"]
000.154 D: 22: str   129..133 0 <21 idle
000.155 D: 23: str   136..143 0 <21 failure
000.165 D: 24: arr   146..167 2 < 2 ["unknown","unknown"]
000.165 D: 25: str   148..155 0 <24 unknown
000.165 D: 26: str   158..165 0 <24 unknown
000.165 D: 27: str   170..173 1 < 0 res
000.175 D: 28: prim  175..176 0 <27 1
000.175 D: 29: undef 000..000 0 < 0 



*/
