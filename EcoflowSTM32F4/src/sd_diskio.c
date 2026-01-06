#include "sd_diskio.h"
#include <string.h>

extern void Error_Handler(void);

static SD_HandleTypeDef hsd;
static volatile DSTATUS Stat = STA_NOINIT;

/**
  * @brief  Initializes the SD card.
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */
DSTATUS SD_Initialize(BYTE pdrv)
{
    if (pdrv > 0) return STA_NOINIT;

    if (Stat != STA_NOINIT) {
        return Stat; // Already initialized
    }

    /* Configure GPIOs for SDIO
       PC8: D0, PC9: D1, PC10: D2, PC11: D3, PC12: CK, PD2: CMD
    */
    __HAL_RCC_SDIO_CLK_ENABLE();
    __HAL_RCC_DMA2_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // PC8..PC12
    GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_SDIO;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    // PD2
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_SDIO;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    hsd.Instance = SDIO;
    hsd.Init.ClockEdge = SDIO_CLOCK_EDGE_RISING;
    hsd.Init.ClockBypass = SDIO_CLOCK_BYPASS_DISABLE;
    hsd.Init.ClockPowerSave = SDIO_CLOCK_POWER_SAVE_DISABLE;
    hsd.Init.BusWide = SDIO_BUS_WIDE_1B; // Start with 1-bit
    hsd.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
    hsd.Init.ClockDiv = 0; // Max Speed

    if (HAL_SD_Init(&hsd) != HAL_OK) {
        Stat = STA_NOINIT;
        return Stat;
    }

    if (HAL_SD_ConfigWideBusOperation(&hsd, SDIO_BUS_WIDE_4B) != HAL_OK) {
        Stat = STA_NOINIT;
        return Stat;
    }

    Stat = 0;
    return Stat;
}

/**
  * @brief  Gets Disk Status
  * @param  pdrv: Physical drive number (0..)
  * @retval DSTATUS: Operation status
  */
DSTATUS SD_Status(BYTE pdrv)
{
    if (pdrv > 0) return STA_NOINIT;
    return Stat;
}

/**
  * @brief  Reads Sector(s)
  * @param  pdrv: Physical drive number (0..)
  * @param  buff: Data buffer to store read data
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to read (1..128)
  * @retval DRESULT: Operation result
  */
DRESULT SD_Read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count)
{
    if (pdrv > 0 || count == 0) return RES_PARERR;
    if (Stat & STA_NOINIT) return RES_NOTRDY;

    // Use blocking read (timeout based on count)
    // 100ms per sector is generous
    if (HAL_SD_ReadBlocks(&hsd, (uint8_t*)buff, sector, count, 1000 * count) != HAL_OK) {
        return RES_ERROR;
    }

    return RES_OK;
}

/**
  * @brief  Writes Sector(s)
  * @param  pdrv: Physical drive number (0..)
  * @param  buff: Data to be written
  * @param  sector: Sector address (LBA)
  * @param  count: Number of sectors to write (1..128)
  * @retval DRESULT: Operation result
  */
DRESULT SD_Write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count)
{
    if (pdrv > 0 || count == 0) return RES_PARERR;
    if (Stat & STA_NOINIT) return RES_NOTRDY;

    if (HAL_SD_WriteBlocks(&hsd, (uint8_t*)buff, sector, count, 1000 * count) != HAL_OK) {
        return RES_ERROR;
    }

    return RES_OK;
}

/**
  * @brief  I/O Control Functions
  * @param  pdrv: Physical drive number (0..)
  * @param  cmd: Control code
  * @param  buff: Buffer to send/receive control data
  * @retval DRESULT: Operation result
  */
DRESULT SD_Ioctl(BYTE pdrv, BYTE cmd, void *buff)
{
    DRESULT res = RES_ERROR;
    HAL_SD_CardInfoTypeDef CardInfo;

    if (pdrv > 0) return RES_PARERR;
    if (Stat & STA_NOINIT) return RES_NOTRDY;

    switch (cmd) {
        case CTRL_SYNC:
            res = RES_OK;
            break;
        case GET_SECTOR_COUNT:
            HAL_SD_GetCardInfo(&hsd, &CardInfo);
            *(DWORD*)buff = CardInfo.BlockNbr;
            res = RES_OK;
            break;
        case GET_SECTOR_SIZE:
            HAL_SD_GetCardInfo(&hsd, &CardInfo);
            *(WORD*)buff = CardInfo.BlockSize;
            res = RES_OK;
            break;
        case GET_BLOCK_SIZE:
            *(DWORD*)buff = 1; // Block size is 1 sector for SD cards generally
            res = RES_OK;
            break;
        default:
            res = RES_PARERR;
            break;
    }

    return res;
}
