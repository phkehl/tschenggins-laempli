/*!
    \file
    \brief flipflip's Tschenggins Lämpli: HTML templates (see \ref USER_HTML)

    - Copyright (c) 2017 Philippe Kehl <flipflip at oinkzwurgl dot org>,
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

    \addtogroup USER_HTML

    @{
*/

#include "user_html.h"
#include "version_gen.h"
#include "html_gen.h"

// -------------------------------------------------------------------------------------------------

void ICACHE_FLASH_ATTR htmlInit(void)
{
    DEBUG("html: init");
}

// -------------------------------------------------------------------------------------------------

static const char skHtmlDefaultTemplate[] PROGMEM = USER_HTML_TEMPL_HTML_STR;

int ICACHE_FLASH_ATTR htmlRender(const char *template, char *html, const int size,
    const char *keys[], const char *vals[], const int numKV, const bool clearKeys)
{
    // use default template
    if (template == NULL)
    {
        template = skHtmlDefaultTemplate;
    }

    // copy template
    if (template != html)
    {
        os_memset(html, 0, size);
        //os_strcpy(html, template); // works for ROM strings, too it seems
        strcpy_P(html, template);
        // FIXME: add size check
    }
    // else: template == html

    const int htmlLenMax = size - 1;
    int htmlLen = os_strlen(html);
    //DEBUG("htmlRender() template=%p html=%p %d/%d", template, html, htmlLen, htmlLenMax);

    // interpolate keys
    int htmlIx = 0;
    while (htmlIx < htmlLen)
    {
        //DEBUG("htmlIx=%d: %c", htmlIx, html[htmlIx]);

        // stop at NUL character
        if (html[htmlIx] == '\0')
        {
            break;
        }

        // interpolate key?
        if (html[htmlIx] == '%')
        {
            char key[20];
            const char *pkKey = NULL;
            const char *pkVal = NULL;

            // have "%%"
            if (html[htmlIx + 1] == '%')
            {
                pkKey = "%%";
                pkVal = "%";
            }
            else
            {
                //DEBUG("key at %d? %c%c%c%c", htmlIx, html[htmlIx+0], html[htmlIx+1], html[htmlIx+2], html[htmlIx+3]);

                // look for key (/^%[A-Za-z0-9]{0,20}%$/)
                int htmlIx2 = htmlIx + 1, tIx = 0;
                key[tIx] = html[htmlIx];
                while (
                    ( (html[htmlIx2] == '%') || isalnum((int)html[htmlIx2]) ) &&
                    (htmlIx2 < htmlLen) &&
                    (tIx < ((int)sizeof(key) - 1)) )
                {
                    const char c = html[htmlIx2];
                    //DEBUG("try %c at %d", c, htmlIx2);
                    key[++tIx] = c;
                    if (c == '%')
                    {
                        break;
                    }
                    htmlIx2++;
                }

                // have key
                if ((tIx > 1) && (key[tIx] == '%'))
                {
                    key[tIx + 1] = '\0';
                    pkKey = key;

                    // find value for it
                    for (int keyIx = 0; keyIx < numKV; keyIx++)
                    {
                        if (/*os_strncmp*/strncmp_PP(keys[keyIx], &pkKey[1], tIx - 1) == 0)
                        {
                            pkVal = vals[keyIx];
                            break;
                        }
                    }
                    //DEBUG("key? %d / %s / %s", tIx, key, pkVal);

                    // handle built-in variables unless overriden by user
                    //if (pkVal == NULL)
                    //{
                    //    if (/*os_strncmp*/strncmp_PP(&pkKey[1], PSTR("MENU"), 4) == 0)
                    //    {
                    //        pkVal = sHtmlMenu;
                    //    }
                    //}

                    // no value
                    if (pkVal == NULL)
                    {
                        // clear key (replace with the empty string)
                        if (clearKeys)
                        {
                            pkVal = "";
                        }
                        // leave key in
                        else
                        {
                            pkKey = NULL;
                        }
                    }
                }
            }

            // replace key with value
            if (pkKey)
            {
                // calculate how far and in what direction we have to move the right part of the string
                int tokLen = os_strlen(pkKey);
                int valLen = os_strlen(pkVal);
                int move   = valLen - tokLen;
                //NOTICE("==> %s at %d --> %s (tokLen=%d valLen=%d move=%d)",
                //    pkKey, htmlIx, pkVal, tokLen, valLen, move);

                // remove key if there is not enough space to interpolate val
                if ((htmlLen + move) > htmlLenMax)
                {
                    ERROR("htmlRender() fail interpolate %s at %d (%d > %d)",
                        pkKey, htmlIx, htmlLen + move, htmlLenMax);
                    move = -(tokLen);
                    valLen = 0;
                    pkVal = "";
                }
                // move right part of string
                if (move < 0) // move to the left
                {
                    char *pHtml = &html[htmlIx + tokLen + move];
                    int n = htmlLen - htmlIx - tokLen + 1;
                    while (n--)
                    {
                        //*(pHtml + move) = *pHtml;
                        *pHtml = *(pHtml - move);
                        pHtml++;
                    }
                }
                else if (move > 0) // move to the right
                {
                    char *pHtml = &html[htmlLen + move];
                    int n = htmlLen - htmlIx - tokLen + 1;
                    while (n--)
                    {
                        *pHtml = *(pHtml - move);
                        pHtml--;
                    }
                }

                htmlLen += move;
                //html[htmlLen] = '\0'; // add NUL terminator

                // insert value
                int n = valLen;
                if ((const void *)pkVal > (const void *)ESP_FLASH_BASE)
                {
                    const char *p = &pkVal[n];
                    while (n--)
                    {
                        p--;
                        html[htmlIx + n] = pgm_read_uint8(p);;
                    }
                }
                else
                {
                    while (n--)
                    {
                        html[htmlIx + n] = pkVal[n];
                    }
                }

                //DEBUG("html=%s (%d, %d)", html, os_strlen(html), htmlLen);
            }

        } // html[htmlIx] == '%'

        htmlIx++;
    }
    const int lenCheck = os_strlen(html);
    //DEBUG("done %d %d", htmlLen, lenCheck);
    if (htmlLen != lenCheck)
    {
        ERROR("htmlRender() %d != %d", htmlLen, lenCheck);
    }

    return htmlLen;
}


