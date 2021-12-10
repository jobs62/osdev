#include "stdtype.h"
#include "bdev.h"
#include "fat.h"
#include "stdlib.h"

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



struct fat_fs fs;

enum bdev_payload_status fat_init(uint8_t drive) {
	struct fat_bpb bpb;
	fs.fat_type = FAT_TYPE_NONE;
	uint8_t err;

	kprintf("fat init drive %1d\n", drive);
	fs.device = drive;

	if ((err = bdev_read(drive, 1, 0, &bpb)) != 0) {
		kprintf("ide_read_sectore error (%d)\n", err);
		return BDEV_ERROR;
	}

	if (bpb.common.BPB_BytsPerSec == 0) {
		kprintf("BPB_BytsPerSec errror\n");
		goto involid_fat_header;
	}

	fs.fat_root_dir_size = ((bpb.common.BPB_RootEntCnt * 32) + (bpb.common.BPB_BytsPerSec - 1)) / bpb.common.BPB_BytsPerSec;
	uint32_t TotSec;

	if (bpb.common.BPB_FATz16 != 0) {
		fs.fat_fat_size = bpb.common.BPB_FATz16;
	} else {
		fs.fat_fat_size = bpb.extended.fat32.BPB_FATz32;
	}

	if (bpb.common.BPB_TotSec16 != 0) {
		TotSec = bpb.common.BPB_TotSec16;
	} else {
		TotSec = bpb.common.BPB_TotSec32;
	}

	fs.fat_first_sector_fat = bpb.common.BPB_RsvdSecCnt;
	fs.fat_first_sector_data = (fs.fat_first_sector_fat + (bpb.common.BPB_NumFATs * fs.fat_fat_size) + fs.fat_root_dir_size);
	uint32_t DataSec = TotSec - fs.fat_first_sector_data;
	uint32_t CountofClusters = DataSec / bpb.common.BPB_SecPerClus;
	fs.fat_data_size = DataSec * bpb.common.BPB_BytsPerSec;
	fs.fat_cluster_size = bpb.common.BPB_SecPerClus;

	if (CountofClusters < 4085) {
		fs.fat_type = FAT_TYPE_12;
	} else if (CountofClusters < 65525) {
		fs.fat_type  = FAT_TYPE_16;
	} else {
		fs.fat_type  = FAT_TYPE_32;
	}

	if (bpb.common.BS_jmpBoot[0] != 0xeb) {
		kprintf("jmp boot fail\n");
		fs.fat_type = FAT_TYPE_NONE;
	} 

	if (fs.fat_type  == FAT_TYPE_12 || fs.fat_type  == FAT_TYPE_16) {
		//Check for FAT12/16
		if (bpb.extended.fat16.Signature_word != 0xAA55) {
			kprintf("sig fail\n");
			fs.fat_type = FAT_TYPE_NONE;
		}

		if (bpb.common.BPB_RootEntCnt * 32 % bpb.common.BPB_BytsPerSec != 0) {
			kprintf("BPB_RootEntCnt fail\n");
			fs.fat_type = FAT_TYPE_NONE;
		}
	} else {
		//Check for FAT32
		if (bpb.extended.fat32.Signature_word != 0xAA55) {
			kprintf("sig fail\n");
			fs.fat_type = FAT_TYPE_NONE;
		}

		if (fs.fat_root_dir_size != 0) {
			kprintf("root_dir_sectors fail\n");
			fs.fat_type = FAT_TYPE_NONE;
		}

		if (bpb.common.BPB_RootEntCnt != 0) {
			kprintf("BPB_RootEntCnt fail\n");
			fs.fat_type = FAT_TYPE_NONE;
		}
	}

	switch (fs.fat_type) {
		case FAT_TYPE_NONE:
involid_fat_header:
			kprintf("Invalid FAT partition\n");
			return BDEV_FORWARD;
		case FAT_TYPE_12:
			kprintf("FAT12 Not supported\n");
			fs.fat_type = FAT_TYPE_NONE;
			return BDEV_ERROR;
		case FAT_TYPE_16:
			kprintf("FAT16\n");
			break;
		case FAT_TYPE_32:
			kprintf("FAT32\n");
			break;
	}

	switch(fs.fat_type) {
		case FAT_TYPE_12:
		case FAT_TYPE_16:
			fs.fat_root_dir_sector = bpb.common.BPB_RsvdSecCnt + (bpb.common.BPB_NumFATs * bpb.common.BPB_FATz16);
			fs.fat_root_dir_size = bpb.common.BPB_RootEntCnt * 32;
			break;
		case FAT_TYPE_32:
			fs.fat_root_dir_sector = fs.fat_first_sector_data + (bpb.extended.fat32.BPB_RootClus - 2) * bpb.common.BPB_SecPerClus;
			fs.fat_root_dir_size = 0; //Size of root dir in illimited, and is only limited by FAT EoF
			break;
	}

