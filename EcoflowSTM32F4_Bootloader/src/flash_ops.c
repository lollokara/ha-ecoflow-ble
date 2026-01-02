#include "main.h"
#include "stm32f4xx_hal_flash.h"
#include "stm32f4xx_hal_flash_ex.h"
#include <stdio.h>
#include <string.h>

// Helper to get Physical Sector Number from Address relative to Active Bank
// However, HAL_FLASH_Erase expects Physical Sector Numbers (0-23).
// If BFB2=0 (Bank 1 Active):
//   0x08000000 -> Sector 0 (Bank 1)
//   0x08100000 -> Sector 12 (Bank 2)
// If BFB2=1 (Bank 2 Active):
//   0x08000000 -> Sector 12 (Mapped to 0) -> But HAL logic might be tricky?
//   Wait, STM32F4 Flash HW always sees Physical Sectors.
//   Address mapping changes.
//   If BFB2=1, 0x00000000 is mapped to Bank 2 (Sector 12).
//   But does 0x08000000 map to Bank 2? Yes.
//   And 0x08100000 maps to Bank 1? Yes.
//   So, if we want to write to the INACTIVE bank:
//     If Active=1, Inactive=2 (Address 0x0810xxxx).
//     If Active=2, Inactive=1 (Address 0x0810xxxx).
//   WAIT! The mapping swaps the BANKS.
//   So 0x08000000 is always the ACTIVE bank.
//   And 0x08100000 is always the INACTIVE bank.
//   So we ALWAYS write to 0x08100000 for OTA.

//   Now, what SECTOR number corresponds to 0x08100000?
//   If BFB2=0: 0x08100000 is Physical Sector 12.
//   If BFB2=1: 0x08100000 is Physical Sector 0 (Bank 1, which is now Inactive).

//   So GetPhysicalSector(addr) needs to know BFB2 state.

uint32_t Flash_GetActiveBank(void) {
    // Check Register directly.
    // OPTCR Bit 4 (BFB2)
    // Note: On some F4 parts (F42xxx/43xxx) DB1M bit also matters.
    // F469NI is 2MB, always dual bank.
    if (FLASH->OPTCR & (1 << 4)) {
        return 2;
    } else {
        return 1;
    }
}

uint32_t GetPhysicalSector(uint32_t Address) {
    uint32_t bank = Flash_GetActiveBank();
    uint32_t sector = 0;

    // Address passed here is usually 0x0810xxxx (The Inactive Bank window)
    // We need to determine if that maps to Physical Sector 12 (Bank 2) or Physical Sector 0 (Bank 1).

    // If Active Bank is 1 (BFB2=0):
    //   0x0800xxxx -> Bank 1 (Phys 0-11)
    //   0x0810xxxx -> Bank 2 (Phys 12-23)

    // If Active Bank is 2 (BFB2=1):
    //   0x0800xxxx -> Bank 2 (Phys 12-23)
    //   0x0810xxxx -> Bank 1 (Phys 0-11)

    // Let's assume input Address is always > 0x08000000.

    uint8_t targeting_inactive_window = (Address >= 0x08100000);

    if (bank == 1) {
        // Normal Map
        if (targeting_inactive_window) {
             // Target is Bank 2 (Phys 12+)
             // 0x08100000 -> 12
             if(Address < 0x08104000) return FLASH_SECTOR_12;
             if(Address < 0x08108000) return FLASH_SECTOR_13;
             if(Address < 0x0810C000) return FLASH_SECTOR_14;
             if(Address < 0x08110000) return FLASH_SECTOR_15;
             if(Address < 0x08120000) return FLASH_SECTOR_16;
             if(Address < 0x08140000) return FLASH_SECTOR_17;
             if(Address < 0x08160000) return FLASH_SECTOR_18;
             if(Address < 0x08180000) return FLASH_SECTOR_19;
             if(Address < 0x081A0000) return FLASH_SECTOR_20;
             if(Address < 0x081C0000) return FLASH_SECTOR_21;
             if(Address < 0x081E0000) return FLASH_SECTOR_22;
             return FLASH_SECTOR_23;
        } else {
             // Target is Bank 1 (Phys 0+)
             if(Address < 0x08004000) return FLASH_SECTOR_0;
             if(Address < 0x08008000) return FLASH_SECTOR_1;
             if(Address < 0x0800C000) return FLASH_SECTOR_2;
             if(Address < 0x08010000) return FLASH_SECTOR_3;
             if(Address < 0x08020000) return FLASH_SECTOR_4;
             if(Address < 0x08040000) return FLASH_SECTOR_5;
             if(Address < 0x08060000) return FLASH_SECTOR_6;
             if(Address < 0x08080000) return FLASH_SECTOR_7;
             if(Address < 0x080A0000) return FLASH_SECTOR_8;
             if(Address < 0x080C0000) return FLASH_SECTOR_9;
             if(Address < 0x080E0000) return FLASH_SECTOR_10;
             return FLASH_SECTOR_11;
        }
    } else {
        // Swapped Map (Bank 2 Active)
        if (targeting_inactive_window) {
             // Target is Inactive Window -> Maps to Bank 1 (Phys 0+)
             // Address 0x08100000 corresponds to Physical Sector 0 start
             uint32_t offset = Address - 0x08100000;
             // Use offset to determine Phys 0-11
             if(offset < 0x4000) return FLASH_SECTOR_0;
             if(offset < 0x8000) return FLASH_SECTOR_1;
             if(offset < 0xC000) return FLASH_SECTOR_2;
             if(offset < 0x10000) return FLASH_SECTOR_3;
             if(offset < 0x20000) return FLASH_SECTOR_4;
             if(offset < 0x40000) return FLASH_SECTOR_5;
             if(offset < 0x60000) return FLASH_SECTOR_6;
             if(offset < 0x80000) return FLASH_SECTOR_7;
             if(offset < 0xA0000) return FLASH_SECTOR_8;
             if(offset < 0xC0000) return FLASH_SECTOR_9;
             if(offset < 0xE0000) return FLASH_SECTOR_10;
             return FLASH_SECTOR_11;
        } else {
             // Target is Active Window (0x0800xxxx) -> Maps to Bank 2 (Phys 12+)
             uint32_t offset = Address - 0x08000000;
             if(offset < 0x4000) return FLASH_SECTOR_12;
             if(offset < 0x8000) return FLASH_SECTOR_13;
             if(offset < 0xC000) return FLASH_SECTOR_14;
             if(offset < 0x10000) return FLASH_SECTOR_15;
             if(offset < 0x20000) return FLASH_SECTOR_16;
             if(offset < 0x40000) return FLASH_SECTOR_17;
             if(offset < 0x60000) return FLASH_SECTOR_18;
             if(offset < 0x80000) return FLASH_SECTOR_19;
             if(offset < 0xA0000) return FLASH_SECTOR_20;
             if(offset < 0xC0000) return FLASH_SECTOR_21;
             if(offset < 0xE0000) return FLASH_SECTOR_22;
             return FLASH_SECTOR_23;
        }
    }
}

