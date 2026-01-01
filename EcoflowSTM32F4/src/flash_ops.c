#include "flash_ops.h"
#include "stm32f4xx_hal.h"
#include <string.h>

#define BANK2_START_ADDR 0x08100000
#define BOOTLOADER_SIZE  0x4000 // 16KB
#define CONFIG_SIZE      0x4000 // 16KB

// Helper to get Sector Number from Address (Specific to F469)
// Now used to calculate sector for erase
uint32_t GetSector(uint32_t Address) {
    if((Address >= 0x08000000) && (Address < 0x08004000)) return FLASH_SECTOR_0;
    if((Address >= 0x08004000) && (Address < 0x08008000)) return FLASH_SECTOR_1;
    if((Address >= 0x08008000) && (Address < 0x0800C000)) return FLASH_SECTOR_2;
    if((Address >= 0x0800C000) && (Address < 0x08010000)) return FLASH_SECTOR_3;
    if((Address >= 0x08010000) && (Address < 0x08020000)) return FLASH_SECTOR_4;
    if((Address >= 0x08020000) && (Address < 0x08040000)) return FLASH_SECTOR_5;
    if((Address >= 0x08040000) && (Address < 0x08060000)) return FLASH_SECTOR_6;
    if((Address >= 0x08060000) && (Address < 0x08080000)) return FLASH_SECTOR_7;
    if((Address >= 0x08080000) && (Address < 0x080A0000)) return FLASH_SECTOR_8;
    if((Address >= 0x080A0000) && (Address < 0x080C0000)) return FLASH_SECTOR_9;
    if((Address >= 0x080C0000) && (Address < 0x080E0000)) return FLASH_SECTOR_10;
    if((Address >= 0x080E0000) && (Address < 0x08100000)) return FLASH_SECTOR_11;

    // Bank 2 (Physical 0x0810xxxx)
    if((Address >= 0x08100000) && (Address < 0x08104000)) return FLASH_SECTOR_12;
    if((Address >= 0x08104000) && (Address < 0x08108000)) return FLASH_SECTOR_13;
    if((Address >= 0x08108000) && (Address < 0x0810C000)) return FLASH_SECTOR_14;
    if((Address >= 0x0810C000) && (Address < 0x08110000)) return FLASH_SECTOR_15;
    if((Address >= 0x08110000) && (Address < 0x08120000)) return FLASH_SECTOR_16;
    if((Address >= 0x08120000) && (Address < 0x08140000)) return FLASH_SECTOR_17;
    if((Address >= 0x08140000) && (Address < 0x08160000)) return FLASH_SECTOR_18;
    if((Address >= 0x08160000) && (Address < 0x08180000)) return FLASH_SECTOR_19;
    if((Address >= 0x08180000) && (Address < 0x081A0000)) return FLASH_SECTOR_20;
    if((Address >= 0x081A0000) && (Address < 0x081C0000)) return FLASH_SECTOR_21;
    if((Address >= 0x081C0000) && (Address < 0x081E0000)) return FLASH_SECTOR_22;
    if((Address >= 0x081E0000) && (Address < 0x08200000)) return FLASH_SECTOR_23;

    return FLASH_SECTOR_23;
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

    // Using Direct Register Bit Defs if HAL missing
    #ifndef FLASH_OPTCR_DB1M
    #define FLASH_OPTCR_DB1M (1 << 30)
    #endif

    if ((OBInit.USERConfig & FLASH_OPTCR_DB1M) == 0) {
        // Logic to enable Dual Bank if needed
        // For F469, this should usually be set.
    }
}

bool Flash_WriteChunk(uint32_t offset, uint8_t *data, uint32_t length) {
    uint32_t base_addr = BANK2_START_ADDR + 0x8000; // Skip Bootloader (16k) and Config (16k)
    uint32_t start_addr = base_addr + offset;

    for (uint32_t i = 0; i < length; i += 4) {
        uint32_t data_word;
        memcpy(&data_word, &data[i], 4);

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

bool Flash_CopyBootloader(void) {
    // 1. Erase Bootloader Sector (12)
    if (!Flash_EraseSector(FLASH_SECTOR_12)) return false;

    // 2. Copy Bootloader (Active 0 -> Inactive 12)
    uint32_t src = 0x08000000;
    uint32_t dst = 0x08100000;

    for (uint32_t i = 0; i < BOOTLOADER_SIZE; i += 4) {
        uint32_t data = *(__IO uint32_t*)(src + i);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, dst + i, data) != HAL_OK) {
            return false;
        }
    }

    // 3. Erase Config Sector (13)
    if (!Flash_EraseSector(FLASH_SECTOR_13)) return false;

    // 4. Copy Config (Active 1 -> Inactive 13)
    // Note: Config is at 0x08004000 (Sector 1)
    // Destination is 0x08104000 (Sector 13)
    src = 0x08004000;
    dst = 0x08104000;

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

    // BFB2 is Bit 4 of OPTCR
    // Use register definition directly to be safe
    #ifndef FLASH_OPTCR_BFB2
    #define FLASH_OPTCR_BFB2 (1 << 4)
    #endif

    // Toggle
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
