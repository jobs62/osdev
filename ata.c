#include "stdtype.h"
#include "pci.h"
#include "ata.h"
#include "io.h"
#include "fat.h"
#include "vmm.h"
#include "pmm.h"
#include "interrupt.h"

struct pdr {
	uint32_t base_address;
	uint16_t size;
	uint32_t endmark;
} __attribute__((packed));

struct IDEChannelRegisters {
	struct pdr pdrt[1];
	unsigned short base;  // I/O Base.
	unsigned short ctrl;  // Control Base
	unsigned short bmide; // Bus Master IDE
	unsigned char nIEN;   // nIEN (No Interrupt);
	uint8_t bus, slot, fonc;
} __attribute__((aligned(64))) channels[2];

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
	unsigned short udma;
} ide_devices[4];


static unsigned char ide_read(unsigned char channel, unsigned char reg);
static void ide_write(unsigned char channel, unsigned char reg, unsigned char data);
static void ide_read_buffer(unsigned char channel, unsigned char reg, unsigned int buffer, unsigned int quads);
static uint8_t ide_polling(uint8_t channel, uint8_t advanced_check);
static uint8_t ide_ata_access(uint8_t direction, uint8_t drive, uint32_t lba, uint8_t numsects, void *edi);


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

#define ATA_BMR_COMMAND (0x00 + 0x0E)
#define ATA_BMR_STATUS (0x02 + 0x0E)
#define ATA_BMR_PRDT (0x04 + 0x0E)

#define ATA_IDENT_DEVICETYPE 0
#define ATA_IDENT_CYLINDERS 2
#define ATA_IDENT_HEADS 6
#define ATA_IDENT_SECTORS 12
#define ATA_IDENT_SERIAL 20
#define ATA_IDENT_MODEL 54
#define ATA_IDENT_UDMA 88
#define ATA_IDENT_CABLE 93
#define ATA_IDENT_CAPABILITIES 98
#define ATA_IDENT_FIELDVALID 106
#define ATA_IDENT_MAX_LBA 120
#define ATA_IDENT_COMMANDSETS 164
#define ATA_IDENT_MAX_LBA_EXT 200

unsigned char ide_buf[2048] = {0};
unsigned static char ide_irq_invoked = 0;
unsigned static char atapi_packet[12] = {0xA8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};

extern void sleep(unsigned int t);

void ide_int(unsigned int intno, void *ext);

void ata_init(struct pci_header *head, uint8_t bus, uint8_t slot, uint8_t fonc) {
	uint32_t bar0, bar1, bar2, bar3, bar4;

	kprintf("prog_if: 0x%4h; status: 0x%4h; bar4: 0x%8h\n", head->prog_if, head->status, head->specific.type0.bar4);

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

	bar4 = head->specific.type0.bar4;

	// 1- Detect I/O Ports which interface IDE Controller:
	channels[ATA_PRIMARY].base = bar0 & 0xFFFFFFFC;
	channels[ATA_PRIMARY].ctrl = bar1 & 0xFFFFFFFC;
	channels[ATA_SECONDARY].base = bar2 & 0xFFFFFFFC;
	channels[ATA_SECONDARY].ctrl = bar3 & 0xFFFFFFFC;
	channels[ATA_PRIMARY].bmide = (bar4 & 0xFFFFFFFC) + 0;   // Bus Master IDE
	channels[ATA_SECONDARY].bmide = (bar4 & 0xFFFFFFFC) + 8; // Bus Master IDE
	channels[ATA_PRIMARY].bus = channels[ATA_SECONDARY].bus = bus;
	channels[ATA_PRIMARY].slot = channels[ATA_SECONDARY].slot = fonc;
	channels[ATA_PRIMARY].fonc = channels[ATA_SECONDARY].fonc = fonc;

	// 2- Disable IRQs:
	//ide_write(ATA_PRIMARY, ATA_REG_CONTROL, 2);
	//ide_write(ATA_SECONDARY, ATA_REG_CONTROL, 2);
	register_interrupt(0x2e, ide_int, 0);
	register_interrupt(0x2f, ide_int, 0);

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
			ide_devices[count].udma = *((unsigned short *)(ide_buf + ATA_IDENT_UDMA));

			kprintf("cable: 0x%4h\n", *((unsigned short *)(ide_buf + ATA_IDENT_CABLE)));
			/*
			 * ok, so the cable emulated by qemu seam to not be 80pins, wich should indicate that using udma > 2 is not a great idea
			 * but udma 6 is enable on the drive so... maybe that why dma is acting wired ? but does is realy matter in a vm ?
			 * i need to invastigate that crap...
			 * for referance heree, cable 0x1020 and ubma: 0x2020
			 */

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
			kprintf(" Found %s Drive %1dGB - %s\n", (const char *[]){"ATA", "ATAPI"}[ide_devices[i].Type],
				  ide_devices[i].Size / 1024 / 1024 / 2, ide_devices[i].Model);
			kprintf("  Capabilites: 0x%4h; CommandSet: 0x%8h; UDMA: 0x%4h\n", ide_devices[i].Capabilities, ide_devices[i].CommandSets, ide_devices[i].udma);

			if (ide_devices[i].Type == 0) {
				fat_init(i);
			}
		}
}

