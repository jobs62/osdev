#include "multiboot.h"
#include "io.h"

#define ROW 25
#define COL 80

unsigned char bitmap[4096];
void bitmap_mark_as_used(unsigned int page);
void bitmap_mark_as_free(unsigned int page);
int bitmap_page_status(unsigned int page);
unsigned int bitmap_find_free_page();
void dump_bitmap();
unsigned int get_physaddr(unsigned int virtualaddr);

extern void PAGE_DIRECTORY(void);
extern void PAGE_TABLE(void);
unsigned int * kpage_directory = (unsigned int *)&PAGE_DIRECTORY;

extern void setup_gdt(void);
extern void setup_idt(void);
void vmm_init(void);

char *vidptr = (char*)0xc00b8000;
unsigned int xpos = 0;
unsigned int ypos = 0;

void kprintf(const char *format, ...);
void itoa(char *buf, unsigned int c, unsigned int base);
int put(char c);
void *memcpy(void *dst, void *src, unsigned long size);
void *memset(void* dst, int c, unsigned long size);

void set_kbrk(void *addr);
void alloc_kbrk(void *min, void *max);
void free_kbrk(void *min, void *max);
void *get_kbrk();

void prepare_switch_to_usermode(void);
extern void switch_to_user_mode(void);

#define CHECK_FLAG(flags,bit)   ((flags) & (1 << (bit)))

void kmain(unsigned long magic, unsigned long addr) {
    memset(vidptr, 0, ROW * COL * 2);
    memset(bitmap, 0, 4096);
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
                    (unsigned) (mmap->addr & 0xffffffff),
                    (unsigned) (mmap->len >> 32),
                    (unsigned) (mmap->len & 0xffffffff),
                    (unsigned) mmap->type);

                if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
                    for (unsigned int page = mmap->addr; page < mmap->addr + mmap->len; page += 4096) {
                        bitmap_mark_as_free(page / 4096);
                    }
                }
        }
    }

    for (unsigned int i = 0; i < 2 * 256; i++) {
        //that should map the first 2Mb as used
        bitmap_mark_as_used(i);
    }

    asm volatile("sti");
   
    //that should unmap the first megabyte ??!
    kpage_directory[0] = 0;
    
    vmm_init();

    prepare_switch_to_usermode();

    return;
}

