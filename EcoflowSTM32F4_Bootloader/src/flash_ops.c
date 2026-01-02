#include "flash_ops.h"
#include "stm32f4xx_hal.h"
#include <string.h>

/*
 * STM32F469 Sector Layout:
 * Bank 1:
 *  0: 16KB 0x08000000
 *  1: 16KB 0x08004000
 *  2: 16KB 0x08008000
 *  3: 16KB 0x0800C000
 *  4: 64KB 0x08010000
 *  5-11: 128KB
 * Bank 2:
 *  12: 16KB 0x08100000
 *  13: 16KB 0x08104000
 *  ...
 *
 * NOTE: If BFB2 is set (Boot from Bank 2), the hardware maps 0x08100000 to 0x00000000
 * AND to 0x08000000 (Aliasing). The original Bank 1 appears at 0x08100000.
 *
 * So, logically:
 * ACTIVE BANK starts at 0x08000000.
 * INACTIVE BANK starts at 0x08100000.
 *
 * However, the Flash Controller registers (FLASH_CR) target PHYSICAL sectors.
 * If BFB2 is active, Physical Bank 2 is at 0x08000000.
 *
 * HAL_FLASHEx_Erase handles this if we give it the correct Sector ID?
 * The Reference Manual says:
 * "When the BFB2 bit is set, the Flash memory Bank 2 is mapped at 0x0000 0000 (and 0x0800 0000).
 *  Bank 1 is mapped at 0x0810 0000."
 *
 * But the Sector Numbers in FLASH_CR (SNB field) are fixed to Physical sectors.
 * 0-11 = Physical Bank 1.
 * 12-23 = Physical Bank 2.
 *
 * So if BFB2=1 (Booting from Bank 2), and we want to write to the INACTIVE bank (Bank 1),
 * we need to target address 0x08100000.
 * The internal bus logic maps 0x08100000 to Physical Bank 1.
 * So writing to 0x0810xxxx should naturally hit Physical Bank 1.
 * And GetSector(0x0810xxxx) should return 0-11.
 *
 * Let's verify GetSector logic.
 */

int Flash_Unlock(void) {
    return (HAL_FLASH_Unlock() == HAL_OK) ? 0 : -1;
}

int Flash_Lock(void) {
    return (HAL_FLASH_Lock() == HAL_OK) ? 0 : -1;
}

uint32_t GetSector(uint32_t Address) {
    uint32_t sector = 0;

    // This logic assumes Standard Map (No BFB2 or BFB2=0)
    // If BFB2=1, the banks are swapped.
    // 0x08000000 maps to Bank 2.
    // 0x08100000 maps to Bank 1.
    // However, the function returns a HAL Sector ID (FLASH_SECTOR_x).
    // FLASH_SECTOR_0 is always Physical Bank 1 Sector 0.
    //
    // If we are booting from Bank 2 (BFB2=1), address 0x08000000 is Physical Bank 2.
    // So GetSector(0x08000000) should return FLASH_SECTOR_12.
    //
    // We need to check BFB2 bit to know the mapping.

    int boot_from_bank2 = 0;

    // Check Register: Bit 4 in OPTCR.
    if ((FLASH->OPTCR & (1 << 4)) != 0) {
        boot_from_bank2 = 1;
    }

    /*
     * MAPPING:
     *
     * Case 1: BFB2 = 0 (Normal)
     * 0x0800_0000 -> Physical Bank 1 (Sector 0)
     * 0x0810_0000 -> Physical Bank 2 (Sector 12)
     *
     * Case 2: BFB2 = 1 (Swapped)
     * 0x0800_0000 -> Physical Bank 2 (Sector 12)
     * 0x0810_0000 -> Physical Bank 1 (Sector 0)
     */

    // We only care about base offsets for 2MB device
    // Bank 1: 0x08000000 - 0x080FFFFF
    // Bank 2: 0x08100000 - 0x081FFFFF

    // Normalize address to 0-based relative to 0x08000000 or 0x08100000?
    // No, input Address is absolute (e.g. 0x0810xxxx).

    uint32_t bank1_start = 0x08000000;
    uint32_t bank2_start = 0x08100000;

    if (boot_from_bank2) {
        // Swap starts for calculation
        // But wait, the hardware does the swapping.
        // If I write to 0x08000000, I am writing to Physical Bank 2.
        // So I need to return FLASH_SECTOR_12.

        // If I write to 0x08100000, I am writing to Physical Bank 1.
        // So I need to return FLASH_SECTOR_0.

        // Simple Logic:
        // If Address < 0x08100000: We are targeting the "Active/Boot" Bank.
        //    Since BFB2=1, Active is Phys Bank 2. -> Ret Sectors 12+
        // If Address >= 0x08100000: We are targeting "Inactive" Bank.
        //    Since BFB2=1, Inactive is Phys Bank 1. -> Ret Sectors 0+

        if (Address < 0x08100000) {
            // Mapping to Physical Bank 2 (12-23)
            // Use offsets relative to 0x08000000 but add 12 to result
             if(Address < 0x08004000) return FLASH_SECTOR_12;
             if(Address < 0x08008000) return FLASH_SECTOR_13;
             if(Address < 0x0800C000) return FLASH_SECTOR_14;
             if(Address < 0x08010000) return FLASH_SECTOR_15;
             if(Address < 0x08020000) return FLASH_SECTOR_16;
             // ... simplify later
             // Actually, it's easier to just offset the address to the Physical Bank 2 range (0x0810xxxx) and run standard logic?
             // No, standard logic assumes 0x0810xxxx is Bank 2.
        } else {
            // Address >= 0x08100000. Maps to Physical Bank 1 (0-11).
            // Treat as if it was 0x0800xxxx
            Address -= 0x00100000;
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
    }

    // Fallthrough: BFB2=0 (Normal) OR Logic above for BFB2=1 needs completing
    // Standard Map (Phys 1 at 0x0800, Phys 2 at 0x0810)

    // If BFB2=1 and we are in the "Active Bank" block above, we need to return Sector 12+.
    // The offsets are same as Bank 1.
    if (boot_from_bank2 && Address < 0x08100000) {
             if(Address < 0x08004000) return FLASH_SECTOR_12;
             if(Address < 0x08008000) return FLASH_SECTOR_13;
             if(Address < 0x0800C000) return FLASH_SECTOR_14;
             if(Address < 0x08010000) return FLASH_SECTOR_15;
             if(Address < 0x08020000) return FLASH_SECTOR_16;
             if(Address < 0x08040000) return FLASH_SECTOR_17;
             if(Address < 0x08060000) return FLASH_SECTOR_18;
             if(Address < 0x08080000) return FLASH_SECTOR_19;
             if(Address < 0x080A0000) return FLASH_SECTOR_20;
             if(Address < 0x080C0000) return FLASH_SECTOR_21;
             if(Address < 0x080E0000) return FLASH_SECTOR_22;
             return FLASH_SECTOR_23;
    }

    // Standard Mapping (BFB2=0 OR Address >= 0x08100000 with BFB2=0)
    // Actually if BFB2=0:
    // 0x0800 -> Bank 1 (0-11)
    // 0x0810 -> Bank 2 (12-23)

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

    // Bank 2
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

    return FLASH_SECTOR_0; // Error default
}

int Flash_EraseSector(uint32_t sector_idx) {
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError;

    // Safety: Refresh Watchdog
    extern IWDG_HandleTypeDef hiwdg;
    HAL_IWDG_Refresh(&hiwdg);

    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    EraseInitStruct.Sector = sector_idx;
    EraseInitStruct.NbSectors = 1;

    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
        return -1;
    }
    return 0;
}