static unsigned char ide_read(unsigned char channel, unsigned char reg) {
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

static void ide_write(unsigned char channel, unsigned char reg, unsigned char data) {
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

static void ide_read_buffer(unsigned char channel, unsigned char reg, unsigned int buffer, unsigned int quads) {
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

static uint8_t ide_ata_access(uint8_t direction, uint8_t drive, uint32_t lba, uint8_t numsects, void *edi) {
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
   	dma = 0; //by default use PIO
	//if (channels[channel].bmide != channel * 8 && (get_physaddr(edi) % 4) == 0)  {
	//	dma = 1; //enable dma if edi is sutable
	//	// i guess i could also alloc and memcpy, but were is the fun ?
	//	//TODO check 64k crossing bondaries
	//}
	
   // (III) Wait if the drive is busy;
   while (ide_read(channel, ATA_REG_STATUS) & ATA_SR_BSY) {}

	// If DMA is enable, prepare pdrt and bus master register
	if (dma) {
		channels[channel].pdrt[0].base_address = get_physaddr(edi) & ~0x1;
		channels[channel].pdrt[0].size = (512 * numsects) & 0xfffd; //TODO hcek 64k limit
		channels[channel].pdrt[0].endmark = 0x8000;

		kprintf("pdrt addr: 0x%8h\n", get_physaddr(channels[channel].pdrt));
		kprintf("pdrt[0] 0x%8h 0x%8h\n", ((uint32_t *)channels[channel].pdrt)[0], ((uint32_t *)channels[channel].pdrt)[1]);
		outl(channels[channel].bmide + ATA_BMR_PRDT - 0x0E, get_physaddr(channels[channel].pdrt));

		ide_write(channel, ATA_BMR_COMMAND, 0x0);
		ide_write(channel, ATA_BMR_STATUS, 0x6);
	}

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
	
	if (dma == 1) {
		if (direction == ATA_READ) {
			ide_write(channel, ATA_REG_COMMAND, cmd);
			ide_write(channel, ATA_BMR_COMMAND, 0x1);

			if ((err = ide_polling(channel, 1)) != 0) {
					return err;
			}
		}
	} else {
		if (direction == ATA_READ) {
			ide_write(channel, ATA_REG_COMMAND, cmd);
			//Read
			for (i = 0; i < numsects; i++) {
				if ((err = ide_polling(channel, 2)) != 0) {
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

static uint8_t ide_polling(uint8_t channel, uint8_t advanced_check) {
	static uint8_t old_ide_irq_invoked = 0;

	while (ide_irq_invoked == old_ide_irq_invoked) {
		asm volatile("hlt");
	}
	old_ide_irq_invoked = ide_irq_invoked;

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
		if ((advanced_check & 0x2) && (state & ATA_SR_DRQ) == 0) {
			return 3; // DRQ should be set
		}
	}
 
   return 0; // No Error.
}


uint8_t ide_read_sectors(uint8_t drive, uint8_t numsects, uint32_t lba, void *edi) {
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

void ide_int(unsigned int intno, void *ext) {
	ide_irq_invoked++;

	if (intno == 0x2e) {
		uint16_t pci_status = pci_config_read(channels[ATA_PRIMARY].bus, channels[ATA_PRIMARY].slot, channels[ATA_PRIMARY].fonc, 0x4) >> 16;
		kprintf("primary ide master bus status: 0x%2h; pci status: 0x%4h\n", ide_read(ATA_PRIMARY, ATA_BMR_STATUS), pci_status);
		kprintf("ide status: 0x%2h; ide error: 0x%2h\n", ide_read(ATA_PRIMARY, ATA_REG_STATUS), ide_read(ATA_PRIMARY, ATA_REG_ERROR));
		ide_write(ATA_PRIMARY, ATA_BMR_STATUS, 0x4);
	} else {
		//kprintf("secodary ide master bus status: 0x%2h;\n", ide_read(ATA_SECONDARY, ATA_BMR_STATUS));
		ide_write(ATA_SECONDARY, ATA_BMR_STATUS, 0x4);
	}
}