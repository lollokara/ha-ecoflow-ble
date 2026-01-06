#include "diskio.h"
#include "sd_driver.h"

DSTATUS disk_status(BYTE pdrv) {
    if (pdrv != 0) return STA_NOINIT;
    return SD_GetStatus();
}

DSTATUS disk_initialize(BYTE pdrv) {
    if (pdrv != 0) return STA_NOINIT;
    return SD_Initialize();
}

DRESULT disk_read(BYTE pdrv, BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv != 0) return RES_PARERR;
    return SD_Read(buff, sector, count);
}

DRESULT disk_write(BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count) {
    if (pdrv != 0) return RES_PARERR;
    return SD_Write(buff, sector, count);
}

DRESULT disk_ioctl(BYTE pdrv, BYTE cmd, void *buff) {
    if (pdrv != 0) return RES_PARERR;
    return SD_Ioctl(cmd, buff);
}

DWORD get_fattime(void) {
    return 0; // Return current time if available
}
