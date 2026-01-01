#ifndef FLASH_WRITER_H
#define FLASH_WRITER_H

#include <stdint.h>
#include <stdbool.h>

// Initialize Flash Writer
void FlashWriter_Init(void);

// Prepare the inactive bank for writing (Erase)
// returns true on success
bool FlashWriter_PrepareBank(uint32_t total_size);

// Write a chunk of data to the inactive bank
// offset is relative to the start of the Application space (0x8000)
bool FlashWriter_WriteChunk(uint32_t offset, uint8_t* data, uint16_t len);

// Finalize the update: Verify CRC, Copy Bootloader if needed, Set BFB2, Reset
bool FlashWriter_Finalize(uint32_t total_size, uint32_t crc32);

// Helper to get current active bank (1 or 2)
uint8_t FlashWriter_GetActiveBank(void);

#endif // FLASH_WRITER_H
