#include "sd_driver.h"
#include <string.h>

SD_HandleTypeDef hsd;
static volatile DSTATUS Stat = STA_NOINIT;

void MX_SDIO_SD_Init(void) {
    hsd.Instance = SDIO;
    hsd.Init.ClockEdge = SDIO_CLOCK_EDGE_RISING;
    hsd.Init.ClockBypass = SDIO_CLOCK_BYPASS_DISABLE;
    hsd.Init.ClockPowerSave = SDIO_CLOCK_POWER_SAVE_DISABLE;
    hsd.Init.BusWide = SDIO_BUS_WIDE_1B;
    hsd.Init.HardwareFlowControl = SDIO_HARDWARE_FLOW_CONTROL_DISABLE;
    hsd.Init.ClockDiv = 0; // Starts with 0, adjusted later

    // GPIO Init should be done in HAL_SD_MspInit or similar
}

DSTATUS SD_Initialize(void) {
    if (HAL_SD_Init(&hsd) != HAL_OK) {
        return STA_NOINIT;
    }

    // Switch to 4-bit mode for speed
    if (HAL_SD_ConfigWideBusOperation(&hsd, SDIO_BUS_WIDE_4B) != HAL_OK) {
        // Fallback or error
    }

    Stat &= ~STA_NOINIT;
    return Stat;
}

DSTATUS SD_GetStatus(void) {
    return Stat;
}

DRESULT SD_Read(BYTE *buff, LBA_t sector, UINT count) {
    if (Stat & STA_NOINIT) return RES_NOTRDY;

    // HAL_SD_ReadBlocks accepts uint32_t* buf, so cast is needed.
    // Also timeout needs to be appropriate.
    if (HAL_SD_ReadBlocks(&hsd, (uint8_t*)buff, sector, count, 1000) != HAL_OK) {
        return RES_ERROR;
    }

    // Wait for transfer complete? HAL_SD_ReadBlocks is blocking.
    // But we should check state.
    while(HAL_SD_GetCardState(&hsd) != HAL_SD_CARD_TRANSFER) {}

    return RES_OK;
}

DRESULT SD_Write(const BYTE *buff, LBA_t sector, UINT count) {
    if (Stat & STA_NOINIT) return RES_NOTRDY;

    if (HAL_SD_WriteBlocks(&hsd, (uint8_t*)buff, sector, count, 1000) != HAL_OK) {
        return RES_ERROR;
    }

    while(HAL_SD_GetCardState(&hsd) != HAL_SD_CARD_TRANSFER) {}

    return RES_OK;
}

DRESULT SD_Ioctl(BYTE cmd, void *buff) {
    DRESULT res = RES_ERROR;
    HAL_SD_CardInfoTypeDef CardInfo;

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
        *(DWORD*)buff = 1; // Block size in sectors
        res = RES_OK;
        break;
    }
    return res;
}

// MSP Init for SDIO
void HAL_SD_MspInit(SD_HandleTypeDef* hsd) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if(hsd->Instance==SDIO) {
        __HAL_RCC_SDIO_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();
        __HAL_RCC_GPIOD_CLK_ENABLE();

        // PC8..PC12: D0..D3, CK
        GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF12_SDIO;
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

        // PD2: CMD
        GPIO_InitStruct.Pin = GPIO_PIN_2;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF12_SDIO;
        HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    }
}
