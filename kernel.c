#include "ata.h"
#include "multiboot.h"
#include "io.h"
#include "stdtype.h"
#include "fat.h"
#include "vmm.h"
#include "pmm.h"
#include "stdlib.h"
#include "liballoc.h"

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

extern void PAGE_TABLE(void);
extern unsigned int * kpage_directory;

void sleep(unsigned int t);

extern void setup_gdt(void);
extern void setup_idt(void);

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

void prepare_switch_to_usermode(void);
extern void switch_to_user_mode(void);

#define CHECK_FLAG(flags,bit)   ((flags) & (1 << (bit)))

extern struct fat_fs fs;

void kmain(unsigned long magic, unsigned long addr) {
    memset(vidptr, 0, ROW * COL * 2);
    bitmap_clear();
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
            bitmap_mark_as_used(kpage_table_init[i] & ~FIRST_12BITS_MASK);
        }
    }
    bitmap_mark_as_used(0); //damn BUUUGGG

    vmm_init();

    asm volatile("sti");

    pci_scan_bus(0);

    /*
    struct fat_directory_iterator fat_dir;
    struct fat_sector_itearator fat_sec;
    struct fat_directory_entry *sec;
    uint32_t sector;
    uint8_t buffer[513];
    uint32_t count = 0;
    fat_directory_iterator_root_dir(&fat_dir, &fs);
    while((sec = fat_directory_iterator_next(&fat_dir)) != 0) {
        buffer[11] = '\0';
		memcpy(buffer, sec->DIR_Name, 11);
		kprintf("dir_entry: name: %s (%d) [%0d] (0x%8h:0x%8h)\n", buffer, sec->DIR_FileSize, count, fat_dir.sec_iter.current_cluster, fat_dir.sec_iter.current_sector);
        if (sec->DIR_Attr & 0x10) {
            struct fat_directory_iterator dir_dir;

            fat_directory_iterator(&dir_dir, sec, &fs);
            while((sec = fat_directory_iterator_next(&dir_dir)) != 0) {
                buffer[11] = '\0';
                memcpy(buffer, sec->DIR_Name, 11);
                kprintf("dir_entry: name: %s (%d)\n", buffer, sec->DIR_FileSize);
            }
        } else {
            count++;
            fat_sector_itearator(&fat_sec, sec, &fs);
            //while((sector = fat_sector_iterator_next(&fat_sec)) != 0) {
                //buffer[512] = '\0';
                //ide_read_sectors(fs.device, 1, sector, buffer);
                //kprintf("%s", buffer);
            //}
        }
    }
    */

    /* oh boy */
    int *maptest = (int *)malloc(sizeof(int) * 1000);
    maptest[300] = 8;
    free(maptest);

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

extern void update_kernel_stack(void *stack);

#define USER_KERN_STACK (virtaddr_t)0xc004ffff
#define USER_STACk (virtaddr_t)0xbfffffff

void prepare_switch_to_usermode() {
    unsigned int *user_page_directory = add_vm_entry(0, PAGE_SIZE, VM_MAP_ANONYMOUS | VM_MAP_WRITE);
    unsigned int *user_kernel_table_directory = add_vm_entry(0, PAGE_SIZE, VM_MAP_ANONYMOUS | VM_MAP_WRITE);
    unsigned int *user_stack_table_directory = add_vm_entry(0, PAGE_SIZE, VM_MAP_ANONYMOUS | VM_MAP_WRITE);
    unsigned int *user_stack_limit =  add_vm_entry(0, PAGE_SIZE, VM_MAP_ANONYMOUS | VM_MAP_USER | VM_MAP_WRITE) + PAGE_SIZE - 1;
    unsigned int *kernel_stack_limit =  add_vm_entry(0, PAGE_SIZE, VM_MAP_ANONYMOUS | VM_MAP_WRITE) + PAGE_SIZE - 1;

    kprintf("user_page_directory: 0x%8h; user_stack_table_directory: 0x%8h; user_stack_limit: 0x%8h\n",
        user_page_directory, user_stack_table_directory, user_stack_limit);

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
        asm volatile("hlt");
    }
}