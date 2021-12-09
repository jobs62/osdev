#ifndef __BDEV__
#define __BDEV__

#include "stdtype.h"

struct bdev_operation{
    int (*read)(void *bdev, uint32_t numsect, uint32_t lba, void *edi);
    int (*write)(void *bdev, uint32_t numsect, uint32_t lba, void *edi);
};

void bdev_init(void);
int bdev_register(struct bdev_operation *ops, void *ext);
int bdev_read(uint8_t device, uint32_t numsect, uint32_t lba, void *edi);
int bdev_write(uint8_t device, uint32_t numsect, uint32_t lba, void *edi);

#endif