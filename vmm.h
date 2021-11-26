#ifndef __VMM__
#define __VMM__

#include "stdtype.h"

#define VM_PDINDEX_SHIFT 22
#define VM_PTINDEX_SHIFT 12
#define GET_BEGINGIN_PREV_PAGE(page) ((unsigned int *)((((unsigned int)(page) >> VM_PTINDEX_SHIFT) - 1) << VM_PTINDEX_SHIFT))

physaddr_t get_physaddr(virtaddr_t virtaddr);
void map_page(physaddr_t physadd, virtaddr_t virtaddr, unsigned int flags);
void unmap_page(virtaddr_t virtaddr);

void *get_kbrk();
void set_kbrk(void *addr);

#endif