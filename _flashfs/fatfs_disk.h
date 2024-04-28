#ifndef __FATFS_DISK_H__
#define __FATFS_DISK_H__

#include <stdbool.h>

#define HW_FLASH_STORAGE_BASE  (0x14c000)
#define SECTOR_NUM 2880  // 1.44 MB floppy = 0x168000
#define SECTOR_NUM 1440  // 720 KB floppy = 0xb4000
#define SECTOR_SIZE 512

#ifdef __cplusplus
extern "C"
{
#endif

bool mount_fatfs_disk();
uint32_t fatfs_disk_read(uint8_t* buff, uint32_t sector, uint32_t count);
uint32_t fatfs_disk_write(const uint8_t* buff, uint32_t sector, uint32_t count);
void fatfs_disk_sync();
#ifdef __cplusplus
}
#endif
#endif