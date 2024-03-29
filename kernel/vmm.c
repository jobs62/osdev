#include "bdev.h"
#include <stdint.h>
#include "io.h"
#include "vmm.h"
#include "pmm.h"
#include "interrupt.h"
#include "stdlib.h"
#include "fat.h"

#define FIRST_12BITS_MASK 0xFFF


#define KERNAL_MAP_BASE 0xC0000000
#define VM_PT_MOUNT_BASE (virtaddr_t)0xFFC00000
#define VM_PDINDEX_TO_PTR(index) ((unsigned int *)((uint32_t)VM_PT_MOUNT_BASE | ((index) << VM_PTINDEX_SHIFT)))
#define VM_VITRADDR_TO_PDINDEX(virtaddr) ((uint32_t)(virtaddr) >> VM_PDINDEX_SHIFT)
#define VM_VITRADDR_TO_PTINDEX(virtaddr) (((uint32_t)(virtaddr) >> VM_PTINDEX_SHIFT) & 0x03FF)
#define VM_INDEXES_TO_PTR(pdindex, ptindex) (void *)(((pdindex) << VM_PDINDEX_SHIFT) | ((ptindex) << VM_PTINDEX_SHIFT))



struct vm_entry {
    uintptr_t base;
    uint32_t size;
    uint32_t flags;
    struct file *file;
    uint32_t offset;
    uint32_t disksize;
};

struct vm_entry vm_map[1024]; //that should be in task struct
uint32_t vm_map_size = 0;

int vm_entry_cmp(const void  *key, const void *obj, void *ext __attribute__((unused))) {
    const struct vm_entry *map_entry = (const struct vm_entry *)obj;

    if (key < map_entry->base) {
        return -1;
    }
    
    if (key >= map_entry->base + map_entry->size) {
        return 1;
    }

    return 0;
}

extern void PAGE_DIRECTORY(void);
extern void PAGE_TABLE(void);
unsigned int * kpage_directory = (unsigned int *)&PAGE_DIRECTORY;

static void page_fault_interrupt_handler(unsigned int interrupt, void *ext);
static int map_change_permission(virtaddr_t virtaddr, unsigned int flags);

physaddr_t get_physaddr(virtaddr_t virtaddr) {
    unsigned int pdindex = VM_VITRADDR_TO_PDINDEX(virtaddr);
    unsigned int ptindex = VM_VITRADDR_TO_PTINDEX(virtaddr);
    //kprintf("pd: 0x%8h; pt: 0x%8h\n", pdindex, ptindex);

    unsigned int pdentry = (unsigned int)kpage_directory[pdindex];
    if ((pdentry & 0x00000001) == 0) {
        //kprintf("pt not present\n");
        return (0);
    }
   
    //So i guess i have to map every pt to a specific virtual location to be able to find them
    unsigned int *pagetable = VM_PDINDEX_TO_PTR(pdindex);
    unsigned int ptentry = pagetable[ptindex];
    //kprintf("pt: 0x%8h; ptentry: 0x%8h\n", pt, ptentry);
    if ((ptentry & 0x00000001) == 0) {
        //kprintf("pte not present\n");
        return (0);
    }
    //kprintf("pt: 0x%8h\n", pt);

    return (ptentry & ~FIRST_12BITS_MASK) + ((unsigned int)virtaddr & FIRST_12BITS_MASK);
}

