#include "multiboot.h"
#include "io.h"

#define ROW 25
#define COL 80
#define PAGE_SIZE 4096

#define FIRST_12BITS_MASK 0xFFF
#define PAGE_LEN 1024

#define VM_PT_MOUNT_BASE (virtaddr_t)0xB0000000
#define VM_PDINDEX_SHIFT 22
#define VM_PTINDEX_SHIFT 12
#define VM_PDINDEX_TO_PTR(index) ((unsigned int *)((uint32_t)VM_PT_MOUNT_BASE | ((index) << VM_PTINDEX_SHIFT)))
#define VM_VITRADDR_TO_PDINDEX(virtaddr) ((uint32_t)(virtaddr) >> VM_PDINDEX_SHIFT)
#define VM_VITRADDR_TO_PTINDEX(virtaddr) ((uint32_t)(virtaddr) >> VM_PTINDEX_SHIFT & 0x03FF)
#define VM_INDEXES_TO_PTR(pdindex, ptindex) (void *)(((pdindex) << VM_PDINDEX_SHIFT) | ((ptindex) << VM_PTINDEX_SHIFT))
#define VM_PAGE_PRESENT 0x1
#define VM_PAGE_READ_WRITE 0x2
#define VM_PAGE_USER_ACCESS 0x4

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef void* virtaddr_t;
typedef uint32_t physaddr_t;

unsigned char bitmap[PAGE_SIZE];
void bitmap_mark_as_used(physaddr_t page);
void bitmap_mark_as_free(physaddr_t page);
int bitmap_page_status(physaddr_t page);
physaddr_t bitmap_find_free_page();
void dump_bitmap();
physaddr_t get_physaddr(virtaddr_t virtaddr);

void sleep(unsigned int t);

extern void PAGE_DIRECTORY(void);
extern void PAGE_TABLE(void);
unsigned int * kpage_directory = (unsigned int *)&PAGE_DIRECTORY;


extern void setup_gdt(void);
extern void setup_idt(void);
void vmm_init(void);

#define BIOS_VIDEO_PTR 0x000b8000
#define KERNAL_MAP_BASE 0xc0000000

char *vidptr = (char*)(BIOS_VIDEO_PTR | KERNAL_MAP_BASE);
unsigned int xpos = 0;
unsigned int ypos = 0;

void kprintf(const char *format, ...);
void itoa(char *buf, unsigned int c, unsigned int base);
int put(char c);
void *memcpy(void *dst, const void *src, unsigned long size);
void *memset(void* dst, int c, unsigned long size);

void set_kbrk(void *addr);
void alloc_kbrk(void *min, void *max);
void free_kbrk(void *min, void *max);
void *get_kbrk();

void prepare_switch_to_usermode(void);
extern void switch_to_user_mode(void);

extern void pci_scan_bus(uint8_t bus);

#define CHECK_FLAG(flags,bit)   ((flags) & (1 << (bit)))

void kmain(unsigned long magic, unsigned long addr) {
    memset(vidptr, 0, ROW * COL * 2);
    memset(bitmap, 0, sizeof(bitmap));
    kprintf("coucou\n");
    
    setup_gdt();
    setup_idt();

    if (magic != MULTIBOOT_BOOTLOADER_MAGIC) {
        kprintf("ERROR: magic number is not correct\n");
        return;
    }

    multiboot_info_t *mbi = (multiboot_info_t *)addr;

    kprintf(" flags: 0x%h\n", mbi->flags);

    if (CHECK_FLAG(mbi->flags, 0) != 0) {
        kprintf(" mem_lower: %dKB\n mem_upper: %dKB\n", mbi->mem_lower, mbi->mem_upper);
    }

    if (CHECK_FLAG (mbi->flags, 6))
    {
        multiboot_memory_map_t *mmap;

        kprintf("mmap_addr = 0x%h, mmap_length = 0x%h\n",
                (unsigned) mbi->mmap_addr, (unsigned) mbi->mmap_length);
        for (mmap = (multiboot_memory_map_t *) mbi->mmap_addr;
                (unsigned long) mmap < mbi->mmap_addr + mbi->mmap_length;
                mmap = (multiboot_memory_map_t *) ((unsigned long) mmap
                    + mmap->size + sizeof (mmap->size))) {
            kprintf(" size = 0x%h, base_addr = 0x%h%8h,"
                    " length = 0x%h%8h, type = 0x%8h\n",
                    (unsigned) mmap->size,
                    (unsigned) (mmap->addr >> 32),
                    (unsigned) (mmap->addr & ~0),
                    (unsigned) (mmap->len >> 32),
                    (unsigned) (mmap->len & ~0),
                    (unsigned) mmap->type);
            if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
                for (unsigned int page = mmap->addr; page < mmap->addr + mmap->len; page += PAGE_SIZE) {
                    bitmap_mark_as_free(page);
                }
            }
        }
    }

    unsigned int *kpage_table_init = (unsigned int *)&PAGE_TABLE;
    flush_tlb_single(0x9000);
    kpage_table_init[VM_VITRADDR_TO_PTINDEX(mbi)] = 0; //we dont need mbi anymore
    flush_tlb_single(0);
    kpage_directory[0] = 0; //we don't need identity mapping anymore

    //lets walk the initial page table for finding physical page in use
    for (unsigned int i = 0; i < PAGE_LEN; i++) {
        if ((kpage_table_init[i] & 0b00000001) != 0) {
            kprintf("addrpgy: 0x%8h\n", kpage_table_init[i] & ~FIRST_12BITS_MASK);
            bitmap_mark_as_used(kpage_table_init[i] & ~FIRST_12BITS_MASK);
        }
    }
    bitmap_mark_as_used(0); //damn BUUUGGG
    asm volatile("sti");

    pci_scan_bus(0);

    vmm_init();

    prepare_switch_to_usermode();
}

