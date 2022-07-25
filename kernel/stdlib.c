#include "stdlib.h"
#include "vmm.h"

uint8_t __stdlib_unsafe = 1;

void *memcpy(void *dst, const void *src, unsigned long size) {
    if (!__stdlib_unsafe && (__check_ptr_write(dst, size) || __check_ptr(src, size))) {
        kprintf("memcpy ptr check fail at %s:%1d\n", __FILE__, __LINE__);
        return (0);
    }

    char *dp = (char *)dst;
    const char *sp = (const char *)src;
    while (size--) {
        *dp++ = *sp++;
    }
    return (dst);
}


void *memset(void* dst, int c, unsigned long size) {
    if (!__stdlib_unsafe && __check_ptr(dst, size)) {
        kprintf("memset ptr check fail at %s:%1d\n", __FILE__, __LINE__);
        return (0);
    }

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