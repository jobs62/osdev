#include "bdev.h"
#include "stdtype.h"
#include "liballoc.h"

struct mbr_entry {
    uint8_t drive_attribute;
    uint8_t chs_start[3];
    uint8_t partition_type;
    uint8_t chs_stop[3];
    uint32_t lba_start;
    uint32_t num_sectors;
} __attribute__((packed));

struct mbr {
    uint32_t uuid;
    uint16_t reserved;
    struct mbr_entry entries[4];
    uint16_t signature;
};

struct mbr_partition {
    uint32_t lba_start;
    uint32_t num_sector;
    uint8_t drive;
};

uint32_t last_uuid = 0xdeadbeaf;

static int mbr_partition_read(void *bdev, uint32_t numsect, uint32_t lba, void *edi) {
    struct mbr_partition *part = (struct mbr_partition *)bdev;

    if (lba > part->num_sector || lba + numsect > part->num_sector) {
        return -1;
    }

    return bdev_read(part->drive, numsect, part->lba_start + lba, edi);
}

static int mbr_partition_write(void *bdev, uint32_t numsect, uint32_t lba, void *edi) {
    struct mbr_partition *part = (struct mbr_partition *)bdev;

    if (lba > part->num_sector || lba + numsect > part->num_sector) {
        return -1;
    }

    return bdev_write(part->drive, numsect, part->lba_start + lba, edi);
}

struct bdev_operation mbr_partition_ops = {
    .read = mbr_partition_read,
    .write = mbr_partition_write,
};

enum bdev_payload_status mbr_init(uint8_t drive) {
    uint8_t buffer[512];
    struct mbr *mbr = (struct mbr *)(&buffer[0x1b8]);
    uint8_t valid = 0;

    kprintf("mbr init drive %1d\n", drive);

    if (bdev_read(drive, 1, 0, buffer) != 0) {
        kprintf("bdev_read error\n");
        return BDEV_ERROR;
    }

    if (mbr->signature != 0xAA55) {
        kprintf("mbr sig lol (0x%4h)\n", mbr->signature);
        return BDEV_FORWARD;
    }

    if (mbr->uuid == last_uuid) {
        kprintf("last uuid\n");
        return BDEV_FORWARD;
    }

    last_uuid = mbr->uuid;

    for (uint8_t i = 0; i < 4; i++) {
        switch (mbr->entries[i].partition_type) {
            case 0x0c:
            case 0x0e:
                break;

            default:
                continue;
        }

        struct mbr_partition *part = (struct mbr_partition *)malloc(sizeof(struct mbr_partition));
        part->lba_start = mbr->entries[i].lba_start;
        part->num_sector = mbr->entries[i].num_sectors;
        part->drive = drive;
        valid++;

        bdev_register(&mbr_partition_ops, part);
    }

    if (valid == 0) {
        return BDEV_FORWARD;
    }

    kprintf("bdev setup ok\n");

    return BDEV_SUCCESS;
}