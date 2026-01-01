#ifndef OTA_CORE_H
#define OTA_CORE_H

#include <stdint.h>
#include <stdbool.h>
#include "stm32f4xx_hal.h"

// OTA Commands (must match ESP32)
#define OTA_CMD_START 0xF0
#define OTA_CMD_DATA  0xF1
#define OTA_CMD_END   0xF2

// Return codes for UART response
#define OTA_RESP_OK      0x00
#define OTA_RESP_ERROR   0xFF

// Function prototypes
void OTA_Init(void);
void OTA_ProcessCommand(uint8_t cmd, uint8_t* payload, uint16_t len);
bool OTA_IsActive(void);

#endif // OTA_CORE_H
