#include "flash_ops.h"
#include "stm32f4xx_hal.h"
#include <string.h>

#define BANK2_START_ADDR 0x08100000
#define BOOTLOADER_SIZE  0x4000 // 16KB
#define CONFIG_SIZE      0x4000 // 16KB

// Using Direct Register Bit Defs if HAL missing
#ifndef FLASH_OPTCR_DB1M
#define FLASH_OPTCR_DB1M (1 << 30)
#endif
#ifndef FLASH_OPTCR_BFB2
#define FLASH_OPTCR_BFB2 (1 << 4)
#endif

// Returns 0 for Bank 1 (Active), 1 for Bank 2 (Active)
// Based on BFB2 bit.
// Note: If BFB2 is SET, Bank 2 is mapped to 0x08000000.
static uint8_t Flash_GetActiveBank(void) {
    if ((FLASH->OPTCR & FLASH_OPTCR_BFB2) == FLASH_OPTCR_BFB2) {
        return 1; // Bank 2 Active
    }
    return 0; // Bank 1 Active
}

void Flash_Unlock(void) {
    HAL_FLASH_Unlock();
}

void Flash_Lock(void) {
    HAL_FLASH_Lock();
}

void Flash_EnsureDualBank(void) {
    FLASH_OBProgramInitTypeDef OBInit;
    HAL_FLASHEx_OBGetConfig(&OBInit);

    if ((OBInit.USERConfig & FLASH_OPTCR_DB1M) == 0) {
        // Logic to enable Dual Bank if needed
    }
}

bool Flash_WriteChunk(uint32_t offset, uint8_t *data, uint32_t length) {
    // We ALWAYS write to the INACTIVE bank address window (0x08100000).
    // Due to aliasing:
    // If BFB2=0 (Active=Bank1 @ 0x0800), then 0x0810 points to Bank 2 (Inactive).
    // If BFB2=1 (Active=Bank2 @ 0x0800), then 0x0810 points to Bank 1 (Inactive).
    uint32_t start_addr = BANK2_START_ADDR + 0x8000 + offset;

    HAL_FLASH_Unlock();

    for (uint32_t i = 0; i < length; i += 4) {
        uint32_t data_word = 0xFFFFFFFF; // Init with 0xFF for padding
        uint8_t bytes_to_copy = (length - i < 4) ? (length - i) : 4;
        memcpy(&data_word, &data[i], bytes_to_copy); // Safe copy

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, start_addr + i, data_word) != HAL_OK) {
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

// Erases the Inactive Bank's Application Area
// Returns true on success
bool Flash_PrepareOTA(void) {
    uint8_t active_bank = Flash_GetActiveBank();
    uint32_t first_sector, last_sector;

    if (active_bank == 0) {
        // Active = Bank 1. Inactive = Bank 2.
        // Erase Physical Sectors 12 to 23.
        // Note: App starts at offset 0x8000 in the bank.
        // Bank 2 structure matches Bank 1.
        // Sector 12 (16K), 13 (16K), 14 (16K)...
        // We skip 12 (Bootloader mirror) and 13 (Config mirror).
        // Start erasing from Sector 14.
        first_sector = FLASH_SECTOR_14;
        last_sector = FLASH_SECTOR_23;
    } else {
        // Active = Bank 2. Inactive = Bank 1.
        // Erase Physical Sectors 0 to 11.
        // Skip 0 (Bootloader) and 1 (Config).
        // Start erasing from Sector 2.
        first_sector = FLASH_SECTOR_2;
        last_sector = FLASH_SECTOR_11;
    }

    // External Watchdog Handle
    extern IWDG_HandleTypeDef hiwdg;

    for (uint32_t i = first_sector; i <= last_sector; i++) {
        HAL_IWDG_Refresh(&hiwdg);
        if (!Flash_EraseSector(i)) return false;
    }
    return true;
}

bool Flash_CopyBootloader(void) {
    uint8_t active_bank = Flash_GetActiveBank();
    uint32_t boot_sector, config_sector;
    uint32_t dst_base;

    if (active_bank == 0) {
        // Active=Bank1. Target=Bank2.
        // Erase Physical 12 & 13.
        boot_sector = FLASH_SECTOR_12;
        config_sector = FLASH_SECTOR_13;
        dst_base = 0x08100000;
    } else {
        // Active=Bank2. Target=Bank1.
        // Erase Physical 0 & 1.
        boot_sector = FLASH_SECTOR_0;
        config_sector = FLASH_SECTOR_1;
        dst_base = 0x08100000; // Mapped address of Inactive Bank is ALWAYS 0x08100000
    }

    // 1. Erase Bootloader Sector
    if (!Flash_EraseSector(boot_sector)) return false;

    // 2. Copy Bootloader (Active Base 0x0800 -> Inactive Base 0x0810)
    uint32_t src = 0x08000000;
    uint32_t dst = dst_base;

    for (uint32_t i = 0; i < BOOTLOADER_SIZE; i += 4) {
        uint32_t data = *(__IO uint32_t*)(src + i);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, dst + i, data) != HAL_OK) {
            return false;
        }
    }

    // 3. Erase Config Sector
    if (!Flash_EraseSector(config_sector)) return false;

    // 4. Copy Config (Active Base + 0x4000 -> Inactive Base + 0x4000)
    src = 0x08004000;
    dst = dst_base + 0x4000;

    for (uint32_t i = 0; i < CONFIG_SIZE; i += 4) {
        uint32_t data = *(__IO uint32_t*)(src + i);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, dst + i, data) != HAL_OK) {
            return false;
        }
    }

    return true;
}

void Flash_SwapBank(void) {
    FLASH_OBProgramInitTypeDef OBInit;

    HAL_FLASH_Unlock();
    HAL_FLASH_OB_Unlock();

    HAL_FLASHEx_OBGetConfig(&OBInit);

    // Toggle BFB2
    if ((OBInit.USERConfig & FLASH_OPTCR_BFB2) == FLASH_OPTCR_BFB2) {
        OBInit.USERConfig &= ~FLASH_OPTCR_BFB2;
    } else {
        OBInit.USERConfig |= FLASH_OPTCR_BFB2;
    }

    OBInit.OptionType = OPTIONBYTE_USER;

    HAL_FLASHEx_OBProgram(&OBInit);
    HAL_FLASH_OB_Launch();
    HAL_NVIC_SystemReset();
}
