#ifndef __ATA__
#define __ATA__

#include "stdtype.h"
#include "pci.h"

void ata_init(struct pci_header *head, uint8_t bus, uint8_t slot, uint8_t fonc);

#endif