#define HEX_BASE 16
#define DEC_BASE 10
#define KPRINTF_BUF_SIZE 30

void kprintf(const char *format, ...) {
    char **args = (char **) &format;
    char c;
    char *p;
    char buf[KPRINTF_BUF_SIZE];

    args++;

    while((c = *format++) != '\0') {
        if (c != '%') {
            put(c);
        } else {
            c = *format++;
            if (c == '\0') {
                return;
            }

            memset(buf, '\0', KPRINTF_BUF_SIZE);
            if (c >= '0' && c <= '9') {
                memset(buf, '0', c - '0');
                c = *format++;
            }

            switch (c) {
                case 'd':
                    itoa(buf, *((unsigned int *)args++), DEC_BASE);
                    p = buf;
                    break;

                case 'h':
                    itoa(buf, *((unsigned int *)args++), HEX_BASE);
                    p = buf;
                    break;

                case 's':
                    p = *args++;
                    break;

                default:
                    buf[0] = '\0';
                    p = buf;
                    break;
            }

            while (*p != '\0') {
                put(*p++);
            }
        }
    }
}


void itoa(char *buf, unsigned int c, unsigned int base) {
    char *p;
    char *p1;
    char s;
    const char *charset = "0123456789abcdef";

    p = buf;
    while (c != 0) {
        *p++ = charset[c % base];
        c /= base;
    }

    p = buf;
    while (*p != '\0') {
        p++;
    }
    p--;

    p1 = buf;
    while (p1 < p) {
        s = *p1;
        *p1 = *p;
        *p = s;
        --p;
        ++p1;
    }
}

#define WHITE_ON_BLACK 0x07

int put(char c) {
    if (c == '\n') {
        ++ypos;
    }

    if (c == '\n' || c == '\r') {
        xpos = 0;
        return (0);
    }

    if (xpos > COL - 1) {
        ++ypos;
        xpos = 0;
    }

    if (ypos > ROW - 1) {
        memcpy(vidptr , vidptr + COL * 2, (ROW - 1) * COL * 2);
        unsigned int j = (ROW - 1) * COL * 2;
        while (j < COL * ROW * 2) {
            vidptr[j++] = ' ';
            vidptr[j++] = WHITE_ON_BLACK;
        }
        --ypos;
    }

    unsigned int i = (ypos * COL + xpos) * 2;
    vidptr[i++] = c;
    vidptr[i] = WHITE_ON_BLACK;
    ++xpos;

    return (0);
}


void *memcpy(void *dst, const void *src, unsigned long size) {
    char *dp = (char *)dst;
    const char *sp = (const char *)src;
    while (size--) {
        *dp++ = *sp++;
    }
    return (dst);
}


void *memset(void* dst, int c, unsigned long size) {
    unsigned char *p = (unsigned char *)dst;
    while (size--) {
        *p++ = (unsigned char)c;
    }
    return (dst);
}

// in the bitmap:
//  1 in page free
//  0 in page used

#define BITS_IN_BYTE 8

void bitmap_mark_as_used(physaddr_t page) {
    page /= PAGE_SIZE;
    unsigned int a = page / BITS_IN_BYTE;
    int mask = ~(1 << (page % BITS_IN_BYTE));

    bitmap[a] &= mask; 
}

void bitmap_mark_as_free(physaddr_t page) {
    page /= PAGE_SIZE;
    unsigned int a = page / BITS_IN_BYTE;
    int mask = 1 << (page % BITS_IN_BYTE);

    bitmap[a] |= mask; 
}

int bitmap_page_status(physaddr_t page) {
    page /= PAGE_SIZE;
    unsigned int a = page / BITS_IN_BYTE;
    int mask = 1 << (page % BITS_IN_BYTE);

    return (bitmap[a] & mask) == 0; 
}

