#include "bdev.h"
#include "fat.h"


struct {
    uint8_t reserved;
    struct bdev_operation *ops;
    void *bdev;
    uint32_t name[11];

} bdev[8];

typedef enum bdev_payload_status (*bdev_payload_init)(uint8_t device);
bdev_payload_init payloads[] = {
    fat_init,
};

uint32_t payloads_sz = sizeof(payloads) / sizeof(bdev_payload_init);

void bdev_init() {
    memset(&bdev, 0, sizeof(bdev));
}

int bdev_register(struct bdev_operation *ops, void *ext) {
    for (int i = 0; i < 8; i++) {
        if (bdev[i].reserved == 0) {
            bdev[i].reserved = 1;
            bdev[i].ops = ops;
            bdev[i].bdev = ext;

            for(uint32_t j = 0; j < payloads_sz; j++) {
                if (payloads[j](i) < BDEV_FORWARD) {
                    break;
                }
            }

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