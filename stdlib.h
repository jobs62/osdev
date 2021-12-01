#ifndef __STDLIB__
#define __STDLIB__

#include "stdtype.h"
typedef int (*cmp_func_ext_t)(const void*, const void*, void*);

void *memcpy(void *dst, const void *src, unsigned long size);
void *memset(void* dst, int c, unsigned long size);
void *bsearch_s(const void *key, const void *base, uint32_t num, uint32_t size, cmp_func_ext_t cmp, void *ext);

#endif