	kprintf("bpb.common.BPB_RsvdSecCnt: 0x%8h\n", bpb.common.BPB_RsvdSecCnt);
	kprintf("bpb.common.BPB_NumFATs: 0x%2h\n", bpb.common.BPB_NumFATs);
	kprintf("bpb.common.BPB_FATz16: 0x%8h\n", bpb.common.BPB_FATz16);
	kprintf("root dir sector : 0x%8h\n", fs.fat_root_dir_sector);

	return BDEV_SUCCESS;
}

uint32_t fat_sector_iterator_root_dir(struct fat_sector_itearator *iter, struct fat_fs *fat) {
	iter->fat = fat;
	iter->current_sector = iter->fat->fat_root_dir_sector;
	iter->eoi = 0;

	switch(fat->fat_type) {
		case FAT_TYPE_12:
		case FAT_TYPE_16:
			iter->reminding_size = iter->fat->fat_root_dir_size;
			iter->current_cluster = 0;
			break;
		case FAT_TYPE_32:
			iter->reminding_size = 0;
			iter->current_cluster = ((iter->fat->fat_root_dir_sector - iter->fat->fat_first_sector_data) / iter->fat->fat_cluster_size) + 2;
			break;
	}

	return (0);
}

void fat_sector_itearator(struct fat_sector_itearator *iter, struct fat_directory_entry *dir, struct fat_fs *fat) {
	iter->fat = fat;
	iter->eoi = 0;
	iter->current_cluster = (dir->DIR_FstClusHI << 16) | dir->DIR_FstClusLO;
	iter->current_sector = iter->fat->fat_first_sector_data + (iter->current_cluster - 2) * iter->fat->fat_cluster_size;
	iter->reminding_size = dir->DIR_FileSize;
}

uint32_t fat_sector_iterator_next(struct fat_sector_itearator *iter) {
	uint8_t buffer[512];
	uint32_t err;

	uint32_t sector = iter->current_sector;
	if (iter->eoi == 1) {
		return (0); //i need to find a better idea to deal with errors
	}

	iter->current_sector++;
	if (iter->reminding_size > 0) {
		if (iter->reminding_size > 512) {
			iter->reminding_size -= 512;
		} else {
			iter->reminding_size = 0;
			iter->eoi = 1;
		}
	}

	if (iter->current_cluster != 0 && (iter->current_sector - iter->fat->fat_first_sector_data) % iter->fat->fat_cluster_size == 0) {
		uint32_t fatoffset = iter->current_cluster * 4;
		uint32_t ThisFATSecNum, ThisFATSecOffset;

		switch(iter->fat->fat_type) {
			case FAT_TYPE_16:
				fatoffset = iter->current_cluster * 2;
			case FAT_TYPE_32:
				ThisFATSecNum = iter->fat->fat_first_sector_fat + fatoffset / 512;
				ThisFATSecOffset = fatoffset % 512;
				break;
			case FAT_TYPE_12:
				fatoffset = iter->current_cluster + (iter->current_cluster / 2);
				ThisFATSecNum = iter->fat->fat_first_sector_fat + fatoffset / 512;
				ThisFATSecOffset = fatoffset % 512;
				break;
		}

		if ((err = bdev_read(iter->fat->device, 1, ThisFATSecNum, buffer)) != 0) {
			kprintf("ide_read_sectore error (%d)\n", err);
			iter->eoi = 1;
		}

		uint32_t fat12ClusEntryVal;
		switch (iter->fat->fat_type) {
			case FAT_TYPE_16:
				iter->current_cluster = *((uint16_t *)&buffer[ThisFATSecOffset]);
				break;
			case FAT_TYPE_32:
				iter->current_cluster = (*((uint32_t *)&buffer[ThisFATSecOffset])) & 0x0FFFFFFF;
				break;
			case FAT_TYPE_12:
				fat12ClusEntryVal = *((uint16_t *)&buffer[ThisFATSecOffset]);
				if (iter->current_cluster & 0x0001) {
					fat12ClusEntryVal = fat12ClusEntryVal >> 4;
				} else {
					fat12ClusEntryVal = fat12ClusEntryVal & 0x0FFF;
				}
				iter->current_cluster = fat12ClusEntryVal;
				break;
		}

		if (iter->fat->fat_type == FAT_TYPE_12 && iter->current_cluster >= 0xFF7) {
			iter->current_cluster = 0;
			iter->eoi = 1;
		}

		if (iter->fat->fat_type == FAT_TYPE_16 && iter->current_cluster >= 0xFFF7) {
			iter->current_cluster = 0;
			iter->eoi = 1;
		}

		if (iter->fat->fat_type == FAT_TYPE_32 && iter->current_cluster >= 0x0FFFFFF7) {
			iter->current_cluster = 0;
			iter->eoi = 1;
		}

		if (iter->current_cluster != 0) {
			iter->current_sector = iter->fat->fat_first_sector_data + (iter->current_cluster - 2) * iter->fat->fat_cluster_size;
		}
	}

	return sector;
}