void Flash_Unlock(void) {
    HAL_FLASH_Unlock();
    // Clear Flags
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);
}

void Flash_Lock(void) {
    HAL_FLASH_Lock();
}

uint8_t Flash_EraseSector(uint32_t sector) {
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError;

    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    EraseInitStruct.Sector = sector;
    EraseInitStruct.NbSectors = 1;

    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
        return HAL_ERROR;
    }
    return HAL_OK;
}

uint8_t Flash_Write(uint32_t addr, uint8_t *data, uint32_t len) {
    // addr is logical address (e.g., 0x0810xxxx)
    for (uint32_t i = 0; i < len; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, addr + i, data[i]) != HAL_OK) {
            return HAL_ERROR;
        }
    }
    return HAL_OK;
}

void Flash_ToggleBank(void) {
    HAL_FLASH_OB_Unlock();
    while(__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY));

    // Toggle Bit 4
    if (FLASH->OPTCR & (1 << 4)) {
        FLASH->OPTCR &= ~(1 << 4); // Set to Bank 1
    } else {
        FLASH->OPTCR |= (1 << 4);  // Set to Bank 2
    }

    FLASH->OPTCR |= FLASH_OPTCR_OPTSTRT;
    while(__HAL_FLASH_GET_FLAG(FLASH_FLAG_BSY));

    HAL_FLASH_OB_Lock();
    HAL_FLASH_OB_Launch();
}

// Copy Bootloader (16KB) + Config (16KB) from Active to Inactive
void Flash_CopyBootloader(uint32_t src_bank_start, uint32_t dst_bank_start) {
    // src_bank_start should be 0x08000000 (Active)
    // dst_bank_start should be 0x08100000 (Inactive)

    uint32_t s0 = GetPhysicalSector(dst_bank_start);
    uint32_t s1 = GetPhysicalSector(dst_bank_start + 0x4000);

    printf("[Flash] Copying Bootloader: Erasing Phys Sectors %d, %d\n", (int)s0, (int)s1);

    Flash_EraseSector(s0);
    Flash_EraseSector(s1);

    printf("[Flash] Copying Data...\n");
    for (uint32_t i = 0; i < 0x8000; i+=4) {
        uint32_t data = *(__IO uint32_t*)(src_bank_start + i);
        // Write to DST address. Hardware handles mapping if we use correct address.
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, dst_bank_start + i, data) != HAL_OK) {
            printf("[Flash] Copy Fail at %X\n", (unsigned int)(dst_bank_start + i));
            // Error_Handler(); // Don't hang here, just report
        }
    }

    if (memcmp((void*)src_bank_start, (void*)dst_bank_start, 0x8000) != 0) {
        printf("[Flash] Verification Failed!\n");
    }
}
