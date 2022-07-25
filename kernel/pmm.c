#include "pmm.h"
#include "stdlib.h"
#include <stdint.h>

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
    unsigned int index = page / BITS_IN_BYTE;
    int mask = 1 << (page % BITS_IN_BYTE);

    bitmap[index] |= mask; 
}

int bitmap_page_status(physaddr_t page) {
    page /= PAGE_SIZE;
    unsigned int index = page / BITS_IN_BYTE;
    int mask = 1 << (page % BITS_IN_BYTE);

    return (bitmap[index] & mask) == 0; 
}

physaddr_t bitmap_find_free_page() {
    uint32_t index;
    uint32_t jndex;
    
    for (index = 0; index < PAGE_SIZE; index++) {
        for (jndex = 0; jndex < BITS_IN_BYTE; jndex++) {
            if ((bitmap[index] & (1 << jndex)) != 0) {
                return (index * BITS_IN_BYTE + jndex) * PAGE_SIZE;    
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