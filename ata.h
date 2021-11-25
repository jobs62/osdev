#ifndef __ATA__
#define __ATA__

#include "stdtype.h"
#include "pci.h"

uint8_t ide_read_sectors(uint8_t drive, uint8_t numsects, uint32_t lba, uint32_t edi);
void ata_init(struct pci_header *head);

#endif