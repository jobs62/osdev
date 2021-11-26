#include "stdtype.h"
#include "io.h"
#include "vmm.h"
#include "pmm.h"

#define FIRST_12BITS_MASK 0xFFF


#define KERNAL_MAP_BASE 0xc0000000

#define VM_PT_MOUNT_BASE (virtaddr_t)0xB0000000
#define VM_PDINDEX_TO_PTR(index) ((unsigned int *)((uint32_t)VM_PT_MOUNT_BASE | ((index) << VM_PTINDEX_SHIFT)))
#define VM_VITRADDR_TO_PDINDEX(virtaddr) ((uint32_t)(virtaddr) >> VM_PDINDEX_SHIFT)
#define VM_VITRADDR_TO_PTINDEX(virtaddr) ((uint32_t)(virtaddr) >> VM_PTINDEX_SHIFT & 0x03FF)
#define VM_INDEXES_TO_PTR(pdindex, ptindex) (void *)(((pdindex) << VM_PDINDEX_SHIFT) | ((ptindex) << VM_PTINDEX_SHIFT))
#define VM_PAGE_PRESENT 0x1
#define VM_PAGE_READ_WRITE 0x2
#define VM_PAGE_USER_ACCESS 0x4

extern void PAGE_DIRECTORY(void);
extern void PAGE_TABLE(void);
unsigned int * kpage_directory = (unsigned int *)&PAGE_DIRECTORY;

static void alloc_kbrk(void *min, void *max);
static void free_kbrk(void *min, void *max);

physaddr_t get_physaddr(virtaddr_t virtaddr) {
    unsigned int pdindex = VM_VITRADDR_TO_PDINDEX(virtaddr);
    unsigned int ptindex = VM_VITRADDR_TO_PTINDEX(virtaddr);

    unsigned int pdentry = (unsigned int)kpage_directory[pdindex];
    if ((pdentry & 0x00000001) == 0) {
        kprintf("pt not present\n");
        return (0);
    }
   
    //So i guess i have to map every pt to a specific virtual location to be able to find them
    unsigned int *pt = VM_PDINDEX_TO_PTR(pdindex);
    unsigned int ptentry = pt[ptindex];
    if ((ptentry & 0x00000001) == 0) {
        kprintf("pte not present\n");
        return (0);
    }
    //kprintf("pt: 0x%8h\n", pt);

    return (ptentry & ~FIRST_12BITS_MASK) + ((unsigned int)virtaddr & FIRST_12BITS_MASK);
}

void map_page(physaddr_t physadd, virtaddr_t virtaddr, unsigned int flags) {
    //kprintf("map_page: physadd: 0x%8h; virtaddr: 0x%8h; flags: 0x%8h\n", physadd, virtaddr, flags);

    unsigned int pdindex = VM_VITRADDR_TO_PDINDEX(virtaddr);
    unsigned int ptindex = VM_VITRADDR_TO_PTINDEX(virtaddr);
    unsigned int pdentry = (unsigned int)kpage_directory[pdindex];
    unsigned int *pt = VM_PDINDEX_TO_PTR(pdindex);

    if ((pdentry & 0x00000001) == 0) {
        kprintf("pt not present (0x%8h)\n", pt);
        
        //alloc page
        physaddr_t pagetable_physmap = bitmap_find_free_page();
        kprintf("pa: 0x%8h\n", pagetable_physmap);
        if (pagetable_physmap == 0) {
            kprintf("ERROR: could not get page\n");
            return;
        }

        bitmap_mark_as_used(pagetable_physmap);

        //map page
        map_page(pagetable_physmap, pt, VM_PAGE_READ_WRITE);
        kpage_directory[pdindex] = (pagetable_physmap & ~FIRST_12BITS_MASK) | VM_PAGE_READ_WRITE | VM_PAGE_PRESENT;
        
        //init page
        memset(pt, 0, PAGE_SIZE);
    }

    if ((pt[ptindex] & 0x00000001) != 0) {
        kprintf("ERROR: pte is present; map not free to use !\n");
        return;
    }

    pt[ptindex] = (physadd & ~FIRST_12BITS_MASK) | (flags & FIRST_12BITS_MASK) | VM_PAGE_PRESENT;
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

    flush_tlb_single(pagetable_physmap);
    ((unsigned int *)&PAGE_TABLE)[PAGE_LEN - 1] = (pagetable_physmap & ~FIRST_12BITS_MASK) | VM_PAGE_READ_WRITE;

    vmm_base[VM_VITRADDR_TO_PDINDEX(VM_PT_MOUNT_BASE) * PAGE_LEN + VM_VITRADDR_TO_PDINDEX(KERNAL_MAP_BASE)] = ((unsigned int)(&PAGE_TABLE) & ~FIRST_12BITS_MASK) | VM_PAGE_READ_WRITE | VM_PAGE_PRESENT;
}

void *get_kbrk() {
    int pdindex;
    int ptindex;

    for (pdindex = PAGE_LEN - 1; pdindex > 0; pdindex--) {
        ptindex = PAGE_LEN - 1;
        if ((kpage_directory[pdindex] & 0x00000001) == 0) {
            goto get_brk_endloop;
        }
        
        unsigned int *pt = (unsigned int *)VM_PDINDEX_TO_PTR(ptindex);
        for (; ptindex > 0; ptindex--) {
            if ((pt[ptindex] & 0x00000001) == 0) {
                goto get_brk_endloop;
            }
        }
    }
get_brk_endloop:

    if (pdindex == PAGE_LEN - 1  && ptindex == PAGE_LEN - 1) {
        return (void *)~0;
    }

    return VM_INDEXES_TO_PTR(pdindex, ptindex);
}

static void alloc_kbrk(void *min, void *max) {
    if (max == (void *)~0) {
        //carful with overflow at first alloc
        unsigned int pagetable_physmap = bitmap_find_free_page();
        if (pagetable_physmap == 0) {
            kprintf("ERROR: could not get page\n");
            return;
        }

        bitmap_mark_as_used(pagetable_physmap);

        map_page(pagetable_physmap, (void *)(~0 & ~FIRST_12BITS_MASK), VM_PAGE_READ_WRITE);

        max = (void*)(~0 & ~FIRST_12BITS_MASK);
    }

    for (; min < max; min += PAGE_SIZE) {
        unsigned int pagetable_physmap = bitmap_find_free_page();
        if (pagetable_physmap == 0) {
            kprintf("ERROR: could not get page\n");
            return;
        }

        bitmap_mark_as_used(pagetable_physmap);

        map_page(pagetable_physmap, min, 0x02);
    }
}

#define GET_BEGINGIN_PREV_PAGE(page) ((unsigned int *)((((unsigned int)(page) >> VM_PTINDEX_SHIFT) - 1) << VM_PTINDEX_SHIFT))

static void free_kbrk(void *min, void *max) {
    for (; min < max; min = (void *)GET_BEGINGIN_PREV_PAGE(min)) {
        unsigned int pagetable_physmap = get_physaddr(min);
        unmap_page(min);
        bitmap_mark_as_free(pagetable_physmap);
    }
}

void set_kbrk(void *addr) {
    void *a;
    void *b;

    a = addr;
    b = get_kbrk();
    if (a > b) {
        free_kbrk(b, a);
    } else {
        alloc_kbrk(a, b);
    }
}