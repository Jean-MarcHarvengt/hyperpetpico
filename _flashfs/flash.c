#include "flash.h"
#include "floppyimage.h"

void flash_fat_initialize(void) {
}

bool flash_fat_read(int block, uint8_t *buffer) {
    const uint8_t *data = (uint8_t *)&floppyimage[FAT_BLOCK_SIZE * block];
    
    memcpy(buffer, data, FAT_BLOCK_SIZE);
    return true;
}

bool flash_fat_write(int block, uint8_t *buffer) {
    return true;
}
