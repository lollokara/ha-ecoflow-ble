#include "flash_ops.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <string.h>

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

// Forward Declaration
static uint32_t GetSector(uint32_t Address);

// Copy Bootloader (Sectors 0 and 1, 32KB) from Bank 1 to Bank 2.
// Required because we are swapping banks, and Bank 2 needs a valid bootloader at offset 0.
int Flash_CopyBootloader(void) {
    uint32_t src_addr = 0x08000000;
    uint32_t dst_addr = 0x08100000;
    uint32_t size = 0x8000; // 32KB (Sector 0 + Sector 1)

    printf("Flash_Ops: Copying Bootloader (32KB) from 0x%08lX to 0x%08lX\n", src_addr, dst_addr);

    HAL_FLASH_Unlock();

    // Erase Destination Sectors (Sector 0 and 1 in Bank 2)
    // In Dual Bank Mode:
    // Bank 1: Sectors 0-11
    // Bank 2: Sectors 12-23
    // But Wait. F469 Sectors are contiguous 0-23 in Linear Mode?
    // RM0386: "In Dual Bank mode... Bank 1 (0 to 11)... Bank 2 (12 to 23)"
    // Sector 12 (16KB) maps to 0x08100000.
    // Sector 13 (16KB) maps to 0x08104000.

    // We must erase the sectors corresponding to destination (0x08100000).
    // Note: Use GetSector to handle mapping if BFB2 is set (e.g. running from Bank 2)
    // Actually GetSector logic handles logical mapping.
    // If we write to 0x08100000, we want to write to "Inactive Bank".

    // However, HAL_FLASHEx_Erase requires a START SECTOR index.
    // And NbSectors must be contiguous?
    // Physical sectors 0-11 and 12-23 are contiguous ranges.
    // GetSector returns the correct physical start sector.

    FLASH_EraseInitTypeDef erase;
    erase.TypeErase = FLASH_TYPEERASE_SECTORS;
    erase.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    erase.Sector = GetSector(dst_addr); // Dynamic resolution
    erase.NbSectors = 2; // Sector 0+1 OR 12+13

    // Sanity check: If Sector is 11, NbSectors 2 would cross bank boundary?
    // Sector 11 is last of Bank 1.
    // But dst_addr is bank aligned. So it will return 0 or 12.

    uint32_t error = 0;
    if (HAL_FLASHEx_Erase(&erase, &error) != HAL_OK) {
        printf("Flash_Ops: Bootloader Erase Failed. Error: %lu\n", error);
        HAL_FLASH_Lock();
        return -1;
    }

    // Copy Loop
    for (uint32_t i = 0; i < size; i += 4) {
        uint32_t data = *(volatile uint32_t*)(src_addr + i);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, dst_addr + i, data) != HAL_OK) {
             printf("Flash_Ops: Bootloader Copy Failed at offset %lu\n", i);
             HAL_FLASH_Lock();
             return -2;
        }
    }

    HAL_FLASH_Lock();
    printf("Flash_Ops: Bootloader Copy Success.\n");
    return 0;
}

// Current approach: Just write to Bank 2 (Address 0x08100000).

// RAM Function to safely switch to Dual Bank Mode
// Must be placed in RAM to avoid crashing during mass erase.
#define FLASH_KEY1               0x45670123U
#define FLASH_KEY2               0xCDEF89ABU
#define FLASH_OPT_KEY1           0x08192A3BU
#define FLASH_OPT_KEY2           0x4C5D6E7FU

__attribute__((section(".data"), noinline)) void Flash_SetDualBankMode_RAM(void) {
    // 1. Unlock Flash Option Bytes
    if (FLASH->OPTCR & FLASH_OPTCR_OPTLOCK) {
        FLASH->OPTKEYR = FLASH_OPT_KEY1;
        FLASH->OPTKEYR = FLASH_OPT_KEY2;
    }

    // 2. Wait for BSY
    while (FLASH->SR & FLASH_SR_BSY);

    // 3. Modify OPTCR
    // Set DB1M (Bit 30)
    // Clear BFB2 (Bit 4) -> Boot from Bank 1
    uint32_t optcr = FLASH->OPTCR;
    optcr |= FLASH_OPTCR_DB1M;
    optcr &= ~FLASH_OPTCR_BFB2;
    FLASH->OPTCR = optcr;

    // 4. Start Programming
    FLASH->OPTCR |= FLASH_OPTCR_OPTSTRT;

    // 5. Wait for BSY (Mass Erase happens here)
    while (FLASH->SR & FLASH_SR_BSY);

    // 6. Launch / Reset
    // Use NVIC_SystemReset equivalent inline
    __DSB();
    SCB->AIRCR  = (uint32_t)((0x5FAUL << SCB_AIRCR_VECTKEY_Pos) |
                             (SCB->AIRCR & SCB_AIRCR_PRIGROUP_Msk) |
                             SCB_AIRCR_SYSRESETREQ_Msk);
    __DSB();

    while(1);
}

