#ifndef FLASH_OPS_H
#define FLASH_OPS_H

#include <stdint.h>
#include <stdbool.h>

void Flash_Unlock(void);
void Flash_Lock(void);
uint8_t Flash_EraseSector(uint32_t address);
uint8_t Flash_Write(uint32_t address, uint8_t* data, uint32_t len);
void Flash_SwapBank(void);
void Flash_EnsureDualBank(void);

#endif
