#include "stdtype.h"
#include "io.h"
#include "vmm.h"
#include "pmm.h"
#include "interrupt.h"
#include "stdlib.h"

#define FIRST_12BITS_MASK 0xFFF


#define KERNAL_MAP_BASE 0xC0000000
#define VM_PT_MOUNT_BASE (virtaddr_t)0xFFC00000
#define VM_PDINDEX_TO_PTR(index) ((unsigned int *)((uint32_t)VM_PT_MOUNT_BASE | ((index) << VM_PTINDEX_SHIFT)))
#define VM_VITRADDR_TO_PDINDEX(virtaddr) ((uint32_t)(virtaddr) >> VM_PDINDEX_SHIFT)
#define VM_VITRADDR_TO_PTINDEX(virtaddr) (((uint32_t)(virtaddr) >> VM_PTINDEX_SHIFT) & 0x03FF)
#define VM_INDEXES_TO_PTR(pdindex, ptindex) (void *)(((pdindex) << VM_PDINDEX_SHIFT) | ((ptindex) << VM_PTINDEX_SHIFT))



struct vm_entry {
    void *base;
    uint32_t size;
    uint32_t flags;
};

struct vm_entry vm_map[1024]; //that should be in task struct
uint32_t vm_map_size = 0;

int vm_entry_cmp(const void  *key, const void *obj, void *ext) {
    struct vm_entry *map_entry = (struct vm_entry *)obj;

    if (key < map_entry->base) {
        return -1;
    } else if (key >= map_entry->base + map_entry->size) {
        return 1;
    } else {
        return 0;
    }
}

extern void PAGE_DIRECTORY(void);
extern void PAGE_TABLE(void);
unsigned int * kpage_directory = (unsigned int *)&PAGE_DIRECTORY;

static void page_fault_interrupt_handler(unsigned int interrupt, void *ext);

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
    unsigned int *pt = VM_PDINDEX_TO_PTR(pdindex);
    unsigned int ptentry = pt[ptindex];
    //kprintf("pt: 0x%8h; ptentry: 0x%8h\n", pt, ptentry);
    if ((ptentry & 0x00000001) == 0) {
        //kprintf("pte not present\n");
        return (0);
    }
    //kprintf("pt: 0x%8h\n", pt);

    return (ptentry & ~FIRST_12BITS_MASK) + ((unsigned int)virtaddr & FIRST_12BITS_MASK);
}

int map_page(physaddr_t physadd, virtaddr_t virtaddr, unsigned int flags) {
    //kprintf("map_page: physadd: 0x%8h; virtaddr: 0x%8h; flags: 0x%8h\n", physadd, virtaddr, flags);

    unsigned int pdindex = VM_VITRADDR_TO_PDINDEX(virtaddr);
    unsigned int ptindex = VM_VITRADDR_TO_PTINDEX(virtaddr);
    unsigned int pdentry = (unsigned int)kpage_directory[pdindex];
    unsigned int *pt = VM_PDINDEX_TO_PTR(pdindex);

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
        map_page(pagetable_physmap, pt, VM_PAGE_READ_WRITE);
        kpage_directory[pdindex] = (pagetable_physmap & ~FIRST_12BITS_MASK) | VM_PAGE_READ_WRITE | (flags & VM_PAGE_USER_ACCESS) | VM_PAGE_PRESENT; //if map if showed with user access flag set, page direcotry should also have it to allow user
        
        //init page
        memset(pt, 0, PAGE_SIZE);
    }

    //kprintf("pt: 0x%8h; ptindex: %1d\n", pt, ptindex);
    if ((pt[ptindex] & 0x00000001) != 0) {
        kprintf("ERROR: pte is present; map not free to use !\n");
        return 2;
    }

    pt[ptindex] = (physadd & ~FIRST_12BITS_MASK) | (flags & FIRST_12BITS_MASK) | VM_PAGE_PRESENT;

    return (0);
}

