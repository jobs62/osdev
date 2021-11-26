#ifndef __PMM__
#define __PMM__

#include "stdtype.h"

#define PAGE_LEN 1024
#define PAGE_SIZE (PAGE_LEN * sizeof(uint32_t))

void bitmap_clear(void);
void bitmap_mark_as_used(physaddr_t page);
void bitmap_mark_as_free(physaddr_t page);
int bitmap_page_status(physaddr_t page);
physaddr_t bitmap_find_free_page();
void dump_bitmap();
physaddr_t get_physaddr(virtaddr_t virtaddr);

#endif