#include "bdev.h"

struct {
    uint8_t reserved;
    struct bdev_operation *ops;
    void *bdev;
} bdev[8];

void bdev_init() {
    memset(&bdev, 0, sizeof(bdev));
}

int bdev_register(struct bdev_operation *ops, void *ext) {
    for (int i = 0; i < 8; i++) {
        if (bdev[i].reserved == 0) {
            bdev[i].reserved = 1;
            bdev[i].ops = ops;
            bdev[i].bdev = ext;

            fat_init(i);

            return i;
        }
    }

    return -1;
}

int bdev_read(uint8_t device, uint32_t numsect, uint32_t lba, void *edi) {
    if (bdev[device].reserved == 0) {
        return -1;
    }

    if (bdev[device].ops->read == (void *)0) {
        return -2;
    }

    return bdev[device].ops->read(bdev[device].bdev, numsect, lba, edi);
}

int bdev_write(uint8_t device, uint32_t numsect, uint32_t lba, void *edi) {
    if (bdev[device].reserved == 0) {
        return -1;
    }

    if (bdev[device].ops->write == (void *)0) {
        return -2;
    }

    return bdev[device].ops->write(bdev[device].bdev, numsect, lba, edi);
} 