void unmap_page(virtaddr_t virtaddr) {
    unsigned int pdindex = VM_VITRADDR_TO_PDINDEX(virtaddr);
    unsigned int ptindex = VM_VITRADDR_TO_PTINDEX(virtaddr);
    unsigned int *pt = VM_PDINDEX_TO_PTR(pdindex);

    kprintf("map_page: virtaddr: 0x%8h; pdindex: 0x%8h; ptindex: 0x%8h; pt: 0x%8h\n", virtaddr, pdindex, ptindex, pt);

    pt[ptindex] = 0;

    int i;
    for (i = 0; i < PAGE_LEN; i++) {
        if (pt[i] != 0) {
            break;
        }
    }

    if (i == PAGE_LEN) {
        kpage_directory[pdindex] = 0;
        unmap_page(pt);
        //bitmap_mark_as_free(virtaddr); that is a bug rigth ?
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
    vm_map[1].base = (void *)0xffffffff;
    vm_map_size = 2;
    register_interrupt(0xE, page_fault_interrupt_handler, 0);
}

void *add_vm_entry(void *hint, uint32_t size, uint32_t flags) {
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

    struct vm_entry *lower = (struct vm_entry *)bsearch_sf(hint, vm_map, vm_map_size, sizeof(struct vm_entry), vm_entry_cmp, (void *)0);
    uint32_t index = (vm_map - lower) / sizeof(struct vm_entry);

    if (vm_map_size >= 1024) {
        return (void *)0;
    }

    //find a free and ordered index
find_empty_spot:
    if (index >= vm_map_size) {
        goto tail;
    }

    if (vm_map[index].base + vm_map[index].size > hint) {
        hint = vm_map[index].base + vm_map[index].size;
    }

    if (vm_map[index + 1].base < hint + size) {
        index += 1;
    }

    if (vm_map[index].base + vm_map[index].size > hint
         || vm_map[index + 1].base < hint + size) {
        goto find_empty_spot;
    }

    if (vm_map[index].size != 0) {
        //memmove-like thing
        for (uint32_t i = 1023; i > index; i--) {
            memcpy(&vm_map[i+1], &vm_map[i], sizeof(struct vm_entry));
        }

        index += 1;
    }
tail:
    //add the entry
    vm_map[index].base = hint;
    vm_map[index].size = size;
    vm_map[index].flags = flags;
    vm_map_size++;

    //post-check the "guard zero"
    if (vm_map[0].base != 0) {
        for (int32_t i = 1023; i >= 0; i--) {
            memcpy(&vm_map[i+1], &vm_map[i], sizeof(struct vm_entry));
        }

        memset(&vm_map[0], 0, sizeof(struct vm_entry));
        index += 1;
    }

    dump_vm_map();

    return vm_map[index].base;
}

void rm_vm_entry(void *base) {
    struct vm_entry *vm = (struct vm_entry *)bsearch_sf(base, vm_map, vm_map_size, sizeof(struct vm_entry), vm_entry_cmp, (void *)0);
    uint32_t index = (vm_map - vm) / sizeof(struct vm_entry);

    if (vm->base != base) {
        return;
    }

    //for every page, check physical address and mark as free
    for(virtaddr_t addr = vm->base; addr < vm->base + vm->size; addr += PAGE_SIZE) {
        physaddr_t phys = get_physaddr(addr);
        if (phys != 0) {
            bitmap_find_free_page(phys);
        }
    }

    //nuke it from orbit
    if (vm->base == 0) {
        vm->size = 0;
    } else {
        memcpy(&vm_map[index], &vm_map[index + 1], sizeof(struct vm_entry) * (1023 - index));
        vm_map_size--;
    }
}

static void page_fault_interrupt_handler(unsigned int interrupt, void *ext) {
    virtaddr_t faulty_address;
    struct vm_entry *vm;
    uint8_t flags;

    asm volatile("mov %%cr2, %0" : "=r"(faulty_address));

    vm = (struct vm_entry *)bsearch_sf(faulty_address, vm_map, vm_map_size, sizeof(struct vm_entry), vm_entry_cmp, (void *)0);
    if (vm == (void *)0) {
        //shit hit the fan hard
        goto page_fault;
    }

    if (faulty_address < vm->base || faulty_address >= vm->base + vm->size) {
        //this is not the droid you are looking for
        kprintf("base: 0x%8h; size: %d;\n",  vm->base, vm->size);
page_fault:
        kprintf("PAGE FAULT at 0x%8h\n", faulty_address);
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

    flags = 0;
    if (vm->flags & VM_MAP_WRITE) {
        flags |= VM_PAGE_READ_WRITE;
    }
    if (vm->flags & VM_MAP_USER) {
        flags |= VM_PAGE_USER_ACCESS;
    }

    bitmap_mark_as_used(physaddr);
    if (map_page(physaddr, vm->base, flags) != 0) {
        //Something went very wrong
        kprintf("PANIC at 0x%8h\n", faulty_address);
        asm volatile ("hlt");
        return;
    }
}

void dump_vm_map() {
    kprintf("\n=== VM DUMP ===\n");
    for (int i = 0; i < 20; i++) {
        kprintf("%5d: 0x%8h (%1d)\n", i, vm_map[i].base, vm_map[i].size);
    }
}

