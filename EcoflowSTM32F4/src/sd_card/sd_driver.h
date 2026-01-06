#ifndef SD_DRIVER_H
#define SD_DRIVER_H

#include "stm32f4xx_hal.h"
#include "diskio.h"

DSTATUS SD_Initialize(void);
DSTATUS SD_GetStatus(void);
DRESULT SD_Read(BYTE *buff, LBA_t sector, UINT count);
DRESULT SD_Write(const BYTE *buff, LBA_t sector, UINT count);
DRESULT SD_Ioctl(BYTE cmd, void *buff);

void MX_SDIO_SD_Init(void);

#endif
