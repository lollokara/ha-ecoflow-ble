#ifndef BRAIN_TRANSPLANT_PROTOCOL_H
#define BRAIN_TRANSPLANT_PROTOCOL_H

#include <stdint.h>

#define START_BYTE 0xAA

// OTA Commands
#define CMD_OTA_START   0xA0
#define CMD_OTA_CHUNK   0xA1
#define CMD_OTA_END     0xA2
#define CMD_OTA_APPLY   0xA3
#define CMD_OTA_ACK     0x06
#define CMD_OTA_NACK    0x15

// Minimal Handshake
#define CMD_HANDSHAKE   0x01

#endif
