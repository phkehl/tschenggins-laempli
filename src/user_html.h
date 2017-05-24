/*!
    \file
    \brief flipflip's Tschenggins Lämpli: HTML templates (see \ref USER_HTML)

    - Copyright (c) 2017 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

    \defgroup USER_HTML HTML
    \ingroup USER

    This implements a simple HTML template system.

    It also comes with the \c tools/html2c.pl that translates html files into string literals can be
    used as input templates (or otherwise).

    @{
*/
#ifndef __USER_HTML_H__
#define __USER_HTML_H__

#include "user_stuff.h"

//! initialise HMTL template engine
void htmlInit(void);

//! render a HTML template
/*!
    Interpolates given variables in a template (in the form "%KEYNAME%") with values. Optionally removes
    unmatched keys. Keys for which the interploation would make the target \c html overflow are
    ignored and the corresponding variable are removed.

    \param[in] template   the template string (can be a ROM string), or #NULL to use the built-in one
    \param[in] html       target HTML string (can be the same as the \c template)
    \param[in] size       size of \c html buffer (> strlen(template)+1 !)
    \param[in] keys       parameter names
    \param[in] vals       parameter values
    \param[in] numKV      number of \c keys and \c vals
    \param[in] clearKeys  clear keys from template that have not been interpolated
    \returns the length of the \c html string after interpolation

    \note There is no size check that \c template fits into \c html.

    Example:
\code{.c}
const char *pkTemplate = "foo %BAR% %BAZ%baz %GUGUS% bla";
char html[30];
const char *keys[] = { PSTR("BAR"), PSTR("BAZ") };
const char *vals[] = { PSTR("11"), PSTR("22222") };
const int htmlLen = htmlRender(pkTemplate, html, sizeof(html), keys, vals, NUMOF(keys), true);
// --> htmlLen = 10, html = "foo 11 22222baz bla\0\0\0\0\0\0\0\0\0\0\0"
\endcode
And since \c template and \c html can be the same buffer:
\code{.c}
const char *pkTemplate = "foo %BAR% %BAZ%baz %GUGUS% bla";
char html[30];
strcpy_P(html, pkTemplate);
const char *keys[] = { PSTR("BAR"), PSTR("BAZ") };
const char *vals[] = { PSTR("11"), PSTR("22222") };
const int htmlLen = htmlRender(html, html, sizeof(html), keys, vals, NUMOF(keys), true);
// --> htmlLen = 10, html = "foo 11 22222baz bla\0\0\0\0\0\0\0\0\0\0\0"
\endcode
*/
int htmlRender(const char *template, char *html, const int size,
    const char *keys[], const char *vals[], const int numKV,
    const bool clearKeys);


//void htmlTest(void);


#endif // __USER_HTML_H__
//@}
// eof
