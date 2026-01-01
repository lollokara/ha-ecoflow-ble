#ifndef FLASH_OPS_H
#define FLASH_OPS_H

#include <stdint.h>
#include <stdbool.h>

void Flash_EnsureDualBank(void);
void Flash_Unlock(void);
void Flash_Lock(void);
bool Flash_EraseSector(uint32_t sector_index);
bool Flash_WriteChunk(uint32_t offset, uint8_t *data, uint32_t length);
bool Flash_CopyBootloader(void);
void Flash_SwapBank(void);

#endif