uint16_t get_flags(virtaddr_t virtaddr) {
    unsigned int pdindex = VM_VITRADDR_TO_PDINDEX(virtaddr);
    unsigned int ptindex = VM_VITRADDR_TO_PTINDEX(virtaddr);
    //kprintf("pd: 0x%8h; pt: 0x%8h\n", pdindex, ptindex);

    unsigned int pdentry = (unsigned int)kpage_directory[pdindex];
    if ((pdentry & 0x00000001) == 0) {
        //kprintf("pt not present\n");
        return (0);
    }
   
    //So i guess i have to map every pt to a specific virtual location to be able to find them
    unsigned int *pagetable = VM_PDINDEX_TO_PTR(pdindex);
    unsigned int ptentry = pagetable[ptindex];
    //kprintf("pt: 0x%8h; ptentry: 0x%8h\n", pt, ptentry);
    if ((ptentry & 0x00000001) == 0) {
        //kprintf("pte not present\n");
        return (0);
    }
    //kprintf("pt: 0x%8h\n", pt);

    return (ptentry & FIRST_12BITS_MASK);
}

int map_page(physaddr_t physadd, virtaddr_t virtaddr, unsigned int flags) {
    kprintf("map_page: physadd: 0x%8h; virtaddr: 0x%8h; flags: 0x%8h\n", physadd, virtaddr, flags);

    unsigned int pdindex = VM_VITRADDR_TO_PDINDEX(virtaddr);
    unsigned int ptindex = VM_VITRADDR_TO_PTINDEX(virtaddr);
    unsigned int pdentry = (unsigned int)kpage_directory[pdindex];
    unsigned int *pagetable = VM_PDINDEX_TO_PTR(pdindex);

    if ((pdentry & 0x00000001) == 0) {
        //alloc page
        physaddr_t pagetable_physmap = bitmap_find_free_page();
        kprintf("pa: 0x%8h\n", pagetable_physmap);
        if (pagetable_physmap == 0) {
            kprintf("ERROR: could not get page\n");
            return 1;
        }

        bitmap_mark_as_used(pagetable_physmap);

        //map page
        map_page(pagetable_physmap, pagetable, VM_PAGE_READ_WRITE);
        kpage_directory[pdindex] = (pagetable_physmap & ~FIRST_12BITS_MASK) | VM_PAGE_READ_WRITE | (flags & VM_PAGE_USER_ACCESS) | VM_PAGE_PRESENT; //if map if showed with user access flag set, page direcotry should also have it to allow user
        
        //init page
        memset(pagetable, 0, PAGE_SIZE);
    }

    //kprintf("pt: 0x%8h; ptindex: %1d\n", pt, ptindex);
    if ((pagetable[ptindex] & 0x00000001) != 0) {
        kprintf("ERROR: pte is present; map not free to use !\n");
        return 2;
    }

    pagetable[ptindex] = (physadd & ~FIRST_12BITS_MASK) | (flags & FIRST_12BITS_MASK) | VM_PAGE_PRESENT;

    return (0);
}

static int map_change_permission(virtaddr_t virtaddr, unsigned int flags) {
    kprintf("map_change_permission: virtaddr: 0x%8h; flags: 0x%8h\n", virtaddr, flags);

    unsigned int pdindex = VM_VITRADDR_TO_PDINDEX(virtaddr);
    unsigned int ptindex = VM_VITRADDR_TO_PTINDEX(virtaddr);
    unsigned int pdentry = (unsigned int)kpage_directory[pdindex];
    unsigned int *pagetable = VM_PDINDEX_TO_PTR(pdindex);

    if ((pdentry & 0x00000001) == 0) {
        kprintf("ERROR: map_change_permission: addr not mapped");
        return (0);
    }

    if ((pagetable[ptindex] & 0x00000001) == 0) {
        kprintf("ERROR: map_change_permission: addr not mapped 2");
        return 2;
    }

    pagetable[ptindex] = (pagetable[ptindex] & ~FIRST_12BITS_MASK) | (flags & FIRST_12BITS_MASK) | VM_PAGE_PRESENT;

    return (0);
}