physaddr_t bitmap_find_free_page() {
    int a;
    int b;
    
    for (a = 0; a < PAGE_SIZE; a++) {
        for (b = 0; b < BITS_IN_BYTE; b++) {
            if ((bitmap[a] & (1 << b)) != 0) {
                return (a * BITS_IN_BYTE + b) * PAGE_SIZE;    
            }
        }
    }

    return 0;
}

void dump_bitmap() {
    for (unsigned int a = 0; a < PAGE_LEN / 2; a += 4) {
        kprintf("0x%8h 0x%8h 0x%8h 0x%8h\n", 
                ((unsigned int *)bitmap)[a], 
                ((unsigned int *)bitmap)[a+1],
                ((unsigned int *)bitmap)[a+2], 
                ((unsigned int *)bitmap)[a+3]);
    }
}



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

void alloc_kbrk(void *min, void *max) {
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

void free_kbrk(void *min, void *max) {
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

extern void update_kernel_stack(void *stack);

#define USER_KERN_STACK (virtaddr_t)0xc004ffff
#define USER_STACk (virtaddr_t)0xbfffffff

void prepare_switch_to_usermode() {
    unsigned int *user_mode_upper_limit = get_kbrk();
    unsigned int *user_page_directory = GET_BEGINGIN_PREV_PAGE(user_mode_upper_limit);
    unsigned int *user_kernel_table_directory = GET_BEGINGIN_PREV_PAGE(user_page_directory);
    unsigned int *user_stack_table_directory = GET_BEGINGIN_PREV_PAGE(user_kernel_table_directory);
    unsigned int *user_stack_limit =  GET_BEGINGIN_PREV_PAGE(user_stack_table_directory);
    unsigned int *kernel_stack_limit =  GET_BEGINGIN_PREV_PAGE(user_stack_limit);
    set_kbrk(kernel_stack_limit);

    kprintf("user_mode_upper_limit: 0x%8h; user_page_directory: 0x%8h; user_stack_table_directory: 0x%8h; user_stack_limit: 0x%8h\n",
        user_mode_upper_limit, user_page_directory, user_stack_table_directory, user_stack_limit);

    memset(user_page_directory, 0, PAGE_SIZE);
    user_page_directory[VM_VITRADDR_TO_PDINDEX(KERNAL_MAP_BASE)] = (get_physaddr(user_kernel_table_directory) & ~FIRST_12BITS_MASK) | VM_PAGE_USER_ACCESS | VM_PAGE_READ_WRITE | VM_PAGE_PRESENT; //kernel mapping
    user_page_directory[VM_VITRADDR_TO_PDINDEX(KERNAL_MAP_BASE) - 1] = (get_physaddr(user_stack_table_directory) & ~FIRST_12BITS_MASK) | VM_PAGE_USER_ACCESS | VM_PAGE_READ_WRITE | VM_PAGE_PRESENT; //user stack

    memset(user_kernel_table_directory, 0, PAGE_SIZE);
    /*
     * having to map kernel code is terrible but as for now it's a good PoC
     * i guess in the feature i should load a elf and point directly at start entry
     */
    unsigned int *kpage_table_init = (unsigned int *)&PAGE_TABLE;
    for (int i = 0; i < PAGE_LEN; i++) {
        if ((kpage_table_init[i] & VM_PAGE_PRESENT) == 0) {
            continue;
        }

        if ((kpage_table_init[i] & VM_PAGE_READ_WRITE) != 0) {
            user_kernel_table_directory[i] = kpage_table_init[i]; //do not give access to kernel stack to userspace
        } else {
            user_kernel_table_directory[i] = kpage_table_init[i] | VM_PAGE_USER_ACCESS; //map kernel for userspace
        }
    }

    user_kernel_table_directory[VM_VITRADDR_TO_PTINDEX(USER_KERN_STACK)] = (get_physaddr(kernel_stack_limit) & ~FIRST_12BITS_MASK) | VM_PAGE_READ_WRITE | VM_PAGE_PRESENT; //kernel stack
    update_kernel_stack(USER_KERN_STACK); /* OK, so i need a valid stack for handling interupt, which is fine, but i still need to go back to
                                      * kernel memory mapping to do some stuff.
                                      */

    memset(user_stack_table_directory, 0, PAGE_SIZE);
    user_stack_table_directory[VM_VITRADDR_TO_PTINDEX(USER_STACk)] = (get_physaddr(user_stack_limit) & ~FIRST_12BITS_MASK) | VM_PAGE_USER_ACCESS | VM_PAGE_READ_WRITE | VM_PAGE_PRESENT; //user stack, read-write, user access, present

    asm volatile("mov %0, %%cr3" : : "r"(get_physaddr(user_page_directory)));

    switch_to_user_mode();
}

extern unsigned int tick;
void sleep(unsigned int t) {
    tick = t / 55 + 1;
    while(tick != 0) {
        asm volatile("nop");
    }
}