// -------------------------------------------------------------------------------------------------


#if 0
#  warning htmlTest() compiled in
static const char templ[] PROGMEM = "foo %FOO% bar %BAR% baz %BAZ% gugus 42% %GUGUS%";
void ICACHE_FLASH_ATTR htmlTest(void)
{
#if 1
    DEBUG("html: templ=%p %s", templ, templ);

    //const char *keys[] = { "FOO" };
    //const char *vals[] = { "iamfoo" };
    //const char *keys[] = { "FOO", "BAR" };
    //const char *vals[] = { "iamfoo", "iambar" };

    //const char *keys[] = { "BAR" };
    //const char *vals[] = { "iambar" };
    //const char *keys[] = { "FOO" };
    //const char *vals[] = { "f!" };

    const char *keys[] = { "FOO", "BAR" };
    //const char *vals[] = { "iamfoo!", "b!" };
    static const char bar[] PROGMEM = "b!";
    const char *vals[] = { "iamfoo!", bar };

    char *html = os_malloc(1000);
    const int len = htmlRender(templ, html, 1000, keys, vals, (int)NUMOF(keys), true);
    DEBUG("html=%s (%p, %d, %d)", html, html, len, os_strlen(html));
    os_free(html);
#endif

#if 1
    DEBUG("html: test %p %s", skHtmlDefaultTemplate, skHtmlDefaultTemplate);

    uint8_t *resp = os_malloc(8192);
    const int len2 = htmlRender(NULL, (char *)resp, 8192, NULL, NULL, 0, true);
    DEBUG("resp=%s (%p, %d, %d)", resp, resp, len2, os_strlen(resp));
    os_free(resp);
#endif
}
#endif

/* *********************************************************************************************** */
//@}
// eof
