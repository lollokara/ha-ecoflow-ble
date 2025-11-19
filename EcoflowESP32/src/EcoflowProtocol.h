#ifndef EcoflowProtocol_h
#define EcoflowProtocol_h

#include "Arduino.h"

// ============================================================================
// SERVICE & CHARACTERISTIC UUIDs
// ============================================================================

// Primary Ecoflow service
static const char* SERVICE_UUID_ECOFLOW = "70D51000-2C7F-4E75-AE8A-D758951CE4E0";
static const char* CHAR_WRITE_UUID_ECOFLOW = "70D51001-2C7F-4E75-AE8A-D758951CE4E0";
static const char* CHAR_READ_UUID_ECOFLOW = "70D51002-2C7F-4E75-AE8A-D758951CE4E0";

// Alternate service (may be used by some devices)
static const char* SERVICE_UUID_ALT = "00001801-0000-1000-8000-00805f9b34fb";
static const char* CHAR_WRITE_UUID_ALT = "0000ff01-0000-1000-8000-00805f9b34fb";
static const char* CHAR_READ_UUID_ALT = "0000ff02-0000-1000-8000-00805f9b34fb";

// ============================================================================
// MANUFACTURER INFO
// ============================================================================

#define ECOFLOW_MANUFACTURER_ID 46517

// ============================================================================
// COMMANDS - Delta 3 Protocol
// ============================================================================

/**
 * Command Structure (typical):
 * [0] = 0xAA (header)
 * [1] = 0x02 (version/type)
 * [2] = msgtype
 * [3-4] = msglen
 * [5-6] = param ID
 * [7-12] = reserved/padding
 * [13-...] = data/command
 * [...] = CRC checksum (last 2 bytes)
 */

// Request device status
const uint8_t CMD_REQUEST_DATA[] = {
    0xaa, 0x02, 0x01, 0x00, 
    0xa0, 0x0d, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 
    0x21, 0x01, 0x20, 0x01, 
    0x43, 0x57
};

// AC Output Control - ON
const uint8_t CMD_AC_ON[] = {
    0xaa, 0x02, 0x07, 0x00, 
    0xde, 0x0d, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 
    0x21, 0x05, 0x20, 0x42, 
    0x01, 0xff, 0xff, 0xff, 
    0xff, 0xff, 0xff, 0x6b, 
    0x11
};

// AC Output Control - OFF
const uint8_t CMD_AC_OFF[] = {
    0xaa, 0x02, 0x07, 0x00, 
    0xde, 0x0d, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 
    0x21, 0x05, 0x20, 0x42, 
    0x00, 0xff, 0xff, 0xff, 
    0xff, 0xff, 0xff, 0x7b, 
    0xd1
};

// 12V DC Output Control - ON
const uint8_t CMD_12V_ON[] = {
    0xaa, 0x02, 0x01, 0x00, 
    0xa0, 0x0d, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 
    0x21, 0x05, 0x20, 0x51, 
    0x01, 0x32, 0xc2
};

// 12V DC Output Control - OFF
const uint8_t CMD_12V_OFF[] = {
    0xaa, 0x02, 0x01, 0x00, 
    0xa0, 0x0d, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 
    0x21, 0x05, 0x20, 0x51, 
    0x00, 0xf3, 0x02
};

// USB Output Control - ON
const uint8_t CMD_USB_ON[] = {
    0xaa, 0x02, 0x01, 0x00, 
    0xa0, 0x0d, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 
    0x21, 0x02, 0x20, 0x22, 
    0x01, 0x16, 0x86
};

// USB Output Control - OFF
const uint8_t CMD_USB_OFF[] = {
    0xaa, 0x02, 0x01, 0x00, 
    0xa0, 0x0d, 0x00, 0x00, 
    0x00, 0x00, 0x00, 0x00, 
    0x21, 0x02, 0x20, 0x22, 
    0x00, 0xd7, 0x46
};

// ============================================================================
// RESPONSE PACKET STRUCTURE
// ============================================================================

/**
 * Response Format (approximately):
 * Byte 0-1:   Frame Header (0xAA 0x02)
 * Byte 2:     Message Type
 * Byte 3-4:   Message Length
 * Byte 5-6:   Param ID
 * Byte 7-12:  Padding
 * Byte 13-19: Data section
 * Byte 20+:   CRC/Status
 * 
 * Data Payload (bytes 13-19 typically contain):
 * Byte 18:  Battery Level (0-100%)
 * Byte 19:  Status bits (AC, USB, DC on/off flags)
 * Byte 12-13: Input Power (16-bit, big-endian)
 * Byte 14-15: Output Power (16-bit, big-endian)
 */

// ============================================================================
// DEBUG UTILITIES
// ============================================================================

inline void printHexBuffer(const uint8_t* buffer, size_t length) {
    for (size_t i = 0; i < length; i++) {
        if (buffer[i] < 0x10) Serial.print("0");
        Serial.print(buffer[i], HEX);
        if (i < length - 1) Serial.print(" ");
    }
    Serial.println();
}

#endif
