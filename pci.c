#include "io.h"

typedef unsigned char uint8_t;
typedef unsigned short uint16_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;

#define PCI_BSFO_TO_ADDRESS(bus, slot, func, offset)                                                                   \
	(((uint32_t)((uint32_t)(bus) << 16) | ((uint32_t)(slot) << 11) | (uint32_t)(func) << 8) |                        \
	 ((uint32_t)(offset)&0xFC))
#define PCI_CONFIG_ADDRESS 0xCF8
#define PCI_CONFIG_DATA 0xCFC

#define PCI_UNKNOWN_VENDOR_ID 0xFFFF

extern void kprintf(const char *format, ...);

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

typedef void (*driver_init)(struct pci_header *);

void ata_init(struct pci_header *head);

struct pci_driver {
	uint16_t vendor_id;
	uint16_t device_id;
	driver_init init;
};

struct pci_driver pci_drivers[] = {
    {0x8086, 0x7010, ata_init},
};

uint32_t pci_drivers_sz = sizeof(pci_drivers) / sizeof(struct pci_driver);

void pci_config_read_header(struct pci_header *head, uint8_t bus, uint8_t slot, uint8_t fonc);
void pci_print_header(struct pci_header *head);
void pci_scan_bus(uint8_t bus);
void pci_scan_device(uint8_t bus, uint8_t device);
uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t fonc, uint8_t offset);
void pci_init_device(struct pci_header *head);

uint32_t pci_config_read(uint8_t bus, uint8_t slot, uint8_t fonc, uint8_t offset) {
	uint32_t address = PCI_BSFO_TO_ADDRESS(bus, slot, fonc, offset) | 0x80000000;

	outl(PCI_CONFIG_ADDRESS, address);
	return inl(PCI_CONFIG_DATA);
}

