
#include "ff.h"
#include "diskio.h"
#include "fatfs_disk.h"
#include "hardware/flash.h"



#define MAGIC_8_BYTES "RHE!FS30"

#define NUM_FAT_SECTORS  (SECTOR_NUM)//30716   // 15megs / 512bytes = 30720, but we used 4 records for the header (8 bytes)
#define NUM_FLASH_SECTORS ((SECTOR_NUM*SECTOR_SIZE)/4096)//3840  // 15megs / 4096bytes = 3840
 
typedef struct {
    uint8_t header[8];
    uint16_t sectors[NUM_FAT_SECTORS];  // map FAT sectors -> flash sectors
} sector_map;

static sector_map fs_map;
static bool flashfs_is_mounted = false;

// each sector entry in the sector map is:
//  13 bits of sector (indexing 8192 4k flash sectors)
//   3 bits of offset (0->7 512 byte FAT sectors in each 4k flash sector)
static uint16_t getMapSector(uint16_t mapEntry) { return (mapEntry & 0xFFF8) >> 3; }
static uint8_t getMapOffset(uint16_t mapEntry) { return mapEntry & 0x7; }
static uint16_t makeMapEntry(uint16_t sector, uint8_t offset) { return (sector << 3) | offset; };

static void flash_read_sector(uint16_t sector, uint8_t offset, void *buffer, uint16_t size)
{
    uint32_t fs_start = XIP_BASE + HW_FLASH_STORAGE_BASE;
    uint32_t addr = fs_start + (sector * FLASH_SECTOR_SIZE) + (offset * 512);   
    memcpy(buffer, (unsigned char *)addr, size);
}

static void flash_fs_read_FAT_sector(uint16_t fat_sector, void *buffer)
{
    int mapEntry = fs_map.sectors[fat_sector];
    if (mapEntry)
        flash_read_sector(getMapSector(mapEntry), getMapOffset(mapEntry), buffer, 512);
    else
        memset(buffer, 0, 512);
    return;
}

bool mount_fatfs_disk()
{
    // read the first sector, with header
    flash_read_sector(0, 0, &fs_map, sizeof(fs_map));

    if (memcmp(fs_map.header, MAGIC_8_BYTES, 8) != 0) {
        flashfs_is_mounted = false;
        return false;
    }
    // read the remaining 14 sectors without headers
    //for (int i=1; i<15; i++)
    //    flash_read_sector(i, 0, (uint8_t*)&fs_map+(4096*i), 4096);

    flashfs_is_mounted = true;
    return true;
}

uint32_t fatfs_disk_read(uint8_t* buff, uint32_t sector, uint32_t count)
{	
//  printf("fatfs_disk_read sector=%d, count=%d\n", sector, count);
    if (!flashfs_is_mounted) return RES_ERROR;
    if (sector < 0 || sector >= SECTOR_NUM)
            return RES_PARERR;

    /* copy data to buffer */
    for (int i=0; i<count; i++)
        flash_fs_read_FAT_sector(sector + i, buff + (i*SECTOR_SIZE));
    return RES_OK;

}

uint32_t fatfs_disk_write(const uint8_t* buff, uint32_t sector, uint32_t count)
{
    return RES_OK;
}

void fatfs_disk_sync()
{
}