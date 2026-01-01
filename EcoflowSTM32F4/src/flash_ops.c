#include "flash_ops.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>

// STM32F469 has 2MB Flash, organized in two banks (if dual bank mode enabled)
// or just sectors.
// Standard Sector Map (2MB):
// Bank 1:
// Sectors 0-3: 16KB
// Sector 4: 64KB
// Sectors 5-11: 128KB
// Bank 2 (Starts at 0x08100000):
// Sectors 12-15: 16KB
// Sector 16: 64KB
// Sectors 17-23: 128KB

// We assume we are running on one bank and writing to the other.
// For simplicity in this demo, we assume the update is always written to Bank 2 (0x08100000)
// regardless of where we are running (unless we are running in Bank 2, then we write to Bank 1).
// BUT: Swapping banks requires manipulating Option Bytes (BFB2).

// Current approach: Just write to Bank 2 (Address 0x08100000).

void Flash_Unlock(void) {
    HAL_FLASH_Unlock();
}

void Flash_Lock(void) {
    HAL_FLASH_Lock();
}

static uint32_t GetSector(uint32_t Address) {
    uint32_t sector = 0;

    // Bank 1
    if((Address >= 0x08000000) && (Address < 0x08004000)) sector = FLASH_SECTOR_0;
    else if((Address >= 0x08004000) && (Address < 0x08008000)) sector = FLASH_SECTOR_1;
    else if((Address >= 0x08008000) && (Address < 0x0800C000)) sector = FLASH_SECTOR_2;
    else if((Address >= 0x0800C000) && (Address < 0x08010000)) sector = FLASH_SECTOR_3;
    else if((Address >= 0x08010000) && (Address < 0x08020000)) sector = FLASH_SECTOR_4;
    else if((Address >= 0x08020000) && (Address < 0x08040000)) sector = FLASH_SECTOR_5;
    else if((Address >= 0x08040000) && (Address < 0x08060000)) sector = FLASH_SECTOR_6;
    else if((Address >= 0x08060000) && (Address < 0x08080000)) sector = FLASH_SECTOR_7;
    else if((Address >= 0x08080000) && (Address < 0x080A0000)) sector = FLASH_SECTOR_8;
    else if((Address >= 0x080A0000) && (Address < 0x080C0000)) sector = FLASH_SECTOR_9;
    else if((Address >= 0x080C0000) && (Address < 0x080E0000)) sector = FLASH_SECTOR_10;
    else if((Address >= 0x080E0000) && (Address < 0x08100000)) sector = FLASH_SECTOR_11;

    // Bank 2
    else if((Address >= 0x08100000) && (Address < 0x08104000)) sector = FLASH_SECTOR_12;
    else if((Address >= 0x08104000) && (Address < 0x08108000)) sector = FLASH_SECTOR_13;
    else if((Address >= 0x08108000) && (Address < 0x0810C000)) sector = FLASH_SECTOR_14;
    else if((Address >= 0x0810C000) && (Address < 0x08110000)) sector = FLASH_SECTOR_15;
    else if((Address >= 0x08110000) && (Address < 0x08120000)) sector = FLASH_SECTOR_16;
    else if((Address >= 0x08120000) && (Address < 0x08140000)) sector = FLASH_SECTOR_17;
    else if((Address >= 0x08140000) && (Address < 0x08160000)) sector = FLASH_SECTOR_18;
    else if((Address >= 0x08160000) && (Address < 0x08180000)) sector = FLASH_SECTOR_19;
    else if((Address >= 0x08180000) && (Address < 0x081A0000)) sector = FLASH_SECTOR_20;
    else if((Address >= 0x081A0000) && (Address < 0x081C0000)) sector = FLASH_SECTOR_21;
    else if((Address >= 0x081C0000) && (Address < 0x081E0000)) sector = FLASH_SECTOR_22;
    else if((Address >= 0x081E0000) && (Address < 0x08200000)) sector = FLASH_SECTOR_23;

    return sector;
}

uint8_t Flash_EraseSector(uint32_t address) {
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError;

    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    EraseInitStruct.Sector = GetSector(address);
    EraseInitStruct.NbSectors = 1;

    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
        printf("Flash_EraseSector Failed! Addr: %08lX Sector: %lu Error: %lu\n", address, EraseInitStruct.Sector, SectorError);
        return 1;
    }
    return 0;
}

uint8_t Flash_Write(uint32_t address, uint8_t* data, uint32_t len) {
    for (uint32_t i = 0; i < len; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, address + i, data[i]) != HAL_OK) {
            return 1;
        }
    }
    return 0;
}

void Flash_SwapBank(void) {
    printf("Flash_SwapBank: Unlocking OB...\n");
    HAL_FLASH_OB_Unlock();

    // Wait for BSY
    while(__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY));

    uint32_t optcr = FLASH->OPTCR;
    uint32_t initial_optcr = optcr;

    printf("Flash_SwapBank: Current OPTCR: %08lX\n", optcr);

    // Toggle BFB2 (Bit 4)
    if ((optcr & FLASH_OPTCR_BFB2) == FLASH_OPTCR_BFB2) {
        printf("Flash_SwapBank: BFB2 is 1. Clearing (Boot Bank 1)...\n");
        optcr &= ~FLASH_OPTCR_BFB2;
    } else {
        printf("Flash_SwapBank: BFB2 is 0. Setting (Boot Bank 2)...\n");
        optcr |= FLASH_OPTCR_BFB2;
    }

    // Write new value
    FLASH->OPTCR = optcr;

    // Start
    FLASH->OPTCR |= FLASH_OPTCR_OPTSTRT;

    // Wait for BSY
    while(__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY));

    // Verify
    uint32_t verify_optcr = FLASH->OPTCR;
    printf("Flash_SwapBank: New OPTCR: %08lX (Target: %08lX)\n", verify_optcr, optcr);

    // Launch/Reset
    printf("Flash_SwapBank: Launching Reset...\n");
    __disable_irq();
    HAL_FLASH_OB_Launch();

    while(1);
}
