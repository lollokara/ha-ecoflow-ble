#include "sd_card.h"
#include "diskio.h"
#include <string.h>
#include <stdio.h>

SD_HandleTypeDef hsd;
FATFS SDFatFS;  /* File system object for SD card logical drive */
char SDPath[4]; /* SD card logical drive path */

// HAL MSP Init for SDIO
void HAL_SD_MspInit(SD_HandleTypeDef* hsd)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(hsd->Instance==SDIO)
  {
    /* Peripheral clock enable */
    __HAL_RCC_SDIO_CLK_ENABLE();

    __HAL_RCC_GPIOC_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    /**SDIO GPIO Configuration
    PC8     ------> SDIO_D0
    PC9     ------> SDIO_D1
    PC10    ------> SDIO_D2
    PC11    ------> SDIO_D3
    PC12    ------> SDIO_CK
    PD2     ------> SDIO_CMD
    */
    GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_SDIO;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF12_SDIO;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /* SDIO interrupt Init */
    HAL_NVIC_SetPriority(SDIO_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(SDIO_IRQn);
  }
}

void HAL_SD_MspDeInit(SD_HandleTypeDef* hsd)
{
  if(hsd->Instance==SDIO)
  {
    __HAL_RCC_SDIO_CLK_DISABLE();

    HAL_GPIO_DeInit(GPIOC, GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12);
    HAL_GPIO_DeInit(GPIOD, GPIO_PIN_2);

    HAL_NVIC_DisableIRQ(SDIO_IRQn);
  }
}

void SDIO_IRQHandler(void)
{
  HAL_SD_IRQHandler(&hsd);
}

void MX_SDIO_SD_Init(void)
{
  hsd.Instance = SDIO;
  hsd.Init.ClockEdge = SDIO_CLOCK_EDGE_RISING;
  hsd.Init.ClockBypass = SDIO_CLOCK_BYPASS_DISABLE;
  hsd.Init.ClockPowerSave = SDIO_CLOCK_POWER_SAVE_DISABLE;
  hsd.Init.BusWide = SDIO_BUS_WIDE_1B; // Start with 1B, switch to 4B later
  hsd.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
  hsd.Init.ClockDiv = 0; // Max speed (48MHz / (0+2) = 24MHz? Check ref manual)

  if (HAL_SD_Init(&hsd) != HAL_OK)
  {
    // Error
  }

  if (HAL_SD_ConfigWideBusOperation(&hsd, SDIO_BUS_WIDE_4B) != HAL_OK)
  {
    // Error
  }
}

int SD_Mount(void)
{
  if (f_mount(&SDFatFS, "", 1) != FR_OK) {
      return 0;
  }
  return 1;
}

void SD_Unmount(void)
{
  f_mount(0, "", 0);
}

int SD_Format(void)
{
    BYTE work[FF_MAX_SS];
    if (f_mkfs("", 0, work, sizeof(work)) != FR_OK) return 0;
    return 1;
}

void SD_GetSpace(uint32_t *total, uint32_t *free)
{
    FATFS *fs;
    DWORD fre_clust, fre_sect, tot_sect;

    if (f_getfree("", &fre_clust, &fs) == FR_OK) {
        tot_sect = (fs->n_fatent - 2) * fs->csize;
        fre_sect = fre_clust * fs->csize;

        *total = tot_sect / 2048; // KB (assuming 512B sectors) -> MB roughly / 1024 / 2
        *free = fre_sect / 2048;
    } else {
        *total = 0;
        *free = 0;
    }
}

// -------------------------------------------------------------------------
// DiskIO Implementation
// -------------------------------------------------------------------------

DSTATUS disk_initialize (BYTE pdrv)
{
    if(HAL_SD_Init(&hsd) != HAL_OK) return STA_NOINIT;
    return 0;
}

DSTATUS disk_status (BYTE pdrv)
{
    if(hsd.State == HAL_SD_STATE_RESET) return STA_NOINIT;
    return 0;
}

DRESULT disk_read (BYTE pdrv, BYTE *buff, LBA_t sector, UINT count)
{
    if(HAL_SD_ReadBlocks(&hsd, buff, sector, count, 1000) == HAL_OK)
    {
        while(HAL_SD_GetCardState(&hsd) != HAL_SD_CARD_TRANSFER);
        return RES_OK;
    }
    return RES_ERROR;
}

DRESULT disk_write (BYTE pdrv, const BYTE *buff, LBA_t sector, UINT count)
{
    if(HAL_SD_WriteBlocks(&hsd, (uint8_t*)buff, sector, count, 1000) == HAL_OK)
    {
        while(HAL_SD_GetCardState(&hsd) != HAL_SD_CARD_TRANSFER);
        return RES_OK;
    }
    return RES_ERROR;
}

DRESULT disk_ioctl (BYTE pdrv, BYTE cmd, void *buff)
{
    DRESULT res = RES_ERROR;
    HAL_SD_CardInfoTypeDef CardInfo;

    if (hsd.State == HAL_SD_STATE_RESET) return RES_NOTRDY;

    switch (cmd)
    {
    case CTRL_SYNC :
        res = RES_OK;
        break;
    case GET_SECTOR_COUNT :
        HAL_SD_GetCardInfo(&hsd, &CardInfo);
        *(DWORD*)buff = CardInfo.LogBlockNbr;
        res = RES_OK;
        break;
    case GET_SECTOR_SIZE :
        HAL_SD_GetCardInfo(&hsd, &CardInfo);
        *(WORD*)buff = CardInfo.LogBlockSize;
        res = RES_OK;
        break;
    case GET_BLOCK_SIZE :
        HAL_SD_GetCardInfo(&hsd, &CardInfo);
        *(DWORD*)buff = CardInfo.LogBlockSize / 512;
        res = RES_OK;
        break;
    default:
        res = RES_PARERR;
    }
    return res;
}

DWORD get_fattime(void)
{
    return 0; // Return zero or implement RTC
}