int Flash_Write(uint32_t address, uint8_t *data, uint32_t length) {
    // Length must be multiple of 4 for Word programming, or we handle bytes
    // HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE) is slower but safer for arbitrary len
    // But we usually get 256 byte chunks.

    // Refresh WDT
    extern IWDG_HandleTypeDef hiwdg;
    HAL_IWDG_Refresh(&hiwdg);

    for (uint32_t i = 0; i < length; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, address + i, data[i]) != HAL_OK) {
            return -1;
        }
    }
    return 0;
}

int Flash_GetActiveBank(void) {
    // Read OPTCR BFB2 (Bit 4)
    if ((FLASH->OPTCR & (1 << 4)) != 0) {
        return 2;
    }
    return 1;
}

int Flash_ToggleBank(void) {
    FLASH_OBProgramInitTypeDef OBInit;

    HAL_FLASH_Unlock();
    HAL_FLASH_OB_Unlock();

    HAL_FLASHEx_OBGetConfig(&OBInit);

    // Toggle BFB2
    // On F4, BFB2 is in USERConfig.
    // However, HAL is tricky. It masks values.
    // Let's use direct register access for reliability on F469

    uint32_t optcr = FLASH->OPTCR;
    if (optcr & (1 << 4)) {
        optcr &= ~(1 << 4); // Clear BFB2 -> Bank 1
    } else {
        optcr |= (1 << 4);  // Set BFB2 -> Bank 2
    }

    FLASH->OPTCR = optcr;
    HAL_FLASH_OB_Launch(); // This triggers reset or commits?
    // Manual says: "The option bytes are loaded... when the device is reset or... by setting the OBL_LAUNCH bit"

    // Just in case Launch returns
    HAL_FLASH_OB_Lock();
    HAL_FLASH_Lock();

    return 0;
}

int Flash_CopyBootloader(void) {
    // Copy active Sector 0 & 1 to inactive Sector 0 & 1 (Physical 12/13 or 0/1)

    int active_bank = Flash_GetActiveBank();
    uint32_t src_addr, dst_addr;
    uint32_t src_sector, dst_sector;

    // Size to copy: 32KB (Sector 0 + Sector 1)
    // We must assume the Bootloader is running from 0x08000000 (Active Bank)
    src_addr = 0x08000000;

    // Destination is the start of the "Inactive" bank, which is mapped to 0x08100000
    dst_addr = 0x08100000;

    // Unlock Flash
    if (Flash_Unlock() != 0) return -1;

    // Erase Destination Sectors
    // Get correct sector IDs based on current mapping
    uint32_t sector_a = GetSector(dst_addr);       // 16KB
    uint32_t sector_b = GetSector(dst_addr + 0x4000); // 16KB

    if (Flash_EraseSector(sector_a) != 0) return -2;
    if (Flash_EraseSector(sector_b) != 0) return -3;

    // Copy Loop
    uint8_t buffer[256]; // Stack buffer
    for (uint32_t offset = 0; offset < 0x8000; offset += 256) {
        memcpy(buffer, (uint8_t*)(src_addr + offset), 256);
        if (Flash_Write(dst_addr + offset, buffer, 256) != 0) return -4;
    }

    Flash_Lock();
    return 0;
}
