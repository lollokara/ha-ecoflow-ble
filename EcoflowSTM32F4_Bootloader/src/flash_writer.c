#include "flash_writer.h"
#include "stm32f4xx_hal.h"
#include "boot_shared.h"

extern IWDG_HandleTypeDef hiwdg;

void Flash_Unlock(void) {
    HAL_FLASH_Unlock();
}

void Flash_Lock(void) {
    HAL_FLASH_Lock();
}

bool Flash_EraseBank2(void) {
    // Actually erasing APP area (Sector 5 to 11)
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError;

    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    EraseInitStruct.NbSectors = 1;

    // Sector 5 (128KB) to 11 (128KB)
    for (uint32_t sector = FLASH_SECTOR_5; sector <= FLASH_SECTOR_11; sector++) {
        EraseInitStruct.Sector = sector;

        HAL_IWDG_Refresh(&hiwdg);

        if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
            return false;
        }
    }
    return true;
}

bool Flash_WriteChunk(uint32_t offset, uint8_t *data, uint32_t length) {
    // Write directly to App Address (0x08020000)
    uint32_t destAddr = 0x08020000 + offset;

    for (uint32_t i = 0; i < length; i += 4) {
        uint32_t word = 0;
        // Construct 32-bit word from bytes (Little Endian)
        if (i < length) word |= data[i];
        if (i + 1 < length) word |= (data[i + 1] << 8);
        else word |= (0xFF << 8); // Padding
        if (i + 2 < length) word |= (data[i + 2] << 16);
        else word |= (0xFF << 16); // Padding
        if (i + 3 < length) word |= (data[i + 3] << 24);
        else word |= (0xFF << 24); // Padding

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, destAddr, word) != HAL_OK) {
            return false;
        }
        destAddr += 4;
    }
    return true;
}

bool Flash_SetUpdateFlag(uint32_t size, uint32_t checksum) {
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError;

    // Erase Config Sector
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    EraseInitStruct.Sector = SECTOR_CONFIG;
    EraseInitStruct.NbSectors = 1;

    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
        return false;
    }

    // Write Struct
    uint32_t addr = ADDR_CONFIG;
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr, BOOT_MAGIC_UPDATE_PENDING) != HAL_OK) return false;
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + 4, size) != HAL_OK) return false;
    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + 8, checksum) != HAL_OK) return false;

    return true;
}
