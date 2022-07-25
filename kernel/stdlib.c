#include "stdlib.h"
#include "vmm.h"


uint8_t __stdlib_unsafe = 1;

void *memcpy(void *dst, const void *src, unsigned long size) {
    if (!__stdlib_unsafe && (__check_ptr_write(dst, size) || __check_ptr(src, size))) {
        kprintf("memcpy ptr check fail at %s:%1d\n", __FILE__, __LINE__);
        return (0);
    }

    char *destptr = (char *)dst;
    const char *srcptr = (const char *)src;
    while (size--) {
        *destptr++ = *srcptr++;
    }
    return (dst);
}


void *memset(void* dst, int chr, size_t size) {
    if (!__stdlib_unsafe && __check_ptr(dst, size)) {
        kprintf("memset ptr check fail at %s:%1d\n", __FILE__, __LINE__);
        return (0);
    }

    unsigned char *dstptr = (unsigned char *)dst;
    while (size--) {
        *dstptr++ = (unsigned char)chr;
    }
    return (dst);
}


void *bsearch_s(const void *key, const void *base, uint32_t num, uint32_t size, cmp_func_ext_t cmp, void *ext) {
    uint32_t left = 0;
    uint32_t rigth = num - 1;
    uint32_t middle;
    int cmp_rst;

    while (left <= rigth) {
        middle = (left + rigth) / 2;
        cmp_rst = cmp(key, base + middle * size, ext);

        if (cmp_rst > 0) {
            left = middle + 1;
        } else if (cmp_rst < 0) {
            rigth = middle - 1;
        } else {
            return (void *)(base + middle * size);
        }
    }

    return (void *)(0);
}