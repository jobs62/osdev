#include "stdlib.h"

void *memcpy(void *dst, const void *src, unsigned long size) {
    char *dp = (char *)dst;
    const char *sp = (const char *)src;
    while (size--) {
        *dp++ = *sp++;
    }
    return (dst);
}


void *memset(void* dst, int c, unsigned long size) {
    unsigned char *p = (unsigned char *)dst;
    while (size--) {
        *p++ = (unsigned char)c;
    }
    return (dst);
}