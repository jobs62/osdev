#include "stdtype.h"
#include "ata.h"
#include "fat.h"

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
