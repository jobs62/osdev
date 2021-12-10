#ifndef __MBR__
#define __MBR__

#include "stdtype.h"
#include "bdev.h"

enum bdev_payload_status mbr_init(uint8_t drive);

#endif