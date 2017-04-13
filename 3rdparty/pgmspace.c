/*
Based on: https://github.com/esp8266/Arduino

pgmspace.cpp - string functions that support PROGMEM
Copyright (c) 2015 Michael C. Miller.  All right reserved.

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/


#include "user_stuff.h"

#include "pgmspace.h"

void ICACHE_FLASH_ATTR memcpy_P(void *dst, const void *src, size_t size)
{
    const uint8_t *_src = (const uint8_t *)src;
    uint8_t *_dst = (uint8_t *)dst;
    if (src >= (const void *)ESP_FLASH_BASE) // copy stuff from flash
    {
        while (size--)
        {
            *_dst++ = pgm_read_uint8(_src++);
        }
    }
    else
    {
        os_memcpy(_dst, _src, size);
    }
}

char * ICACHE_FLASH_ATTR strcpy_P(char *dst, const char *src)
{
    if ((const void *)src >= (const void *)ESP_FLASH_BASE) // copy stuff from flash
    {
        char *_dst = dst;
        char c = pgm_read_uint8(src++);
        while (c != '\0')
        {
            *_dst++ = c;
            c = pgm_read_uint8(src++);
        }
        *_dst = '\0';
    }
    else
    {
        os_strcpy(dst, src);
    }
    return dst;
}

int ICACHE_FLASH_ATTR strncmp_PP(const char *s1, const char *s2, int size)
{
   int res = 0;

   while (size > 0)
   {
       const char c1 = (char)pgm_read_uint8(s1++);
       const char c2 = (char)pgm_read_uint8(s2++);
       res = (int)c1 - (int)c2;
       if ( (res != 0) || (c2 == '\0') )
       {
           break;
       }
       size--;
   }

   return res;
}

int ICACHE_FLASH_ATTR strlen_P(const char *s)
{
    const char *pS = s;
    while (pgm_read_uint8(pS) != 0)
    {
        pS++;
    }
    return (int)(pS - s);
}

char * ICACHE_FLASH_ATTR strcat_P(char *dst, const char *src)
{
    const int len = os_strlen(dst);
    return strcpy_P(&dst[len], src);
}

char * ICACHE_FLASH_ATTR strstr_P(const char *haystack, const char *needle)
{
   if (haystack[0] == 0)
   {
       if (pgm_read_uint8(needle))
       {
           return NULL;
       }
       return (char *)haystack;
   }

   while (*haystack)
   {
       int i = 0;
       while (true)
       {
           char n = pgm_read_uint8(needle + i);
           if (n == 0)
           {
               return (char *)haystack;
           }
           if (n != haystack[i])
           {
               break;
           }
           ++i;
       }
       ++haystack;
   }
   return NULL;
}

char * ICACHE_FLASH_ATTR strcasestr_P(const char *haystack, const char *needle)
{
   if (haystack[0] == 0)
   {
       if (pgm_read_uint8(needle))
       {
           return NULL;
       }
       return (char *)haystack;
   }

   while (*haystack)
   {
       int i = 0;
       while (true)
       {
           char n = pgm_read_uint8(needle + i);
           if (n == 0)
           {
               return (char *)haystack;
           }
           if ( tolower((int)n) != tolower((int)haystack[i]) )
           {
               break;
           }
           ++i;
       }
       ++haystack;
   }
   return NULL;
}

// eof
