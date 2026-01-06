#ifndef __SD_DISKIO_H
#define __SD_DISKIO_H

#include "diskio.h"
#include "stm32f4xx_hal.h"

#ifdef __cplusplus
extern "C" {
#endif

DSTATUS SD_Initialize(BYTE pdrv);
DSTATUS SD_Status(BYTE pdrv);
DRESULT SD_Read(BYTE pdrv, BYTE* buff, DWORD sector, UINT count);
DRESULT SD_Write(BYTE pdrv, const BYTE* buff, DWORD sector, UINT count);
DRESULT SD_Ioctl(BYTE pdrv, BYTE cmd, void* buff);

#ifdef __cplusplus
}
#endif

#endif
