#ifndef FLASH_OPS_H
#define FLASH_OPS_H

#include <stdint.h>

#define SECTOR_0_ADDR  0x08000000
#define SECTOR_1_ADDR  0x08004000
#define SECTOR_2_ADDR  0x08008000
#define SECTOR_12_ADDR 0x08100000 // Start of Bank 2

int Flash_Unlock(void);
int Flash_Lock(void);
int Flash_EraseSector(uint32_t sector_idx);
int Flash_Write(uint32_t address, uint8_t *data, uint32_t length);
int Flash_GetActiveBank(void); // 1 or 2
int Flash_ToggleBank(void);
int Flash_CopyBootloader(void); // Copies Active Bank Sector 0-1 to Inactive Bank

// Helper to get sector index from address
uint32_t GetSector(uint32_t Address);

#endif
