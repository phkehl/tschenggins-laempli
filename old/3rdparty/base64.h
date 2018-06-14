// base64 encode and decode, see base64.c for credits, copyrights and licenses

#ifndef __BASE64_H__
#define __BASE64_H__

#include <stdbool.h>
#include <stddef.h>

// decode base64 to string
bool base64dec(const char *src, char *dst, const int dstlen);

// required destination string length (incl. \0) given a base64 encoded message
#define BASE64_DECLEN(msglen) ( ((msglen) / 4 * 3 + 1) )

// encode string to base64
bool base64enc(const char *src, char *dst, const int dstlen);

// required destination string length (incl. \0) given a message
#define BASE64_ENCLEN(msglen) ( (((msglen) * 4 / 3) / 3 + 2) * 3 )




#endif
