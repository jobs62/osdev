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
unsigned int * kpage_directory = (unsigned int *)&PAGE_DIRECTORY;

extern void setup_gdt(void);
extern void setup_idt(void);

char *vidptr = (char*)0xc00b8000;
unsigned int xpos = 0;
unsigned int ypos = 0;

void kprintf(const char *format, ...);
void itoa(char *buf, unsigned int c, unsigned int base);
int put(char c);
void *memcpy(void *dst, void *src, unsigned long size);
void *memset(void* dst, int c, unsigned long size);

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
    
    //that should unmap the first megabyte ??!
    kpage_directory[0] = 0;

    asm volatile("sti");
   
    dump_bitmap();
    kprintf("still alive\n");
    kprintf("physaddr from 0x%8h: 0x%8h\n", vidptr, get_physaddr(vidptr));

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
    unsigned int *pt = (unsigned int *)((pdentry & ~0xFFF) + 0xC0000000);
    unsigned int ptentry = pt[ptindex];
    if ((ptentry & 0x00000001) == 0) {
        kprintf("pte not present\n");
        return (0);
    }
    kprintf("pt: 0x%8h\n", pt);

    return (ptentry & ~0xFFF) + ((unsigned int)virtualaddr & 0xFFF);
}

void map_page(unsigned int physadd, unsigned int virtaddr, unsigned int flags) {
    unsigned int pdindex = (unsigned int)virtaddr >> 22;
    unsigned int ptindex = (unsigned int)virtaddr >> 12 & 0x03FF;
    
    unsigned int pdentry = (unsigned int)kpage_directory[pdindex];
    if ((pdentry & 0x00000001) == 0) {
        kprintf("pt not present\n");
        //TODO: alloc page, map page and init pd entry

    }

    unsigned int *pt = (unsigned int *)((pdentry & ~0xFFF) + 0xC0000000);
    if ((pt[ptindex] & 0x00000001) != 0) {
        kprintf("ERROR: pte is present; map not free to use !\n");
        return;
    }

    pt[ptindex] = (physadd & ~0xFFF) | (flags & 0xFFF) | 0x01;
}
