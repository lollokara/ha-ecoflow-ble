#include "flash_ops.h"
#include "stm32h7xx_hal.h"
#include <string.h>

#define BANK2_START_ADDR 0x08020000
#define BOOTLOADER_SIZE  0x4000 // 16KB
#define CONFIG_SIZE      0x4000 // 16KB

void Flash_Unlock(void) {
    HAL_FLASH_Unlock();
}

void Flash_Lock(void) {
    HAL_FLASH_Lock();
}

void Flash_EnsureDualBank(void) {
    // STM32H735 does not use dual bank swapping for this OTA logic
}

bool Flash_WriteChunk(uint32_t offset, uint8_t *data, uint32_t length) {
    uint32_t start_addr = BANK2_START_ADDR + offset;

    HAL_FLASH_Unlock();

    for (uint32_t i = 0; i < length; i += 32) {
        uint8_t copy_len = (length - i < 32) ? (length - i) : 32;
        uint8_t flash_word[32];
        memset(flash_word, 0xFF, 32);
        memcpy(flash_word, &data[i], copy_len);

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_FLASHWORD, start_addr + i, (uint32_t)flash_word) != HAL_OK) {
            return false;
        }
    }
    return true;
}

bool Flash_EraseSector(uint32_t sector_index) {
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError = 0;

    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    EraseInitStruct.Sector = sector_index;
    EraseInitStruct.NbSectors = 1;

    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
        return false;
    }
    return true;
}

// Erases the Application Area
// Returns true on success
bool Flash_PrepareOTA(void) {
    uint32_t first_sector = FLASH_SECTOR_1;
    uint32_t last_sector = FLASH_SECTOR_7;

    // External Watchdog Handle
    extern IWDG_HandleTypeDef hiwdg;

    for (uint32_t i = first_sector; i <= last_sector; i++) {
        HAL_IWDG_Refresh(&hiwdg);
        if (!Flash_EraseSector(i)) return false;
    }
    return true;
}

bool Flash_CopyBootloader(void) {
    // No bank swapping, bootloader copy not required
    return true;
}

void Flash_SwapBank(void) {
    NVIC_SystemReset();
}
