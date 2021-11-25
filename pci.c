#include "io.h"
#include "stdtype.h"
#include "pci.h"
#include "ata.h"

#define PCI_BSFO_TO_ADDRESS(bus, slot, func, offset)                                                                   \
	(((uint32_t)((uint32_t)(bus) << 16) | ((uint32_t)(slot) << 11) | (uint32_t)(func) << 8) |                        \
	 ((uint32_t)(offset)&0xFC))
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC

#define PCI_UNKNOWN_VENDOR_ID 0xFFFF

extern void kprintf(const char *format, ...);

struct pci_driver {
	uint16_t vendor_id;
	uint16_t device_id;
	driver_init init;
};

struct pci_driver pci_drivers[] = {
    {0x8086, 0x7010, ata_init},
};

uint32_t pci_drivers_sz = sizeof(pci_drivers) / sizeof(struct pci_driver);

static uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t fonc, uint8_t offset);
static void pci_config_read_header(struct pci_header *head, uint8_t bus, uint8_t slot, uint8_t fonc);
static void pci_scan_device(uint8_t bus, uint8_t device);
static void pci_print_header(struct pci_header *head);
static void pci_init_device(struct pci_header *head);


static uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t fonc, uint8_t offset) {
	uint32_t address = PCI_BSFO_TO_ADDRESS(bus, slot, fonc, offset) | 0x80000000;

	outl(PCI_CONFIG_ADDRESS, address);
	return inl(PCI_CONFIG_DATA);
}

static void pci_config_read_header(struct pci_header *head, uint8_t bus, uint8_t slot, uint8_t fonc) {
	uint32_t *buffer = (uint32_t *)head;
	uint32_t i;

	buffer[0] = pci_config_read(bus, slot, fonc, 0x00);
	if (head->vendor_id == PCI_UNKNOWN_VENDOR_ID)
		return; // the device don't exist

	for (i = 1; i < 16; i++) {
		buffer[i] = pci_config_read(bus, slot, fonc, i * 4);
	}
}

void pci_scan_bus(uint8_t bus) {
	for (uint8_t i = 0; i < 32; i++) {
		pci_scan_device(bus, i);
	}
}

static void pci_scan_device(uint8_t bus, uint8_t device) {
	struct pci_header pci_header;

	pci_config_read_header(&pci_header, bus, device, 0);
	if (pci_header.vendor_id == PCI_UNKNOWN_VENDOR_ID) {
		return;
	}

	pci_print_header(&pci_header);
	pci_init_device(&pci_header);

	if ((pci_header.header_type & 0x80) != 0) {
		for (uint8_t i = 1; i < 8; i++) {
			pci_config_read_header(&pci_header, bus, device, i);
			if (pci_header.vendor_id == PCI_UNKNOWN_VENDOR_ID) {
				continue;
			}
			pci_print_header(&pci_header);
			pci_init_device(&pci_header);
		}
	}
}

static void pci_print_header(struct pci_header *head) {
	kprintf("PCI head: vendor_id: 0x%3h; device_id: 0x%3h; class: 0x%2h; subclass: 0x%2h\n", head->vendor_id,
		  head->device_id, head->class, head->subclass);
}

static void pci_init_device(struct pci_header *head) {
	for (uint32_t i = 0; i < pci_drivers_sz; i++) {
		if (head->vendor_id == pci_drivers[i].vendor_id && head->device_id == pci_drivers[i].device_id) {
			pci_drivers[i].init(head);
		}
	}
}