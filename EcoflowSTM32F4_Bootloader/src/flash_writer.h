#ifndef FLASH_WRITER_H
#define FLASH_WRITER_H

#include <stdint.h>
#include <stdbool.h>

void Flash_Unlock(void);
void Flash_Lock(void);
bool Flash_EraseBank2(void);
bool Flash_WriteChunk(uint32_t offset, uint8_t *data, uint32_t length);
bool Flash_SetUpdateFlag(uint32_t size, uint32_t checksum);

#endif // FLASH_WRITER_H