void fat_directory_iterator_root_dir(struct fat_directory_iterator *iter, struct fat_fs *fat) {
	uint32_t sec;
	uint8_t err;

	fat_sector_iterator_root_dir(&iter->sec_iter, fat);
	iter->i = 0;

	sec = fat_sector_iterator_next(&iter->sec_iter);
	if (sec == 0) {
		kprintf("fat_sector_iterator_next error\n");
		iter->eoi = 1;
	} else if ((err = bdev_read(iter->sec_iter.fat->device, 1, sec, iter->direntry)) != 0) {
		kprintf("fat_sector_iterator_next error (%d)\n", err);
		iter->eoi = 1;
	} else {
		iter->eoi = 0;
	}
}

void fat_directory_iterator(struct fat_directory_iterator *iter, struct fat_directory_entry *dir, struct fat_fs *fat) {
	uint32_t sec;
	
	if ((dir->DIR_Attr & 0x10) == 0) {
		//dir entry not a directory, wtf;
		iter->eoi = 1;
		return;
	}

	fat_sector_itearator(&iter->sec_iter, dir, fat);
	iter->i = 0;

	sec = fat_sector_iterator_next(&iter->sec_iter);
	if (sec == 0) {
		iter->eoi = 1;
	} else {
		bdev_read(iter->sec_iter.fat->device, 1, sec, iter->direntry); //Todo: manage errors
		iter->eoi = 0;
	}
}

struct fat_directory_entry *fat_directory_iterator_next(struct fat_directory_iterator *iter) {
	uint32_t sec;
	struct fat_directory_entry *fat_directory_entry;
	uint8_t err;

fetch_next_one:
	fat_directory_entry = &iter->direntry[iter->i];

	if (iter->eoi == 0) {
		iter->i++;
	} else {
		return (void *)0;
	}

	if (iter->i >= 16) {
		memcpy(&iter->tmp, fat_directory_entry, 32);
		fat_directory_entry = &iter->tmp;
		sec = fat_sector_iterator_next(&iter->sec_iter);
		if (sec == 0) {
			/* EoI */
			iter->eoi = 1;
		} else if ((err = bdev_read(iter->sec_iter.fat->device, 1, sec, iter->direntry)) != 0) {
			kprintf("error ide_read_sector (%d)\n", err);
			iter->eoi = 1;
		} else {
			iter->i = 0;
		}
	}

	if (fat_directory_entry->DIR_Name[0] == '\0') {
		iter->eoi = 1;
		return (void *)0;
	}

	if (fat_directory_entry->DIR_Name[0] == 0xE5) {
		goto fetch_next_one;
	}
	
	if ((fat_directory_entry->DIR_Attr & (0x01 | 0x02 | 0x04 | 0x08)) != 0) {
		goto fetch_next_one;
	}
	
	return fat_directory_entry;
}

int fat_name_cmp(unsigned char *left, char *rigth) {
	uint8_t buffer[11];
	char *q = rigth;

	memcpy(buffer, left, 11);
	buffer[8] = '\0';
	char *p = buffer;

	//kprintf("left: %s; right: %s\n", p, q);

	//thats a gross simlification of fat naming
	while(*p == *q) {
		p++;
		q++;
	}

	if (*p == ' ' && *q == '\0') {
		return (0);
	}

	return (1);
}

int fat_open_from_path(struct fat_sector_itearator *iter, char *path[]) {
	struct fat_directory_iterator inner;
	struct fat_directory_iterator inner2;
	struct fat_directory_entry *direntry;
	uint32_t i = 0;

	fat_directory_iterator_root_dir(&inner, &fs);
	while ((direntry = fat_directory_iterator_next(&inner)) != (void *)0) {
		if (fat_name_cmp(direntry->DIR_Name, path[i]) == 0) {
			i++;
			if (path[i] == (void *)0) {
				fat_sector_itearator(iter, direntry, &fs);
				return (0);
			} else {
				fat_directory_iterator(&inner2, direntry, &fs);
				fat_directory_itearator_copy(&inner, &inner2);
			}
		}
	}

	return (1);
}