void pci_config_read_header(struct pci_header *head, uint8_t bus, uint8_t slot, uint8_t fonc) {
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

void pci_scan_device(uint8_t bus, uint8_t device) {
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

void pci_print_header(struct pci_header *head) {
	kprintf("PCI head: vendor_id: 0x%3h; device_id: 0x%3h; class: 0x%2h; subclass: 0x%2h\n", head->vendor_id,
		  head->device_id, head->class, head->subclass);
}

void pci_init_device(struct pci_header *head) {
	for (uint32_t i = 0; i < pci_drivers_sz; i++) {
		if (head->vendor_id == pci_drivers[i].vendor_id && head->device_id == pci_drivers[i].device_id) {
			pci_drivers[i].init(head);
		}
	}
}

/*
 * ATA
 */

struct IDEChannelRegisters {
	unsigned short base;  // I/O Base.
	unsigned short ctrl;  // Control Base
	unsigned short bmide; // Bus Master IDE
	unsigned char nIEN;   // nIEN (No Interrupt);
} channels[2];

struct ide_device {
	unsigned char Reserved;	     // 0 (Empty) or 1 (This Drive really exists).
	unsigned char Channel;	     // 0 (Primary Channel) or 1 (Secondary Channel).
	unsigned char Drive;	     // 0 (Master Drive) or 1 (Slave Drive).
	unsigned short Type;	     // 0: ATA, 1:ATAPI.
	unsigned short Signature;    // Drive Signature
	unsigned short Capabilities; // Features.
	unsigned int CommandSets;    // Command Sets Supported.
	unsigned int Size;	     // Size in Sectors.
	unsigned char Model[41];     // Model in string.
} ide_devices[4];

unsigned char ide_read(unsigned char channel, unsigned char reg);
void ide_write(unsigned char channel, unsigned char reg, unsigned char data);
void ide_read_buffer(unsigned char channel, unsigned char reg, unsigned int buffer, unsigned int quads);

#define ATA_PRIMARY 0x00
#define ATA_SECONDARY 0x01

#define ATA_READ 0x00
#define ATA_WRITE 0x01

#define ATA_SR_BSY 0x80	 // Busy
#define ATA_SR_DRDY 0x40 // Drive ready
#define ATA_SR_DF 0x20	 // Drive write fault
#define ATA_SR_DSC 0x10	 // Drive seek complete
#define ATA_SR_DRQ 0x08	 // Data request ready
#define ATA_SR_CORR 0x04 // Corrected data
#define ATA_SR_IDX 0x02	 // Index
#define ATA_SR_ERR 0x01	 // Error

#define ATA_ER_BBK 0x80	  // Bad block
#define ATA_ER_UNC 0x40	  // Uncorrectable data
#define ATA_ER_MC 0x20	  // Media changed
#define ATA_ER_IDNF 0x10  // ID mark not found
#define ATA_ER_MCR 0x08	  // Media change request
#define ATA_ER_ABRT 0x04  // Command aborted
#define ATA_ER_TK0NF 0x02 // Track 0 not found
#define ATA_ER_AMNF 0x01  // No address mark

#define ATA_CMD_READ_PIO 0x20
#define ATA_CMD_READ_PIO_EXT 0x24
#define ATA_CMD_READ_DMA 0xC8
#define ATA_CMD_READ_DMA_EXT 0x25
#define ATA_CMD_WRITE_PIO 0x30
#define ATA_CMD_WRITE_PIO_EXT 0x34
#define ATA_CMD_WRITE_DMA 0xCA
#define ATA_CMD_WRITE_DMA_EXT 0x35
#define ATA_CMD_CACHE_FLUSH 0xE7
#define ATA_CMD_CACHE_FLUSH_EXT 0xEA
#define ATA_CMD_PACKET 0xA0
#define ATA_CMD_IDENTIFY_PACKET 0xA1
#define ATA_CMD_IDENTIFY 0xEC

#define ATAPI_CMD_READ 0xA8
#define ATAPI_CMD_EJECT 0x1B

#define IDE_ATA 0x00
#define IDE_ATAPI 0x01

#define ATA_REG_DATA 0x00
#define ATA_REG_ERROR 0x01
#define ATA_REG_FEATURES 0x01
#define ATA_REG_SECCOUNT0 0x02
#define ATA_REG_LBA0 0x03
#define ATA_REG_LBA1 0x04
#define ATA_REG_LBA2 0x05
#define ATA_REG_HDDEVSEL 0x06
#define ATA_REG_COMMAND 0x07
#define ATA_REG_STATUS 0x07
#define ATA_REG_SECCOUNT1 0x08
#define ATA_REG_LBA3 0x09
#define ATA_REG_LBA4 0x0A
#define ATA_REG_LBA5 0x0B
#define ATA_REG_CONTROL 0x0C
#define ATA_REG_ALTSTATUS 0x0C
#define ATA_REG_DEVADDRESS 0x0D

#define ATA_IDENT_DEVICETYPE 0
#define ATA_IDENT_CYLINDERS 2
#define ATA_IDENT_HEADS 6
#define ATA_IDENT_SECTORS 12
#define ATA_IDENT_SERIAL 20
#define ATA_IDENT_MODEL 54
#define ATA_IDENT_CAPABILITIES 98
#define ATA_IDENT_FIELDVALID 106
#define ATA_IDENT_MAX_LBA 120
#define ATA_IDENT_COMMANDSETS 164
#define ATA_IDENT_MAX_LBA_EXT 200

unsigned char ide_buf[2048] = {0};
unsigned static char ide_irq_invoked = 0;
unsigned static char atapi_packet[12] = {0xA8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

extern void sleep(unsigned int t);

void ata_init(struct pci_header *head) {
	uint32_t bar0, bar1, bar2, bar3, bar4;

	if ((head->prog_if & 1) == 0) {
		bar0 = 0x1F0;
		bar1 = 0x3F6;
	} else {
		bar0 = head->specific.type0.bar0;
		bar1 = head->specific.type0.bar1;
	}

	if ((head->prog_if & 4) == 0) {
		bar2 = 0x170;
		bar3 = 0x376;
	} else {
		bar2 = head->specific.type0.bar2;
		bar3 = head->specific.type0.bar3;
	}

	bar4 = head->specific.type0.bar4; // probably 0

	// 1- Detect I/O Ports which interface IDE Controller:
	channels[ATA_PRIMARY].base = (bar0 & 0xFFFFFFFC) + 0x1F0 * (!bar0);
	channels[ATA_PRIMARY].ctrl = (bar1 & 0xFFFFFFFC) + 0x3F6 * (!bar1);
	channels[ATA_SECONDARY].base = (bar2 & 0xFFFFFFFC) + 0x170 * (!bar2);
	channels[ATA_SECONDARY].ctrl = (bar3 & 0xFFFFFFFC) + 0x376 * (!bar3);
	channels[ATA_PRIMARY].bmide = (bar4 & 0xFFFFFFFC) + 0;   // Bus Master IDE
	channels[ATA_SECONDARY].bmide = (bar4 & 0xFFFFFFFC) + 8; // Bus Master IDE

	// 2- Disable IRQs:
	ide_write(ATA_PRIMARY, ATA_REG_CONTROL, 2);
	ide_write(ATA_SECONDARY, ATA_REG_CONTROL, 2);

	// 3- Detect ATA-ATAPI Devices:
	int count = 0;
	for (int i = 0; i < 2; i++) {
		for (int j = 0; j < 2; j++) {
			unsigned char err = 0, type = IDE_ATA, status;
			ide_devices[count].Reserved = 0; // Assuming that no drive here.

			// (I) Select Drive:
			ide_write(i, ATA_REG_HDDEVSEL, 0xA0 | (j << 4)); // Select Drive.
			sleep(1);					 // Wait 1ms for drive select to work.

			// (II) Send ATA Identify Command:
			ide_write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
			sleep(1); // This function should be implemented in your OS. which waits for 1 ms.
				     // it is based on System Timer Device Driver.

			// (III) Polling:
			if (ide_read(i, ATA_REG_STATUS) == 0)
				continue; // If Status = 0, No Device.

			while (1) {
				status = ide_read(i, ATA_REG_STATUS);
				if ((status & ATA_SR_ERR)) {
					err = 1;
					break;
				} // If Err, Device is not ATA.
				if (!(status & ATA_SR_BSY) && (status & ATA_SR_DRQ))
					break; // Everything is right.
			}

			// (IV) Probe for ATAPI Devices:
			if (err != 0) {
				unsigned char cl = ide_read(i, ATA_REG_LBA1);
				unsigned char ch = ide_read(i, ATA_REG_LBA2);

				if (cl == 0x14 && ch == 0xEB)
					type = IDE_ATAPI;
				else if (cl == 0x69 && ch == 0x96)
					type = IDE_ATAPI;
				else
					continue; // Unknown Type (may not be a device).

				ide_write(i, ATA_REG_COMMAND, ATA_CMD_IDENTIFY_PACKET);
				sleep(1);
			}

			// (V) Read Identification Space of the Device:
			ide_read_buffer(i, ATA_REG_DATA, (unsigned int)ide_buf, 128);

			// (VI) Read Device Parameters:
			ide_devices[count].Reserved = 1;
			ide_devices[count].Type = type;
			ide_devices[count].Channel = i;
			ide_devices[count].Drive = j;
			ide_devices[count].Signature = *((unsigned short *)(ide_buf + ATA_IDENT_DEVICETYPE));
			ide_devices[count].Capabilities = *((unsigned short *)(ide_buf + ATA_IDENT_CAPABILITIES));
			ide_devices[count].CommandSets = *((unsigned int *)(ide_buf + ATA_IDENT_COMMANDSETS));

			// (VII) Get Size:
			if (ide_devices[count].CommandSets & (1 << 26)) {
				// Device uses 48-Bit Addressing:
				ide_devices[count].Size = *((unsigned int *)(ide_buf + ATA_IDENT_MAX_LBA_EXT));
			} else {
				// Device uses CHS or 28-bit Addressing:
				ide_devices[count].Size = *((unsigned int *)(ide_buf + ATA_IDENT_MAX_LBA));
			}

			// (VIII) String indicates model of device (like Western Digital HDD and SONY DVD-RW...):
			for (int k = 0; k < 40; k += 2) {
				ide_devices[count].Model[k] = ide_buf[ATA_IDENT_MODEL + k + 1];
				ide_devices[count].Model[k + 1] = ide_buf[ATA_IDENT_MODEL + k];
			}
			ide_devices[count].Model[40] = 0; // Terminate String.

			count++;
		}
	}

	// 4- Print Summary:
	for (int i = 0; i < 4; i++)
		if (ide_devices[i].Reserved == 1) {
			kprintf(" Found %s Drive %dGB - %s\n", (const char *[]){"ATA", "ATAPI"}[ide_devices[i].Type],
				  ide_devices[i].Size / 1024 / 1024 / 2, ide_devices[i].Model);
		}
}

unsigned char ide_read(unsigned char channel, unsigned char reg) {
	unsigned char result;
	if (reg > 0x07 && reg < 0x0C)
		ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
	if (reg < 0x08)
		result = inb(channels[channel].base + reg - 0x00);
	else if (reg < 0x0C)
		result = inb(channels[channel].base + reg - 0x06);
	else if (reg < 0x0E)
		result = inb(channels[channel].ctrl + reg - 0x0A);
	else if (reg < 0x16)
		result = inb(channels[channel].bmide + reg - 0x0E);
	if (reg > 0x07 && reg < 0x0C)
		ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
	return result;
}

void ide_write(unsigned char channel, unsigned char reg, unsigned char data) {
	if (reg > 0x07 && reg < 0x0C)
		ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
	if (reg < 0x08)
		outb(channels[channel].base + reg - 0x00, data);
	else if (reg < 0x0C)
		outb(channels[channel].base + reg - 0x06, data);
	else if (reg < 0x0E)
		outb(channels[channel].ctrl + reg - 0x0A, data);
	else if (reg < 0x16)
		outb(channels[channel].bmide + reg - 0x0E, data);
	if (reg > 0x07 && reg < 0x0C)
		ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
}

void ide_read_buffer(unsigned char channel, unsigned char reg, unsigned int buffer, unsigned int quads) {
	if (reg > 0x07 && reg < 0x0C)
		ide_write(channel, ATA_REG_CONTROL, 0x80 | channels[channel].nIEN);
    unsigned int *c = (unsigned int *)buffer;
	for (unsigned int i = 0; i < quads; i++) {
		if (reg < 0x08)
			*c++ = inl(channels[channel].base + reg - 0x00);
		else if (reg < 0x0C)
			*c++ = inl(channels[channel].base + reg - 0x06);
		else if (reg < 0x0E)
			*c++ = inl(channels[channel].ctrl + reg - 0x0A);
		else if (reg < 0x16)
			*c++ = inl(channels[channel].bmide + reg - 0x0E);
	}
	if (reg > 0x07 && reg < 0x0C)
		ide_write(channel, ATA_REG_CONTROL, channels[channel].nIEN);
}