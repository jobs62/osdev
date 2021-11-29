#ifndef __ATA__
#define __ATA__

#include "stdtype.h"
#include "pci.h"

uint8_t ide_read_sectors(uint8_t drive, uint8_t numsects, uint32_t lba, void *edi);
void ata_init(struct pci_header *head, uint8_t bus, uint8_t slot, uint8_t fonc);

#endif