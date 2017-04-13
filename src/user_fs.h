/*!
    \file
    \brief flipflip's Tschenggins Lämpli: filesystem (see \ref USER_FS)

    - Copyright (c) 2017 Philippe Kehl <flipflip at oinkzwurgl dot org>,
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli

    \defgroup USER_FS FS
    \ingroup USER

    This implements a read-only filesystem. It registers a \ref USER_HTTPD callback for every file
    found. The \c tools/mkfs.pl script is used to generate an image of the filesystem, which is
    loaded to the flash (see \c Makefile).

    @{
*/
#ifndef __USER_FS_H____
#define __USER_FS_H____

#include "user_stuff.h"

//! initialise filesystem and register files in the \ref USER_HTTPD
void fsInit(void);

#endif // __USER_FS_H____
//@}
// eof
