#ifndef __VMM__
#define __VMM__

#include "stdtype.h"
#include "fat.h"

#define VM_PDINDEX_SHIFT 22
#define VM_PTINDEX_SHIFT 12
#define VM_PAGE_PRESENT 0x1
#define VM_PAGE_READ_WRITE 0x2
#define VM_PAGE_USER_ACCESS 0x4
#define GET_BEGINGIN_PREV_PAGE(page) ((unsigned int *)((((unsigned int)(page) >> VM_PTINDEX_SHIFT) - 1) << VM_PTINDEX_SHIFT))

#define VM_MAP_ANONYMOUS 0x00000001
#define VM_MAP_FILE      0x00000002
#define VM_MAP_PRIVATE   0x00000100
#define VM_MAP_SHARED    0x00000200
#define VM_MAP_WRITE     0x00010000
#define VM_MAP_KERNEL    0x10000000
#define VM_MAP_USER      0x20000000

physaddr_t get_physaddr(virtaddr_t virtaddr);
int map_page(physaddr_t physadd, virtaddr_t virtaddr, unsigned int flags);
void unmap_page(virtaddr_t virtaddr);

void *add_vm_entry(void *hint, uint32_t size, uint32_t flags, struct fat_sector_itearator *sec);
void rm_vm_entry(void *base);
void dump_vm_map(void);

#endif