#ifndef __FAT__
#define __FAT__

#include <stdint.h>
#include "stdlib.h"
#include "bdev.h"

#define FAT_TYPE_12 1
#define FAT_TYPE_16 2
#define FAT_TYPE_32 3
#define FAT_TYPE_NONE 0

struct fat_fs {
	uint8_t device;
	uint8_t fat_type;
	uint32_t fat_first_sector_fat;
	uint32_t fat_fat_size; //Warning: this size is in sectors not in bytes!
	uint32_t fat_first_sector_data;
	uint32_t fat_data_size;
	uint32_t fat_root_dir_sector;
	uint32_t fat_root_dir_size;
	uint32_t fat_cluster_size; //Warning: this size is in sectors not in bytes!
};

struct fat_sector_itearator {
	struct fat_fs *fat;
	uint32_t current_sector;
	uint32_t current_cluster;
	uint32_t reminding_size;
    uint8_t eoi;
};

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

struct fat_directory_iterator{
	struct fat_sector_itearator sec_iter;
	struct fat_directory_entry direntry[16];
	struct fat_directory_entry tmp;
	uint16_t i;
	uint8_t eoi;
};

struct file {
	struct fat_sector_itearator inital_iter;
	struct fat_sector_itearator iter;
	uint32_t offset;
};

#define SEEK_SET 0
#define SEEK_CUR 1

enum bdev_payload_status fat_init(uint8_t drive);
uint32_t fat_sector_iterator_next(struct fat_sector_itearator *iter);
uint32_t fat_sector_iterator_root_dir(struct fat_sector_itearator *iter, struct fat_fs *fat);
void fat_directory_iterator_root_dir(struct fat_directory_iterator *iter, struct fat_fs *fat);
void fat_directory_iterator(struct fat_directory_iterator *iter, struct fat_directory_entry *dir, struct fat_fs *fat);
struct fat_directory_entry *fat_directory_iterator_next(struct fat_directory_iterator *iter);
void fat_sector_itearator(struct fat_sector_itearator *iter, struct fat_directory_entry *dir, struct fat_fs *fat);
int fat_open_from_path(struct fat_sector_itearator *iter, char *path[]);
int fat_read(struct file *file, void *buffer, uint32_t size);
int fat_seek(struct file *file, uint32_t offset, uint8_t whence);
int fat_open(struct file *file, struct fat_sector_itearator *iter);

inline void fat_sector_itearator_copy(struct fat_sector_itearator *dst, struct fat_sector_itearator *src) {
	memcpy(dst, src, sizeof(struct fat_sector_itearator));
}
  
inline void fat_directory_itearator_copy(struct fat_directory_iterator *dst, struct fat_directory_iterator *src) {
	memcpy(dst, src, sizeof(struct fat_directory_iterator));
}

#endif