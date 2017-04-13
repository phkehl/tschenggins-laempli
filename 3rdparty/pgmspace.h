// see pgmspace.c for credits, copyrights and licenses

#ifndef __PGMSPACE_H__
#define __PGMSPACE_H__

#define PROGMEM     ICACHE_RODATA_ATTR STORE_ATTR
#define PGM_P  		const char *
#define PGM_VOID_P  const void *
//! put string in ROM
#define PSTR(s) (__extension__({static const char __c[] PROGMEM = (s); &__c[0];}))

#define pgm_read_uint8(addr) /* pgm_read_byte() in Arduino world */ \
        (__extension__({ \
                PGM_P __local = (PGM_P)(addr);  /* isolate varible for macro expansion */ \
                ptrdiff_t __offset = ((uint32_t)__local & 0x00000003); /* byte aligned mask */ \
                const uint32_t* __addr32 = (const uint32_t*)(const void *)((const uint8_t*)(__local)-__offset); \
                uint8_t __result = ((*__addr32) >> (__offset * 8)); \
                __result; }))

#define pgm_read_uint16(addr) /* pgm_read_word() in Arduino world */ \
        (__extension__({ \
                PGM_P __local = (PGM_P)(addr); /* isolate varible for macro expansion */ \
                ptrdiff_t __offset = ((uint32_t)__local & 0x00000002);   /* word aligned mask */ \
                const uint32_t* __addr32 = (const uint32_t*)(const void *)((const uint8_t*)(__local) - __offset); \
                uint16_t __result = ((*__addr32) >> (__offset * 8)); \
                __result; }))

#define pgm_read_uint32(addr) /* not exactly required */ \
    *((const uint32_t *)addr)


// memcpy() that can copy from ROM (and RAM)
void memcpy_P(void *dst, const void *src, size_t size);

// strcpy() that can copy from ROM (and RAM)
char *strcpy_P(char *dst, const char *src);

// strncmp() that can use both strings from ROM (and RAM)
int strncmp_PP(const char *s1, const char *s2, int size);

// strcmp() that can use both strings from ROM (and RAM)
#define strcmp_PP(s1, s2) strncmp_PP(s1, s2, INT32_MAX)

// strlen() that works with strings from ROM (and RAM)
int strlen_P(const char *s);

// strcat() that can copy from ROM (and RAM) strings
char *strcat_P(char *dst, const char *src);

// strstr() that can find needle from ROM (and RAM)
char *strstr_P(const char *haystack, const char *needle);

// strcasestr() that can find needle from ROM (and RAM)
char *strcasestr_P(const char *haystack, const char *needle);


/* *********************************************************************************************** */

#endif // __PGMSPACE_H__
