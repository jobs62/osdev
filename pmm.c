#include "pmm.h"

// in the bitmap:
//  1 in page free
//  0 in page used
unsigned char bitmap[PAGE_SIZE];


#define BITS_IN_BYTE 8

void bitmap_clear() {
    memset(bitmap, 0, PAGE_SIZE);
}

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