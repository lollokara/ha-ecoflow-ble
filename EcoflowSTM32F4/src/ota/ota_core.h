#ifndef OTA_CORE_H
#define OTA_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include "boot_shared.h"

// Initialize OTA handling
void OTA_Init();

// Process OTA packet
// Returns true if handled
bool OTA_ProcessPacket(uint8_t* buffer, uint16_t len);

// Check if update is pending/in progress
bool OTA_IsActive();

// Get Progress (0-100)
uint8_t OTA_GetProgress();

#endif