void kprintf(const char *format, ...) {
    char **args = (char **) &format;
    int c;
    char *p;
    char buf[30];

    args++;

    while((c = *format++) != '\0') {
        if (c != '%') {
            put(c);
        } else {
            c = *format++;
            if (c == '\0') {
                return;
            }

            memset(buf, '\0', 30);
            if (c >= '0' && c <= '9') {
                memset(buf, '0', c - '0');
                c = *format++;
            }

            switch (c) {
                case 'd':
                    itoa(buf, *((unsigned int *)args++), 10);
                    p = buf;
                    break;

                case 'h':
                    itoa(buf, *((unsigned int *)args++), 16);
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
    char *p, *p1;
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
            vidptr[j++] = 0x07;
        }
        --ypos;
    }

    unsigned int i = (ypos * COL + xpos) * 2;
    vidptr[i++] = c;
    vidptr[i] = 0x07;
    ++xpos;

    return (0);
}


void *memcpy(void *dst, void *src, unsigned long size) {
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

void bitmap_mark_as_used(unsigned int page) {
    int a = page / 8;
    int mask = ~(1 << (page % 8));

    bitmap[a] &= mask; 
}

void bitmap_mark_as_free(unsigned int page) {
    int a = page / 8;
    int mask = 1 << (page % 8);

    bitmap[a] |= mask; 
}

int bitmap_page_status(unsigned int page) {
    int a = page / 8;
    int mask = 1 << (page % 8);

    return (bitmap[a] & mask) == 0; 
}

unsigned int bitmap_find_free_page() {
    int a, b;
    
    for (a = 0; a < 4096; a++) {
        for (b = 0; b < 8; b++) {
            if ((bitmap[a] & (1 << b)) != 0) {
                return a * 8 + b;    
            }
        } 
    }

    return 0;
}

void dump_bitmap() {
    for (unsigned int a = 0; a < 512; a += 4) {
        kprintf("0x%8h 0x%8h 0x%8h 0x%8h\n", 
                ((unsigned int *)bitmap)[a], 
                ((unsigned int *)bitmap)[a+1],
                ((unsigned int *)bitmap)[a+2], 
                ((unsigned int *)bitmap)[a+3]);
    }
}

unsigned int get_physaddr(unsigned int virtualaddr) {
    unsigned int pdindex = (unsigned int)virtualaddr >> 22;
    unsigned int ptindex = (unsigned int)virtualaddr >> 12 & 0x03FF;
    kprintf("pdindex: 0x%8h; ptindex: 0x%8h\n", pdindex, ptindex);

    unsigned int pdentry = (unsigned int)kpage_directory[pdindex];
    if ((pdentry & 0x00000001) == 0) {
        kprintf("pt not present\n");
        return (0);
    }
   
    //So i guess i have to map every pt to a specific virtual location to be able to find them
    unsigned int *pt = (unsigned int *)(0xB0000000 | (pdindex << 12));
    unsigned int ptentry = pt[ptindex];
    if ((ptentry & 0x00000001) == 0) {
        kprintf("pte not present\n");
        return (0);
    }
    kprintf("pt: 0x%8h\n", pt);

    return (ptentry & ~0xFFF) + ((unsigned int)virtualaddr & 0xFFF);
}

void map_page(unsigned int physadd, unsigned int virtaddr, unsigned int flags) {
    //kprintf("map_page: physadd: 0x%8h; virtaddr: 0x%8h; flags: 0x%8h\n", physadd, virtaddr, flags);

    unsigned int pdindex = (unsigned int)virtaddr >> 22;
    unsigned int ptindex = (unsigned int)virtaddr >> 12 & 0x03FF;
    unsigned int pdentry = (unsigned int)kpage_directory[pdindex];
    unsigned int *pt = (unsigned int *)(0xB0000000 | (pdindex << 12));

    if ((pdentry & 0x00000001) == 0) {
        kprintf("pt not present (0x%8h)\n", pt);
        
        //alloc page
        unsigned int pagetable_physmap = bitmap_find_free_page() * 4096;
        if (pagetable_physmap == 0) {
            kprintf("ERROR: could not get page\n");
            return;
        }

        bitmap_mark_as_used(pagetable_physmap / 4096);

        //map page
        map_page(pagetable_physmap, (unsigned int)pt, 0x02);
        kpage_directory[pdindex] = (pagetable_physmap & ~0xFFF) | 0x03;
        
        //init page
        memset(pt, 0, 1024);
    }

    if ((pt[ptindex] & 0x00000001) != 0) {
        kprintf("ERROR: pte is present; map not free to use !\n");
        return;
    }

    pt[ptindex] = (physadd & ~0xFFF) | (flags & 0xFFF) | 0x01;
}

void unmap_page(unsigned int virtaddr) {
    unsigned int pdindex = (unsigned int)virtaddr >> 22;
    unsigned int ptindex = (unsigned int)virtaddr >> 12 & 0x03FF;
    unsigned int pdentry = (unsigned int)kpage_directory[pdindex];
    unsigned int *pt = (unsigned int *)(0xB0000000 | (pdindex << 12));

    kprintf("map_page: virtaddr: 0x%8h; pdindex: 0x%8h; ptindex: 0x%8h; pt: 0x%8h\n", virtaddr, pdindex, ptindex, pt);

    //asm volatile("hlt");

    pt[ptindex] = 0;

    int i;
    for (i = 0; i < 1024; i++) {
        if (pt[i] != 0) {
            break;
        }
    }

    if (i == 1024) {
        kpage_directory[pdindex] = 0;
        unmap_page((unsigned int)pt);
        bitmap_mark_as_free(virtaddr / 4096);
        
    }
}

void vmm_init() {
    unsigned int *vmm_base = 0xB0000000;
    unsigned int tmp;

    unsigned int pagetable_physmap = bitmap_find_free_page() * 4096;
    if (pagetable_physmap == 0) {
        kprintf("ERROR: could not get page\n");
        return;
    }

    bitmap_mark_as_used(pagetable_physmap / 4096);
    kpage_directory[704] = (pagetable_physmap & ~0xFFF) | 0x03; //present and read-write
    
    //use the last page of the bootloaded page table to bootstrap the maps' map
    tmp = ((unsigned int *)&PAGE_TABLE)[1023];
    ((unsigned int *)&PAGE_TABLE)[1023] = (pagetable_physmap & ~0xFFF) | 0x03;
    vmm_base = (unsigned int *)0xC03ff000;
    memset(vmm_base, 0, 1024);
    vmm_base[704] = (pagetable_physmap & ~0xFFF) | 0x03; //present and read-write
    vmm_base = (unsigned int *)0xB0000000;
    //((unsigned int *)&PAGE_TABLE)[1023] = 0; /* if uncommentaed, it crash everithing ?? need to be investigated */

    vmm_base[704 * 1024 + 768] = ((unsigned int)(&PAGE_TABLE) & ~0xFFF) | 0x03; //present and read-write
}

void *get_kbrk() {
    int pdindex, ptindex;

    for (pdindex = 1023; pdindex > 0; pdindex--) {
        ptindex = 1023;
        if ((kpage_directory[pdindex] & 0x00000001) == 0) {
            goto get_brk_endloop;
        }
        
        unsigned int *pt = (unsigned int *)(0xB0000000 | (pdindex << 12));
        for (; ptindex > 0; ptindex--) {
            if ((pt[ptindex] & 0x00000001) == 0) {
                goto get_brk_endloop;
            }
        }
    }
get_brk_endloop:

    if (pdindex == 1023 && ptindex == 1023) {
        return (void *)0xffffffff;
    }

    return (void *)((pdindex << 22) | (ptindex << 12) | 0x000); //0xFFF to get the end of the page
}

void alloc_kbrk(void *min, void *max) {
    if (max == 0xffffffff) {
        //carful with overflow at first alloc
        unsigned int pagetable_physmap = bitmap_find_free_page() * 4096;
        if (pagetable_physmap == 0) {
            kprintf("ERROR: could not get page\n");
            return;
        }

        bitmap_mark_as_used(pagetable_physmap / 4096);

        map_page(pagetable_physmap, 0xfffff000, 0x02);

        max = 0xfffff000;
    }

    for (; min < max; min += 4096) {
        unsigned int pagetable_physmap = bitmap_find_free_page() * 4096;
        if (pagetable_physmap == 0) {
            kprintf("ERROR: could not get page\n");
            return;
        }

        bitmap_mark_as_used(pagetable_physmap / 4096);

        map_page(pagetable_physmap, (unsigned int)min, 0x02);
    }
}

#define GET_BEGINGIN_PREV_PAGE(page) ((((unsigned int)page >> 12) - 1) << 12)

void free_kbrk(void *min, void *max) {
    for (; min < max; min = (void *)GET_BEGINGIN_PREV_PAGE(min)) {
        unsigned int pagetable_physmap = get_physaddr((unsigned int)min);
        unmap_page((unsigned int)min);
        bitmap_mark_as_free(pagetable_physmap / 4096);
    }
}

void set_kbrk(void *addr) {
    void *min, *max;

    min = addr;
    max = get_kbrk();
    if (min > max) {
        free_kbrk(max, min);
    } else {
        alloc_kbrk(min, max);
    }
}



void prepare_switch_to_usermode() {
    unsigned int user_mode_upper_limit = get_kbrk();
    unsigned int *user_page_directory = (unsigned int *)GET_BEGINGIN_PREV_PAGE(user_mode_upper_limit);
    unsigned int *user_kernel_table_directory = (unsigned int *)GET_BEGINGIN_PREV_PAGE(user_page_directory);
    unsigned int *user_stack_table_directory = (unsigned int *)GET_BEGINGIN_PREV_PAGE(user_kernel_table_directory);
    unsigned int *user_stack_limit =  (unsigned int *)GET_BEGINGIN_PREV_PAGE(user_stack_table_directory);
    set_kbrk(user_stack_limit);

    kprintf("user_mode_upper_limit: 0x%8h; user_page_directory: 0x%8h; user_stack_table_directory: 0x%8h; user_stack_limit: 0x%8h\n",
        user_mode_upper_limit, user_page_directory, user_stack_table_directory, user_stack_limit);

    memset(user_page_directory, 0, 4096);
    user_page_directory[768] = (get_physaddr(user_kernel_table_directory) & ~0xFFF) | 0x05; //kernel maping, read-only, user accesible, present
    user_page_directory[767] = (get_physaddr(user_stack_table_directory) & ~0xFFF) | 0x07; //user stack, read-write, user access, present

    memset(user_kernel_table_directory, 0, 4096);
    for (int i = 0; i < 512; i++) {
        user_kernel_table_directory[i] = (i << 12) | 0x05;
    }

    memset(user_stack_table_directory, 0, 4096);
    user_stack_table_directory[1023] = (get_physaddr((unsigned int)user_stack_limit) & ~0xFFF) | 0x07; //user stack, read-write, user access, present

    asm volatile("mov %0, %%cr3" : : "r"(get_physaddr((unsigned int)user_page_directory)));

    switch_to_user_mode();
}