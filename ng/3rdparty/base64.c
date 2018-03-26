// based on public domain code from libmicrohttpd (https://www.gnu.org/software/libmicrohttpd/)
// base64.c by Matthieu Speder and tlsauthentication.c

#include "base64.h"

static const char base64_chars[] =
  "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static const char base64_digits[] =
{
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 62, 0, 0, 0, 63, 52, 53, 54, 55, 56, 57, 58, 59, 60, 61,
    0, 0, 0, -1, 0, 0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13,
    14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 0, 0, 0, 0, 0, 0, 26,
    27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44,
    45, 46, 47, 48, 49, 50, 51, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

bool base64dec(const char *src, char *dst, const int dstlen)
{
    const int inlen = strlen(src);

    if ( ((inlen % 4) != 0) || (dstlen < BASE64_DECLEN(inlen)) )
    {
        // wrong base64 string length
        dst[0] = '\0';
        return false;
    }

    char *dest = dst;
    while (*src)
    {
        char a = base64_digits[(unsigned char)*(src++)];
        char b = base64_digits[(unsigned char)*(src++)];
        char c = base64_digits[(unsigned char)*(src++)];
        char d = base64_digits[(unsigned char)*(src++)];
        *(dest++) = (a << 2) | ((b & 0x30) >> 4);
        if (c == (char)-1)
        {
            break;
        }
        *(dest++) = ((b & 0x0f) << 4) | ((c & 0x3c) >> 2);
        if (d == (char)-1)
        {
            break;
        }
        *(dest++) = ((c & 0x03) << 6) | d;
    }
    *dest = '\0';
    return true;
}

#define BASE64_ENCLEN(msglen) ( (((msglen) * 4 / 3) / 3 + 2) * 3 )

bool base64enc(const char *src, char *dst, const int dstlen)
{
    const int srclen = strlen(src);
    const int reqdstlen = BASE64_ENCLEN(srclen);
    dst[0] = '\0';
    if (dstlen < reqdstlen)
    {
        return false;
    }
    int dix = 0;
    for (int i = 0; i < srclen; i += 3)
    {
        const uint32_t l =
                                    (((uint32_t) src[i])     << 16)
            | (((i + 1) < srclen) ? (((uint32_t) src[i + 1]) <<  8) : 0)
            | (((i + 2) < srclen) ?  ((uint32_t) src[i + 2])        : 0);

        dst[dix++] = base64_chars[(l >> 18) & 0x3f];
        dst[dix++] = base64_chars[(l >> 12) & 0x3f];

        if ((i + 1) < srclen)
        {
            dst[dix++] = base64_chars[(l >> 6) & 0x3f];
        }
        if ((i + 2) < srclen)
        {
            dst[dix++] = base64_chars[l & 0x3f];
        }
    }

    int pad = srclen % 3;
    if (pad)
    {
        pad = 3 - pad;
        while (pad--)
        {
            dst[dix++] = '=';
        }
    }
    dst[dix] = '\0';

    return true;
}


// eof
