#ifndef OTA_HANDLER_H
#define OTA_HANDLER_H

#include <stdint.h>

void OTA_Start(uint32_t size);
void OTA_WriteChunk(uint8_t* payload, uint32_t len);
void OTA_End();
void OTA_ProcessCommand(uint8_t cmd, uint8_t* payload, uint32_t len);

#endif
