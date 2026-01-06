#include "diskio.h"
#include "sd_diskio.h"

// Wrapper functions to link FatFS middleware to our driver

DSTATUS disk_status(BYTE pdrv) {
    return SD_Status(pdrv);
}

DSTATUS disk_initialize(BYTE pdrv) {
    return SD_Initialize(pdrv);
}

DRESULT disk_read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count) {
    return SD_Read(pdrv, buff, sector, count);
}

DRESULT disk_write(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count) {
    return SD_Write(pdrv, buff, sector, count);
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void* buff) {
    return SD_Ioctl(pdrv, cmd, buff);
}

// Stub for get_fattime (required if FF_FS_NORTC = 0, but it is 0 by default)
DWORD get_fattime(void) {
    return 0; // Fixed time or hook into RTC later
}
