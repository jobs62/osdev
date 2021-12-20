#ifndef __VIRTIO_BLK__
#define __VIRTIO_BLK__

#include "stdtype.h"
#include "pci.h"

void virtio_blk_init(struct pci_header *head, uint8_t bus, uint8_t slot, uint8_t fonc);

#endif