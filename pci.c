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
void fat_init(uint8_t drive);

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
uint8_t ide_polling(uint8_t channel, uint8_t advanced_check);
uint8_t ide_ata_access(uint8_t direction, uint8_t drive, uint32_t lba, uint8_t numsects, uint32_t edi);
uint8_t ide_read_sectors(uint8_t drive, uint8_t numsects, uint32_t lba, uint32_t edi);

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

			if (ide_devices[i].Type == 0) {
				fat_init(i);
			}
		}
}

unsigned char ide_read(unsigned char channel, unsigned char reg) {
	//kprintf("ide_read: reg: Ox%2h\n", reg);

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
	//kprintf("ide_write: reg: Ox%2h; data: 0x%2h\n", reg, data);

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

uint8_t ide_ata_access(uint8_t direction, uint8_t drive, uint32_t lba, uint8_t numsects, uint32_t edi) {
	uint8_t lba_mode, cmd;
	uint8_t lba_io[6];
	uint8_t channel = ide_devices[drive].Channel;
	uint8_t slavebit = ide_devices[drive].Drive;
	uint32_t bus = channels[channel].base;
	uint32_t words = 256;
	uint16_t cyl, i;
	uint8_t head, sect, err;
	uint8_t dma;

	if (lba >= 0x10000000) {
		//LBA48
		lba_mode = 2;
		lba_io[0] = (lba & 0x000000FF) >> 0;
    	lba_io[1] = (lba & 0x0000FF00) >> 8;
    	lba_io[2] = (lba & 0x00FF0000) >> 16;
    	lba_io[3] = (lba & 0xFF000000) >> 24;
    	lba_io[4] = 0; // LBA28 is integer, so 32-bits are enough to access 2TB.
    	lba_io[5] = 0; // LBA28 is integer, so 32-bits are enough to access 2TB.
      	head      = 0; // Lower 4-bits of HDDEVSEL are not used here.
	} else if (ide_devices[drive].Capabilities & 0x200) {
		//LBA28
		lba_mode  = 1;
      	lba_io[0] = (lba & 0x00000FF) >> 0;
      	lba_io[1] = (lba & 0x000FF00) >> 8;
      	lba_io[2] = (lba & 0x0FF0000) >> 16;
      	lba_io[3] = 0; // These Registers are not used here.
      	lba_io[4] = 0; // These Registers are not used here.
      	lba_io[5] = 0; // These Registers are not used here.
      	head      = (lba & 0xF000000) >> 24;
	} else {
		//CHS
      	lba_mode  = 0;
      	sect      = (lba % 63) + 1;
      	cyl       = (lba + 1  - sect) / (16 * 63);
      	lba_io[0] = sect;
      	lba_io[1] = (cyl >> 0) & 0xFF;
      	lba_io[2] = (cyl >> 8) & 0xFF;
      	lba_io[3] = 0;
      	lba_io[4] = 0;
      	lba_io[5] = 0;
      	head      = (lba + 1  - sect) % (16 * 63) / (63); // Head number is written to HDDEVSEL lower 4-bits.
	}

   	// (II) See if drive supports DMA or not;
   	dma = 0; // We don't support DMA

   // (III) Wait if the drive is busy;
   while (ide_read(channel, ATA_REG_STATUS) & ATA_SR_BSY) {}

    // (IV) Select Drive from the controller;
   if (lba_mode == 0) {
    	ide_write(channel, ATA_REG_HDDEVSEL, 0xA0 | (slavebit << 4) | head); // Drive & CHS.
	} else {
    	ide_write(channel, ATA_REG_HDDEVSEL, 0xE0 | (slavebit << 4) | head); // Drive & LBA
	}

	// (V) Write Parameters;
   	if (lba_mode == 2) {
      	ide_write(channel, ATA_REG_SECCOUNT1,   0);
      	ide_write(channel, ATA_REG_LBA3,   lba_io[3]);
      	ide_write(channel, ATA_REG_LBA4,   lba_io[4]);
      	ide_write(channel, ATA_REG_LBA5,   lba_io[5]);
   	}
	ide_write(channel, ATA_REG_SECCOUNT0,   numsects);
   	ide_write(channel, ATA_REG_LBA0,   lba_io[0]);
   	ide_write(channel, ATA_REG_LBA1,   lba_io[1]);
   	ide_write(channel, ATA_REG_LBA2,   lba_io[2]);

	if (lba_mode == 0 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO;
   	if (lba_mode == 1 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO;   
   	if (lba_mode == 2 && dma == 0 && direction == 0) cmd = ATA_CMD_READ_PIO_EXT;   
   	if (lba_mode == 0 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA;
   	if (lba_mode == 1 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA;
   	if (lba_mode == 2 && dma == 1 && direction == 0) cmd = ATA_CMD_READ_DMA_EXT;
   	if (lba_mode == 0 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO;
   	if (lba_mode == 1 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO;
   	if (lba_mode == 2 && dma == 0 && direction == 1) cmd = ATA_CMD_WRITE_PIO_EXT;
   	if (lba_mode == 0 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA;
   	if (lba_mode == 1 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA;
   	if (lba_mode == 2 && dma == 1 && direction == 1) cmd = ATA_CMD_WRITE_DMA_EXT;
	ide_write(channel, ATA_REG_COMMAND, cmd);
	
	if (dma == 1) {
		//No DMA support for now
	} else {
		if (direction == ATA_READ) {
			//Read
			for (i = 0; i < numsects; i++) {
				if ((err = ide_polling(channel, 1)) != 0) {
					return err;
				}
				
				for (unsigned int j = 0; j < words; j++) {
					*(uint16_t *)edi = inw(channels[channel].base + ATA_REG_DATA);
					edi += 2;
				}
			}
		} else {
			//No suppoort for write for now
		}
	}

	return (0);
}

uint8_t ide_polling(uint8_t channel, uint8_t advanced_check) {
	// (I) Delay 400 nanosecond for BSY to be set:
   	// -------------------------------------------------
   	for(int i = 0; i < 4; i++) {
      	ide_read(channel, ATA_REG_ALTSTATUS); // Reading the Alternate Status port wastes 100ns; loop four times.
	}

   	// (II) Wait for BSY to be cleared:
   	// -------------------------------------------------
   	while (ide_read(channel, ATA_REG_STATUS) & ATA_SR_BSY) {}
 
   	if (advanced_check) {
    	unsigned char state = ide_read(channel, ATA_REG_STATUS); // Read Status Register.
 
      	// (III) Check For Errors:
      	// -------------------------------------------------
      	if (state & ATA_SR_ERR) {
        	return 2; // Error.
		}
 
      	// (IV) Check If Device fault:
      	// -------------------------------------------------
      	if (state & ATA_SR_DF) {
        	return 1; // Device Fault.
		}
 
      	// (V) Check DRQ:
      	// -------------------------------------------------
      	// BSY = 0; DF = 0; ERR = 0 so we should check for DRQ now.
      	if ((state & ATA_SR_DRQ) == 0) {
        	return 3; // DRQ should be set
		}
 
   	}
 
   return 0; // No Error.
}


uint8_t ide_read_sectors(uint8_t drive, uint8_t numsects, uint32_t lba, uint32_t edi) {
	// 1: Check if the drive presents:
	if (drive > 3 || ide_devices[drive].Reserved == 0) {
		return 1;
	}

	// 2: Check if inputs are valid:
	if (((lba + numsects) > ide_devices[drive].Size) && (ide_devices[drive].Type == IDE_ATA)) {
		return 2;
	}

	if (ide_devices[drive].Type == IDE_ATA) {
		return ide_ata_access(ATA_READ, drive, lba, numsects, edi);
	}

	return 4;
}


/*
 * FAT
 */

struct fat_bpb_common {
	uint8_t BS_jmpBoot[3];
	uint8_t BS_OEMName[8];
	uint16_t BPB_BytsPerSec;
	uint8_t BPB_SecPerClus;
	uint16_t BPB_RsvdSecCnt;
	uint8_t BPB_NumFATs;
	uint16_t BPB_RootEntCnt;
	uint16_t BPB_TotSec16;
	uint8_t BPB_Media;
	uint16_t BPB_FATz16;
	uint16_t BPB_SecPerTrk;
	uint16_t BPB_NumHeads;
	uint32_t BPB_HiddSec;
	uint32_t BPB_TotSec32;
} __attribute__((packed));

struct fat16_extended {
	uint8_t BS_DrvNum;
	uint8_t BS_Reserved1;
	uint8_t BS_BootSig;
	uint32_t BS_VolID;
	uint8_t BS_VolLab[11];
	uint8_t BS_FilSysType[8];
	uint8_t Reserved2[448];
	uint16_t Signature_word;
} __attribute__((packed));

struct fat32_extended {
	uint32_t BPB_FATz32;
	uint16_t BPB_ExtFlags;
	uint16_t BPB_FSVer;
	uint32_t BPB_RootClus;
	uint16_t BPB_FSInfo;
	uint16_t BPB_BkBootSec;
	uint8_t BPB_Reserved[12];
	uint8_t BS_DrvNum;
	uint8_t BS_Reserved1;
	uint8_t BS_BootSig;
	uint32_t BS_VolID;
	uint8_t BS_VolLab[11];
	uint8_t BS_FilSysType[8];
	uint8_t Reserved2[420];
	uint16_t Signature_word;
} __attribute__((packed));

struct fat_bpb {
	struct fat_bpb_common common;
	union {
		struct fat16_extended fat16;
		struct fat32_extended fat32;
	} extended;
} __attribute__((packed));

struct fat_directory_entry {
	uint8_t DIR_Name[11];
	uint8_t DIR_Attr;
	uint8_t DIR_NTRes;
	uint8_t DIR_CrtTimeTenth;
	uint16_t DIR_CrtTime;
	uint16_t DIR_CrtDate;
	uint16_t DIR_LstAccDate;
	uint16_t DIR_FstClusHI;
	uint16_t DIR_WrtTime;
	uint16_t DIR_WrtDate;
	uint16_t DIR_FstClusLO;
	uint32_t DIR_FileSize;
};

#define FAT_TYPE_12 0
#define FAT_TYPE_16 1
#define FAT_TYPE_32 2

void fat_init(uint8_t drive) {
	struct fat_bpb bpb;
	uint8_t fat_type;

	if (ide_read_sectors(drive, 1, 0, &bpb) != 0) {
		kprintf("ide_read_sectore error\n");
		return;
	}

	kprintf("BPB_FATz16: 0x%4h\n", bpb.common.BPB_FATz16);
	kprintf("BPB_TotSec16: 0x%4h\n", bpb.common.BPB_TotSec16);

	uint32_t RootDirSectors = ((bpb.common.BPB_RootEntCnt * 32) + (bpb.common.BPB_BytsPerSec - 1)) / bpb.common.BPB_BytsPerSec;
	uint32_t FATz;
	uint32_t TotSec;

	if (bpb.common.BPB_FATz16 != 0) {
		FATz = bpb.common.BPB_FATz16;
	} else {
		FATz = bpb.extended.fat32.BPB_FATz32;
	}

	if (bpb.common.BPB_TotSec16 != 0) {
		TotSec = bpb.common.BPB_TotSec16;
	} else {
		TotSec = bpb.common.BPB_TotSec32;
	}

	kprintf("RootDirSectors: 0x%8h\n", RootDirSectors);
	kprintf("FATz: 0x%8h\n", FATz);
	kprintf("TotSec: 0x%8h\n", TotSec);

	uint32_t DataSec = TotSec - (bpb.common.BPB_RsvdSecCnt + (bpb.common.BPB_NumFATs * FATz) + RootDirSectors);
	uint32_t CountofClusters = DataSec / bpb.common.BPB_SecPerClus;

	kprintf("DataSec: 0x%8h\n", DataSec);
	kprintf("CountofClusters: 0x%8h\n", CountofClusters);

	if (CountofClusters < 4085) {
		fat_type = FAT_TYPE_12;
		kprintf("Fat12 not supported\n");
		return;
	} else if (CountofClusters < 65525) {
		fat_type = FAT_TYPE_16;
	} else {
		fat_type = FAT_TYPE_32;
	}

	if (bpb.common.BS_jmpBoot[0] != 0xeb) {
		kprintf("jmp boot fail\n");
		return;
	} 

	if (fat_type == FAT_TYPE_12 || fat_type == FAT_TYPE_16) {
		//Check for FAT12/16
		if (bpb.extended.fat16.Signature_word != 0xAA55) {
			kprintf("sig fail\n");
			return;
		}

		if (bpb.common.BPB_RootEntCnt * 32 % bpb.common.BPB_BytsPerSec != 0) {
			kprintf("BPB_RootEntCnt fail\n");
			return;
		}
	} else {
		//Check for FAT32
		if (bpb.extended.fat32.Signature_word != 0xAA55) {
			kprintf("sig fail\n");
			return;
		}

		if (RootDirSectors != 0) {
			kprintf("root_dir_sectors fail\n");
			return;
		}

		if (bpb.common.BPB_RootEntCnt != 0) {
			kprintf("BPB_RootEntCnt fail\n");
			return;
		}
	}

	switch (fat_type) {
		case FAT_TYPE_16:
			kprintf("FAT16\n");
			break;
		case FAT_TYPE_32:
			kprintf("FAT32\n");
			break;
	}

	uint32_t FirstRotDirSecNum = bpb.common.BPB_RsvdSecCnt + (bpb.common.BPB_NumFATs * bpb.common.BPB_FATz16);
	struct fat_directory_entry fat_root_dir[16];

	kprintf("FirstRotDirSecNum: 0x%8h; BPB_RootEntCnt: 0x%8h\n", FirstRotDirSecNum, bpb.common.BPB_RootEntCnt);

	if (ide_read_sectors(drive, 1, FirstRotDirSecNum, fat_root_dir) != 0) {
		kprintf("ide_read_sectore error\n");
		return;
	}

	char buffer[512];
	for (int i = 0; i < 16; i++) {
		if (fat_root_dir[i].DIR_Name[0] == '\0') {
			kprintf("last entry\n");
			break;
		}

		if (fat_root_dir[i].DIR_Name[0] == 0xE5) {
			continue; //free space; skip
		}

		if (fat_root_dir[i].DIR_Attr == (0x01 | 0x02 | 0x04 | 0x08)) {
			kprintf("long name\n");
			continue;
		}
 
		buffer[11] = '\0';
		memcpy(buffer, fat_root_dir[i].DIR_Name, 11);
		kprintf("dir_entry: name: %s\n", buffer);

		uint32_t data_cluster = (fat_root_dir[i].DIR_FstClusHI << 16) | fat_root_dir[i].DIR_FstClusLO;
		uint32_t first_data_sector = bpb.common.BPB_RsvdSecCnt + (bpb.common.BPB_NumFATs * FATz) + RootDirSectors;
		uint32_t sector = ((data_cluster - 2) * bpb.common.BPB_SecPerClus) + first_data_sector;

		kprintf("sector: 0x%8h\n", sector);
		if (ide_read_sectors(drive, 1, sector, buffer) != 0) {
			kprintf("ide_read_sectore error\n");
			return;
		}

		buffer[fat_root_dir[i].DIR_FileSize] = '\0';

		kprintf("file content: %s\n", buffer);
	}
}
