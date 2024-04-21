#ifndef _FLASH_H_
#define _FLASH_H_

#include <pico/stdlib.h>

#define FAT_BLOCK_NUM          2880  // 1.44 MB floppy
#define FAT_BLOCK_SIZE         512

void flash_fat_initialize(void);
bool flash_fat_read(int block, uint8_t *buffer);
bool flash_fat_write(int block, uint8_t *buffer);

#endif
