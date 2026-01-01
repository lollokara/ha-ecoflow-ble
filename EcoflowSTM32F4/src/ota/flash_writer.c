#include "flash_writer.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdio.h>

// Flash Sectors for STM32F469NI (2MB)
// Dual Bank organization (when enabled)
// Bank 1: Sectors 0-11
// Bank 2: Sectors 12-23
// Each Bank is 1MB.
// Sector 0 (16KB) - Bootloader
// Sector 1 (16KB) - App Slot 1 / Config
// Sector 2 (16KB) - App Start (0x08008000)

// Target Address Calculation
// If running in Bank 1 (0x0800xxxx), Target is Bank 2 (0x0810xxxx)
// If running in Bank 2 (0x0800xxxx remapped), Target is Bank 1 (Physical 0x0800xxxx which is mapped to 0x0810xxxx)

#define FLASH_BANK_SIZE     0x100000 // 1MB
#define FLASH_BASE_ADDR     0x08000000
#define INACTIVE_BANK_ADDR  0x08100000
#define APP_OFFSET          0x00008000 // 32KB offset (Sector 0 + 1)

void FlashWriter_Init(void) {
    HAL_FLASH_Unlock();
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
}

static uint32_t GetSectorByAddress(uint32_t addr) {
    // Check Option Byte for Bank Swapping
    // FLASH_OPTCR_BFB2 is the bit in OPTCR register

    // If BFB2 is active, we are running from Bank 2 (physically).
    // So 0x0810xxxx is Bank 1 (Sectors 0-11).
    // If BFB2 inactive, we are running from Bank 1.
    // So 0x0810xxxx is Bank 2 (Sectors 12-23).

    uint32_t optcr = FLASH->OPTCR;
    bool bfb2_active = (optcr & FLASH_OPTCR_BFB2) != 0;

    uint32_t offset = addr - 0x08100000;
    uint32_t sector_idx = 0;

    if (offset < 0x4000) sector_idx = 0;
    else if (offset < 0x8000) sector_idx = 1;
    else if (offset < 0xC000) sector_idx = 2;
    else if (offset < 0x10000) sector_idx = 3;
    else if (offset < 0x20000) sector_idx = 4;
    else if (offset < 0x40000) sector_idx = 5;
    else if (offset < 0x60000) sector_idx = 6;
    else if (offset < 0x80000) sector_idx = 7;
    else if (offset < 0xA0000) sector_idx = 8;
    else if (offset < 0xC0000) sector_idx = 9;
    else if (offset < 0xE0000) sector_idx = 10;
    else sector_idx = 11;

    if (!bfb2_active) {
        sector_idx += 12; // Shift to Bank 2 sectors
    }

    return sector_idx;
}

bool FlashWriter_PrepareBank(uint32_t total_size) {
    uint32_t start_addr = INACTIVE_BANK_ADDR + APP_OFFSET; // 0x08108000
    uint32_t end_addr = start_addr + total_size;

    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError = 0;

    // Calculate start and end sectors
    uint32_t start_sector = GetSectorByAddress(start_addr);
    uint32_t end_sector = GetSectorByAddress(end_addr - 1); // Inclusive

    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3; // 3.3V
    EraseInitStruct.Sector = start_sector;
    EraseInitStruct.NbSectors = (end_sector - start_sector) + 1;

    printf("Erasing Sectors %lu to %lu...\n", start_sector, end_sector);

    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
        printf("Erase Error: Sector %lu\n", SectorError);
        return false;
    }

    return true;
}

bool FlashWriter_WriteChunk(uint32_t offset, uint8_t* data, uint16_t len) {
    uint32_t dest_addr = INACTIVE_BANK_ADDR + APP_OFFSET + offset;

    for (uint16_t i = 0; i < len; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, dest_addr + i, data[i]) != HAL_OK) {
            return false;
        }
    }
    return true;
}

static void CopyBootloaderIfNeeded() {
    uint32_t src = 0x08000000;
    uint32_t dst = 0x08100000;
    bool mismatch = false;

    for (uint32_t i = 0; i < 0x4000; i += 4) {
        if (*(volatile uint32_t*)(src + i) != *(volatile uint32_t*)(dst + i)) {
            mismatch = true;
            break;
        }
    }

    if (mismatch) {
        printf("Copying Bootloader...\n");
        // Erase Target Sector 0
        FLASH_EraseInitTypeDef EraseInitStruct;
        uint32_t SectorError = 0;
        EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
        EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
        EraseInitStruct.Sector = GetSectorByAddress(dst);
        EraseInitStruct.NbSectors = 1;

        HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError);

        // Copy
        for (uint32_t i = 0; i < 0x4000; i++) {
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, dst + i, *(volatile uint8_t*)(src + i));
        }
    }
}

bool FlashWriter_Finalize(uint32_t total_size, uint32_t crc32) {
    // 1. Verify CRC of written data
    // TODO: Read back from 0x08108000 and calc CRC

    // 2. Ensure Bootloader exists in target bank
    CopyBootloaderIfNeeded();

    // 3. Swap Bank (Toggle BFB2)
    printf("Swapping Banks...\n");
    HAL_FLASH_OB_Unlock();
    FLASH_OBProgramInitTypeDef OBInit;
    HAL_FLASHEx_OBGetConfig(&OBInit);

    OBInit.OptionType = OPTIONBYTE_USER;
    OBInit.USERConfig ^= FLASH_OPTCR_BFB2; // Toggle

    if (HAL_FLASHEx_OBProgram(&OBInit) == HAL_OK) {
        HAL_FLASH_OB_Launch(); // Applies changes and Resets

        // If not, force reset
        HAL_NVIC_SystemReset();
    }

    HAL_FLASH_OB_Lock();
    return true;
}

uint8_t FlashWriter_GetActiveBank(void) {
    return (FLASH->OPTCR & FLASH_OPTCR_BFB2) ? 2 : 1;
}
