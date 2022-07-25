#include <stdint.h>
#include "pci.h"
#include "io.h"
#include "vmm.h"
#include "pmm.h"
#include "bdev.h"
#include "liballoc.h"

#define VIRTQ_DESC_F_NEXT 1
#define VIRTQ_DESC_F_WRITE 2
#define VIRTQ_DESC_F_INDIRECT 4

struct virtq_desc {
    uint64_t address;
    uint32_t length;
    uint16_t flags;
    uint16_t next;
};

struct virtq_avail {
    uint16_t flags;
    uint16_t index;
    uint16_t ring[];
};

struct virtq_used_elem {
    uint32_t index;
    uint32_t len;
};

struct virtq_used {
    uint16_t flags;
    uint16_t index;
    struct virtq_used_elem ring[];
};

struct virtq {
    struct virtq_desc *desc;
    struct virtq_avail *avail;
    struct virtq_used *used;
};

struct virtio_blk {
    uint16_t base;
    struct virtq queue;
    uint16_t queue_size;
    uint16_t last_seen;
};

struct virtio_blk_req_header {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
} __attribute__((packed));

#define ALIGN(x) (((x) + 4096) & ~4096)
static inline uint32_t virtq_size(uint32_t queue_size) {
    return ALIGN(16 * queue_size + 6 + 2 * queue_size) + ALIGN(6 + 8 * queue_size);
}

static int virtio_blk_read(void *bdev, uint32_t numsect, uint32_t lba, void *edi) {
    struct virtio_blk_req_header header;
    struct virtio_blk *blk = (struct virtio_blk *)bdev;
    uint8_t status;

    header.type = 0; //Read op
    header.sector = lba;
    mfence();

    //kprintf("numsect: %1d; lba: 0x%8h\n", numsect, lba);

    blk->queue.desc[0].address = get_physaddr(&header);
    blk->queue.desc[0].length = 16;
    blk->queue.desc[0].flags = 1; //Next and read
    blk->queue.desc[0].next = 1;
    blk->queue.desc[1].address = get_physaddr(edi);
    blk->queue.desc[1].length = 512 * numsect;
    blk->queue.desc[1].flags = 1 | 2; // write and next
    blk->queue.desc[1].next = 2;
    blk->queue.desc[2].address = get_physaddr(&status);
    blk->queue.desc[2].length = 1;
    blk->queue.desc[2].flags = 2; // write
    mfence();

    blk->queue.avail->ring[blk->queue.avail->index] = 0;
    blk->queue.avail->index = (blk->queue.avail->index + 1) % blk->queue_size;
    mfence();

    //device notification
    outb(blk->base + 0x10, 0);

    // wait for result
    for(;;) {
        mfence();
        if (blk->queue.used->index != blk->last_seen) {
            blk->last_seen = (blk->last_seen + 1) % blk->queue_size;
            break;
        }
    }

    //kprintf("read finished\n");

    return status;
}

struct bdev_operation virtio_blk_ops = {
    .read = virtio_blk_read,
};

extern uint8_t __stdlib_unsafe;

void virtio_blk_init(struct pci_header *head, uint8_t bus, uint8_t slot, uint8_t fonc) {
    kprintf("virtio_blk_init: bar0: 0x%8h\n", head->specific.type0.bar0);
    struct virtio_blk *device = (struct virtio_blk *)malloc(sizeof(struct virtio_blk));

    device->base = head->specific.type0.bar0 & 0xffffc;

    outb(device->base + 0x12, 0); //reset
    outb(device->base + 0x12, inb(device->base + 0x12) | 1); //Acknowledge
    outb(device->base + 0x12, inb(device->base + 0x12) | 2); //i know how to drive that thing (kind of loul)
    outl(device->base + 0x04, inl(device->base + 0x00) & (9 | 13 | 1 | 2 | 6));
    outb(device->base + 0x12, inb(device->base + 0x12) | 8); //Feature ok 

    device->queue_size = inl(device->base + 0x0c);
    kprintf("virtio_blk_init: queue size: 0x%4h\n", device->queue_size);
    uint32_t desc_size = 16 * device->queue_size;
    uint32_t avail_size = 6 + 2 * device->queue_size;
    uint32_t totan_queue_size = ((virtq_size(device->queue_size) / 4096) + 1) * 4096; //to have full page size

    device->queue.desc = (struct virtq_desc *)add_vm_entry((void *)0xC0200000, totan_queue_size, VM_MAP_ANONYMOUS | VM_MAP_WRITE | VM_MAP_KERNEL, (void *)0, 0, 0);
    memset_unsafe(device->queue.desc, 0, totan_queue_size);
    device->queue.avail = (struct virtq_avail *)((uint32_t)device->queue.desc + desc_size);
    device->queue.used = (struct virtq_used *)(((((uint32_t)device->queue.avail + avail_size) / 4096) + 1) * 4096);

    outw(device->base + 0x0e, 0); //select queue 0
    outl(device->base + 0x08, get_physaddr(device->queue.desc) / 4096); //set queue adresse
    outb(device->base + 0x12, inb(device->base + 0x12) | 4); //driver_ok

    kprintf("virtio_blk_init: device status: 0x%2h\n", inb(device->base + 0x12));
    kprintf("virtio_blk_init: device feature: 0x%8h\n", inl(device->base + 0x00));
    kprintf("virtio_blk_init: isr line: 0x%2h\n", head->specific.type0.interrupt_line);
    kprintf("virtio_blk_init: isr status: 0x%2h\n", inb(device->base + 0x13));
    kprintf("virtio_blk_init: desc_size: 0x%8h\n", desc_size);
    kprintf("virtio_blk_init: avai_size: 0x%8h\n", avail_size);
    kprintf("virtio_blk_init: totan_queue_size: 0x%8h\n", totan_queue_size);
    kprintf("virtio_blk_init: virtq_desc: 0x%8h (0x%8h)\n", get_physaddr(device->queue.desc), device->queue.desc);
    kprintf("virtio_blk_init: virtq_avail: 0x%8h (0x%8h)\n", get_physaddr(device->queue.avail), device->queue.avail);
    kprintf("virtio_blk_init: virtq_used: 0x%8h (0x%8h)\n", get_physaddr(device->queue.used), device->queue.used);

    device->last_seen = device->queue.used->index;

    bdev_register(&virtio_blk_ops, device);
}
