#ifndef _FLASH_H_
#define _FLASH_H_

#include <ctype.h>
#include <math.h>
#include <hardware/flash.h>
#include <hardware/sync.h>
#include <pico/stdlib.h>
#include <stdio.h>
#include <string.h>

#define FAT_BLOCK_NUM          2880  // 1.44 MB floppy
#define FAT_BLOCK_SIZE         512

void flash_fat_initialize(void);
bool flash_fat_read(int block, uint8_t *buffer);
bool flash_fat_write(int block, uint8_t *buffer);

#endif