void Flash_EnableDualBank_IfMissing(void) {
    if ((FLASH->OPTCR & FLASH_OPTCR_DB1M) == 0) {
        printf("Flash_Ops: Enabling Dual Bank Mode (DB1M). Mass Erase Imminent.\n");
        printf("Flash_Ops: Device will reset and stay blank. Re-flash via PIO required.\n");
        HAL_Delay(100);

        __disable_irq();
        Flash_SetDualBankMode_RAM();
    }
}

void Flash_Unlock(void) {
    HAL_FLASH_Unlock();
}

void Flash_Lock(void) {
    HAL_FLASH_Lock();
}

static uint32_t GetSector(uint32_t Address) {
    uint32_t sector = 0;

    // Determine Bank Base Addresses
    // If BFB2 = 0: Bank 1 @ 0x08000000, Bank 2 @ 0x08100000 (Default)
    // If BFB2 = 1: Bank 2 @ 0x08000000, Bank 1 @ 0x08100000 (Swapped)

    // The input 'Address' is the Virtual Address we are erasing/writing.
    // If we write to 0x0810xxxx, we are targeting the "Second Logical Bank".
    // If BFB2=0, that maps to Physical Sectors 12-23.
    // If BFB2=1, that maps to Physical Sectors 0-11.

    uint8_t bfb2 = (FLASH->OPTCR & FLASH_OPTCR_BFB2) ? 1 : 0;

    // Normalize Address to Offset within a 1MB Bank
    uint32_t offset = 0;
    uint32_t bank_target = 0; // 0 for 1st Logical, 1 for 2nd Logical

    if (Address >= 0x08100000) {
        offset = Address - 0x08100000;
        bank_target = 1;
    } else if (Address >= 0x08000000) {
        offset = Address - 0x08000000;
        bank_target = 0;
    }

    // Determine Target Physical Bank (0=Sectors 0-11, 1=Sectors 12-23)
    // Logical 0 (0x0800...) -> Active Bank
    // Logical 1 (0x0810...) -> Inactive Bank

    // If BFB2=0 (Boot Bank 1):
    // Active (Log 0) = Phys Bank 1 (0-11)
    // Inactive (Log 1) = Phys Bank 2 (12-23)

    // If BFB2=1 (Boot Bank 2):
    // Active (Log 0) = Phys Bank 2 (12-23)
    // Inactive (Log 1) = Phys Bank 1 (0-11)

    uint32_t target_phys_bank = 0;
    if (bank_target == 0) {
        // Addressing 0x0800xxxx
        target_phys_bank = (bfb2 == 1) ? 1 : 0;
    } else {
        // Addressing 0x0810xxxx
        target_phys_bank = (bfb2 == 1) ? 0 : 1;
    }

    // Map Offset to Physical Sector Index relative to Bank Start
    uint32_t phys_sector_base = (target_phys_bank == 1) ? 12 : 0;
    uint32_t relative_idx = 0;

    if (offset < 0x4000) relative_idx = 0;
    else if (offset < 0x8000) relative_idx = 1;
    else if (offset < 0xC000) relative_idx = 2;
    else if (offset < 0x10000) relative_idx = 3;
    else if (offset < 0x20000) relative_idx = 4; // 64KB
    else if (offset < 0x40000) relative_idx = 5; // 128KB
    else if (offset < 0x60000) relative_idx = 6;
    else if (offset < 0x80000) relative_idx = 7;
    else if (offset < 0xA0000) relative_idx = 8;
    else if (offset < 0xC0000) relative_idx = 9;
    else if (offset < 0xE0000) relative_idx = 10;
    else relative_idx = 11;

    sector = phys_sector_base + relative_idx;

    // Mapping debug
    // printf("GetSector Addr %08lX (Offset %05lX) -> BankTarget %lu (BFB2=%d) -> PhysBank %lu -> Sector %lu\n",
    //        Address, offset, bank_target, bfb2, target_phys_bank, sector);

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
    uint32_t i = 0;

    // Ensure clean state before writing
    if (FLASH->SR & (FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                     FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR)) {
        printf("Flash_Write: Clearing Pre-existing Flags: SR=%08lX\n", FLASH->SR);
        __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                               FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
    }

    // Program in 32-bit Words (x4 faster)
    for (i = 0; i < (len & ~3); i += 4) {
        uint32_t word;
        memcpy(&word, &data[i], 4);

        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address + i, word) != HAL_OK) {
             uint32_t err = HAL_FLASH_GetError();
             printf("Flash_Write: Word Program Failed @ %08lX. Error: %lu, SR: %08lX\n", address + i, err, FLASH->SR);
             return 1;
        }
    }

    // Handle remaining bytes
    for (; i < len; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, address + i, data[i]) != HAL_OK) {
             uint32_t err = HAL_FLASH_GetError();
             printf("Flash_Write: Byte Program Failed @ %08lX. Error: %lu, SR: %08lX\n", address + i, err, FLASH->SR);
             return 1;
        }
    }
    return 0;
}

void Flash_SwapBank(void) {
    // If DB1M is 0, we must enable it first.
    // This will wipe the device.
    Flash_EnableDualBank_IfMissing();

    printf("Flash_SwapBank: Unlocking OB...\n");
    HAL_FLASH_OB_Unlock();

    // Wait for BSY
    while(__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY));

    uint32_t optcr = FLASH->OPTCR;

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
