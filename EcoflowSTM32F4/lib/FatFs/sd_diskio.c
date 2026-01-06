#include "sd_diskio.h"
#include "stm32f4xx_hal.h"

#define SD_TIMEOUT 30000 // 30s timeout

extern SD_HandleTypeDef hsd;

static volatile DSTATUS Stat = STA_NOINIT;

static DSTATUS SD_check_status(BYTE lun);
static DSTATUS SD_initialize(BYTE lun);
static DSTATUS SD_status(BYTE lun);
static DRESULT SD_read(BYTE lun, BYTE *buff, LBA_t sector, UINT count);
static DRESULT SD_write(BYTE lun, const BYTE *buff, LBA_t sector, UINT count);
static DRESULT SD_ioctl(BYTE lun, BYTE cmd, void *buff);

const Diskio_drvTypeDef SD_Driver =
{
  SD_initialize,
  SD_status,
  SD_read,
  SD_write,
  SD_ioctl
};

static DSTATUS SD_check_status(BYTE lun)
{
  Stat = STA_NOINIT;
  if (HAL_SD_GetCardState(&hsd) == HAL_SD_CARD_TRANSFER)
  {
    Stat &= ~STA_NOINIT;
  }
  return Stat;
}

static DSTATUS SD_initialize(BYTE lun)
{
  // We assume HAL_SD_Init is called in main.c
  return SD_check_status(lun);
}

static DSTATUS SD_status(BYTE lun)
{
  return SD_check_status(lun);
}

static DRESULT SD_read(BYTE lun, BYTE *buff, LBA_t sector, UINT count)
{
  DRESULT res = RES_ERROR;
  if (HAL_SD_ReadBlocks(&hsd, (uint8_t*)buff, sector, count, SD_TIMEOUT) == HAL_OK)
  {
    while (HAL_SD_GetCardState(&hsd) != HAL_SD_CARD_TRANSFER) {}
    res = RES_OK;
  }
  return res;
}

static DRESULT SD_write(BYTE lun, const BYTE *buff, LBA_t sector, UINT count)
{
  DRESULT res = RES_ERROR;
  if (HAL_SD_WriteBlocks(&hsd, (uint8_t*)buff, sector, count, SD_TIMEOUT) == HAL_OK)
  {
    while (HAL_SD_GetCardState(&hsd) != HAL_SD_CARD_TRANSFER) {}
    res = RES_OK;
  }
  return res;
}

static DRESULT SD_ioctl(BYTE lun, BYTE cmd, void *buff)
{
  DRESULT res = RES_ERROR;
  HAL_SD_CardInfoTypeDef CardInfo;

  if (Stat & STA_NOINIT) return RES_NOTRDY;

  switch (cmd)
  {
     case CTRL_SYNC:
       res = RES_OK;
       break;

     case GET_SECTOR_COUNT:
       HAL_SD_GetCardInfo(&hsd, &CardInfo);
       *(DWORD*)buff = CardInfo.LogBlockNbr;
       res = RES_OK;
       break;

     case GET_SECTOR_SIZE:
       HAL_SD_GetCardInfo(&hsd, &CardInfo);
       *(WORD*)buff = CardInfo.LogBlockSize;
       res = RES_OK;
       break;

     case GET_BLOCK_SIZE:
       HAL_SD_GetCardInfo(&hsd, &CardInfo);
       *(DWORD*)buff = CardInfo.LogBlockSize / 512;
       res = RES_OK;
       break;

     default:
       res = RES_PARERR;
  }
  return res;
}