void unmap_page(virtaddr_t virtaddr) {
    unsigned int pdindex = VM_VITRADDR_TO_PDINDEX(virtaddr);
    unsigned int ptindex = VM_VITRADDR_TO_PTINDEX(virtaddr);
    unsigned int *pagetable = VM_PDINDEX_TO_PTR(pdindex);

    kprintf("map_page: virtaddr: 0x%8h; pdindex: 0x%8h; ptindex: 0x%8h; pt: 0x%8h\n", virtaddr, pdindex, ptindex, pagetable);

    pagetable[ptindex] = 0;

    int index;
    for (index = 0; index < PAGE_LEN; index++) {
        if (pagetable[index] != 0) {
            break;
        }
    }

    if (index == PAGE_LEN) {
        kpage_directory[pdindex] = 0;
        physaddr_t page = get_physaddr(pagetable);
        unmap_page(pagetable);
        bitmap_mark_as_free(page);
    }
}

void vmm_init() {
    unsigned int *vmm_base = VM_PT_MOUNT_BASE;

    unsigned int pagetable_physmap = bitmap_find_free_page();
    if (pagetable_physmap == 0) {
        kprintf("ERROR: vmm_init: could not get page\n");
        return;
    }

    bitmap_mark_as_used(pagetable_physmap);
    kpage_directory[VM_VITRADDR_TO_PDINDEX(VM_PT_MOUNT_BASE)] = (pagetable_physmap & ~FIRST_12BITS_MASK) | VM_PAGE_READ_WRITE | VM_PAGE_PRESENT;
    
    //use the last page of the bootloaded page table to bootstrap the maps' map
    //tmp = ((unsigned int *)&PAGE_TABLE)[1023];
    ((unsigned int *)&PAGE_TABLE)[PAGE_LEN - 1] = (pagetable_physmap & ~FIRST_12BITS_MASK) | VM_PAGE_READ_WRITE | VM_PAGE_PRESENT;
    vmm_base = VM_INDEXES_TO_PTR(VM_VITRADDR_TO_PDINDEX(KERNAL_MAP_BASE), PAGE_LEN - 1);
    memset(vmm_base, 0, PAGE_LEN);
    vmm_base[VM_VITRADDR_TO_PDINDEX(VM_PT_MOUNT_BASE)] = (pagetable_physmap & ~FIRST_12BITS_MASK) | VM_PAGE_READ_WRITE | VM_PAGE_PRESENT;
    vmm_base = VM_PT_MOUNT_BASE;

    //clean up the booststrap
    flush_tlb_single(pagetable_physmap);
    ((unsigned int *)&PAGE_TABLE)[PAGE_LEN - 1] = (pagetable_physmap & ~FIRST_12BITS_MASK) | VM_PAGE_READ_WRITE;

    vmm_base[VM_VITRADDR_TO_PDINDEX(VM_PT_MOUNT_BASE) * PAGE_LEN + VM_VITRADDR_TO_PDINDEX(KERNAL_MAP_BASE)] = ((unsigned int)(&PAGE_TABLE - KERNAL_MAP_BASE) & ~FIRST_12BITS_MASK) | VM_PAGE_READ_WRITE | VM_PAGE_PRESENT;

    memset(vm_map, 0, sizeof(vm_map));
    vm_map[0].base = (void *)0xffffffff;
    vm_map_size = 1;
    register_interrupt(0xE, page_fault_interrupt_handler, 0);

    __stdlib_unsafe = 0;
}

static int is_hint_allowed(uintptr_t hint, size_t size, int flags) {
    if (flags & VM_MAP_USER && hint + size >= KERNAL_MAP_BASE) {
        return 0;
    }

    if (flags & VM_MAP_KERNEL && (hint < 0xC0200000 || hint + size > 0xFFD00000)) {
        return 0;
    }

    return 1;
}

static unsigned long int next = 4; //https://xkcd.com/221/
uint32_t rdrand_rand(void) {
    next = next * 1103515245 + 12345;
    return (unsigned int) (next / 65536) % 32768;
}

