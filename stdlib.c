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


void *bsearch_s(const void *key, const void *base, uint32_t num, uint32_t size, cmp_func_ext_t cmp, void *ext) {
    uint32_t l = 0;
    uint32_t r = num - 1;
    uint32_t m;
    int cmp_rst;

    while (l <= r) {
        m = (l + r) / 2;
        cmp_rst = cmp(key, base + m * size, ext);

        if (cmp_rst > 0) {
            l = m + 1;
        } else if (cmp_rst < 0) {
            r = m - 1;
        } else {
            return (void *)(base + m * size);
        }
    }

    return (void *)(0);
}

uint32_t min(uint32_t a, uint32_t b) {
    if (a > b){
        return b;
    }
    return a;
}