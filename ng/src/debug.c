/*!
    \file
    \brief flipflip's Tschenggins Lämpli: debugging output (see \ref FF_DEBUG)

    - Copyright (c) 2018 Philippe Kehl (flipflip at oinkzwurgl dot org),
      https://oinkzwurgl.org/projaeggd/tschenggins-laempli
*/

#include "debug.h"

#include <stdout_redirect.h>
#include <esp/uart.h>

//ssize_t _write_stdout_r(struct _reent *r, int fd, const void *ptr, size_t len )
static ssize_t IRAM foo_write_stdout_r(struct _reent *r, int fd, const void *ptr, size_t len )
{
    for (int i = 0; i < len; i++)
    {
        uart_putc(0, ((char *)ptr)[i]);
    }
    return len;
}

//! initialise debugging output
void debugInit(void)
{
    uart_set_baud(0, 115200);
    //sdk_os_install_putc1(foo_putc); // nope!
    set_write_stdout(foo_write_stdout_r);
}


void HEXDUMP(const void *pkData, int size)
{

}


// eof