void *add_vm_entry(void *hint, uint32_t size, uint32_t flags, struct file *file, uint32_t offset, uint32_t disksize) {
    //check flags for idotique things
    if ((flags & VM_MAP_USER) && (flags & VM_MAP_KERNEL)) {
        return 0;
    }

    if ((flags & VM_MAP_SHARED) && (flags & VM_MAP_PRIVATE)) {
        return 0;
    }

    if ((flags & VM_MAP_ANONYMOUS) && (flags & VM_MAP_FILE)) {
        return 0;
    }

    if ((flags & VM_MAP_FILE) && file == (void*)0) {
        return 0;
    }

    //constrain hint
    if (flags & VM_MAP_USER && hint + size >= KERNAL_MAP_BASE) {
        hint = rdrand_rand() % 0xC0200000;
    }

    if (flags & VM_MAP_KERNEL && (hint < 0xC0200000 || hint + size > 0xFFD00000)) {
        hint = rdrand_rand() % (0xFFD00000 - 0xC0200000) + 0xC0200000;
    }

    hint = (virtaddr_t)((uint32_t)hint & ~FIRST_12BITS_MASK);

    uint32_t best_hint;
    uint32_t best_lost_space;
    uint32_t lost_space;
    uint32_t index;

    best_lost_space = ~0;
    best_hint = 0;

check_with_hint:
    index = 0;

    //test i veux dire que l'on peu le foutre entre i-1 et i
    if (vm_map[0].base > hint + size) {
        //it fit here with the hint
        goto fit_with_hint;
    }
    
    for (index = 1; index < vm_map_size; index++) {
        if (vm_map[index - 1].base + vm_map[index - 1].size <= hint && vm_map[index].base > hint + size) {
fit_with_hint:
            //it fit here with the hint
            for (uint32_t j = vm_map_size; j > index; j--) {
                memcpy(&vm_map[j], &vm_map[j-1], sizeof(struct vm_entry));
            }

            vm_map[index].base = hint;
            vm_map[index].size = size;
            vm_map[index].flags = flags;
            vm_map[index].file = file;
            vm_map[index].offset = offset;
            vm_map[index].disksize = disksize;
            vm_map_size++;

            return (vm_map[index].base);
        }

        uint32_t potential_hint = vm_map[index - 1].base + vm_map[index - 1].size;
        if (vm_map[index].base - potential_hint >= size && is_hint_allowed(potential_hint, size, flags)) {
            //it fit here, even if it's not the choosen hint
            lost_space = vm_map[index].base - potential_hint - size;
            if (lost_space < best_lost_space) {
                best_lost_space = lost_space;
                best_hint = potential_hint;
            }
        }
    }

    if (best_hint) {
        hint = best_hint;
        goto check_with_hint;
    }

    return (0);
}



void rm_vm_entry(void *base) {
    struct vm_entry *vmem = (struct vm_entry *)bsearch_s(base, vm_map, vm_map_size, sizeof(struct vm_entry), vm_entry_cmp, (void *)0);
    uint32_t index = (vm_map - vmem) / sizeof(struct vm_entry);
    if (vmem == (void *)0) {
        return;
    }

    if (vmem->base != base) {
        return;
    }

    //for every page, check physical address and mark as free
    for(virtaddr_t addr = vmem->base; addr < vmem->base + vmem->size; addr += PAGE_SIZE) {
        physaddr_t phys = get_physaddr(addr);
        if (phys != 0) {
            bitmap_find_free_page(phys);
        }
    }

    //nuke it from orbit
    if (vmem->base == 0) {
        vmem->size = 0;
    } else {
        memcpy(&vm_map[index], &vm_map[index + 1], sizeof(struct vm_entry) * (1023 - index));
        vm_map_size--;
    }
}

