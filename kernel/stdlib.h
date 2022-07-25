#ifndef __STDLIB__
#define __STDLIB__

#include <stdint.h>
#include <stddef.h>
#define _HAVE_SIZE_T

typedef int (*cmp_func_ext_t)(const void*, const void*, void*);

extern uint8_t __stdlib_unsafe;

void *memcpy(void *dst, const void *src, unsigned long size);
void *memset(void* dst, int chr, size_t size);
void *bsearch_s(const void *key, const void *base, uint32_t num, uint32_t size, cmp_func_ext_t cmp, void *ext);

inline void *memset_unsafe(void* dst, int c, size_t size) {
    __stdlib_unsafe = 1;
    void * r = memset(dst, c, size);
    __stdlib_unsafe = 0;
    return r;
}

void kprintf(const char *format, ...);

#define min(a, b) ((a) < (b) ? (a) : (b))

#endif