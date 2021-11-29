#ifndef __PCI__
#define __PCI__

#include "stdtype.h"

struct pci_header {
	uint16_t vendor_id;
	uint16_t device_id;
	uint16_t command;
	uint16_t status;
	uint8_t revison_id;
	uint8_t prog_if;
	uint8_t subclass;
	uint8_t class;
	uint8_t cache_line_size;
	uint8_t latency_timer;
	uint8_t header_type;
	uint8_t bist;
	union {
		struct {
			uint32_t bar0;
			uint32_t bar1;
			uint32_t bar2;
			uint32_t bar3;
			uint32_t bar4;
			uint32_t bar5;
			uint32_t cardbus_cis_ptr;
			uint16_t subsystem_vendor_id;
			uint16_t subsystem_id;
			uint32_t rom_base_address;
			uint8_t capabilities_ptr;
			uint8_t reserved[7];
			uint8_t interrupt_line;
			uint8_t interrupt_pin;
			uint8_t min_grant;
			uint8_t max_latency;
		} __attribute__((packed)) type0;
	} __attribute__((packed)) specific;
} __attribute__((packed));

typedef void (*driver_init)(struct pci_header *, uint8_t, uint8_t, uint8_t);

void pci_scan_bus(uint8_t bus);
uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t fonc, uint8_t offset);

#endif