static void page_fault_interrupt_handler(unsigned int interrupt __attribute__((unused)), void *ext __attribute__((unused))) {
    virtaddr_t faulty_address;
    struct vm_entry *vmem;
    uint8_t flags;

    asm volatile("mov %%cr2, %0" : "=r"(faulty_address));

    if (get_physaddr(faulty_address) != 0) {
        //that's a perm issue, burn it with fire
        goto page_fault;
    }

    vmem = (struct vm_entry *)bsearch_s(faulty_address, vm_map, vm_map_size, sizeof(struct vm_entry), vm_entry_cmp, (void *)0);
    if (vmem == (void *)0) {
        //shit hit the fan hard
        goto page_fault;
    }

    if (faulty_address < vmem->base || faulty_address >= vmem->base + vmem->size) {
        //this is not the droid you are looking for
        kprintf("base: 0x%8h; size: %d;\n",  vmem->base, vmem->size);
page_fault:
        kprintf("PAGE FAULT at 0x%8h\n", faulty_address);
        //dump_vm_map();
        asm volatile ("hlt");
        return;
    }

    physaddr_t physaddr = bitmap_find_free_page();
    if (physaddr == 0) {
        //We are out of memory, so maybe try to clean stuff and retry
        kprintf("Out Of Memory\n");
        asm volatile ("hlt");
        return;
    }

    flags = VM_PAGE_READ_WRITE;
    if (vmem->flags & VM_MAP_WRITE) {
        flags |= VM_PAGE_READ_WRITE;
    }
    if (vmem->flags & VM_MAP_USER) {
        flags |= VM_PAGE_USER_ACCESS;
    }

    bitmap_mark_as_used(physaddr);
    if (map_page(physaddr, (virtaddr_t)((uint32_t)faulty_address & ~FIRST_12BITS_MASK), flags | VM_PAGE_READ_WRITE) != 0) {
        //Something went very wrong
        kprintf("PANIC at 0x%8h\n", faulty_address);
        dump_vm_map();
        asm volatile ("hlt");
        return;
    }

    memset((virtaddr_t)((uint32_t)faulty_address & ~FIRST_12BITS_MASK), 0, PAGE_SIZE);
    if (vmem->flags & VM_MAP_FILE && ((faulty_address - vmem->base) & ~FIRST_12BITS_MASK) < vmem->disksize) {
        if (fat_seek(vmem->file, vmem->offset + (faulty_address - vmem->base) & ~FIRST_12BITS_MASK, SEEK_SET) != 0) {
            kprintf("fat_seek error\n");
        }
        fat_read(vmem->file, (virtaddr_t)((uint32_t)faulty_address & ~FIRST_12BITS_MASK), min(PAGE_SIZE, vmem->disksize - ((faulty_address - vmem->base) & ~FIRST_12BITS_MASK)));
    }

    map_change_permission(faulty_address, flags);
}

#define MAX_LINE_DUMP 20

void dump_vm_map() {
    kprintf("\n=== VM DUMP ===\n");
    for (int i = 0; i < MAX_LINE_DUMP; i++) {
        kprintf("%5d: 0x%8h (%1d)\n", i, vm_map[i].base, vm_map[i].size);
    }
}

int __check_ptr_userspace(const void *ptr, uint32_t len) {
    for (const void *page = ptr; page < ptr + len; page += PAGE_SIZE) {
        if ((get_flags(page) & VM_PAGE_USER_ACCESS) == 0) {
            return 1;
        }
    }

    return 0;
}

int __check_ptr(const void *ptr, uint32_t len) {
    for (const void *page = ptr; page < ptr + len; page += PAGE_SIZE) {
        if ((get_flags(page) & VM_PAGE_PRESENT) == 0) {
            return 1;
        }
    }
    
    return 0;
}

int __check_ptr_write(const void *ptr, uint32_t len) {
    for (const void *page = ptr; page < ptr + len; page += PAGE_SIZE) {
        if ((get_flags(page) & (VM_PAGE_PRESENT | VM_PAGE_READ_WRITE)) != (VM_PAGE_PRESENT | VM_PAGE_READ_WRITE)) {
            return 1;
        }
    }
    
    return 0;
}