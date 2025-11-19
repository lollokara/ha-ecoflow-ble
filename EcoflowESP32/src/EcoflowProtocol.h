/*
 * EcoflowProtocol.h - Ecoflow BLE Protocol Constants (FINAL CORRECTED)
 * 
 * YOUR DELTA 3 USES:
 * Service:  0x0001 (16-bit UUID)
 * Write:    0x0002 (16-bit UUID) 
 * Read/Notify: 0x0003 (16-bit UUID)
 * 
 * These are converted to full 128-bit UUIDs using the Bluetooth SIG standard:
 * xxxx-0000-1000-8000-00805f9b34fb
 */

#ifndef EcoflowProtocol_h
#define EcoflowProtocol_h

// Manufacturer ID for Ecoflow devices
#define ECOFLOW_MANUFACTURER_ID 46517  // 0xB5D5

// ============================================================================
// SERVICE & CHARACTERISTICS - YOUR DELTA 3 (CORRECTED FORMAT)
// ============================================================================

// Convert 16-bit UUIDs to full 128-bit format for NimBLE
// Service 0x0001 → 00000001-0000-1000-8000-00805f9b34fb
#define SERVICE_UUID_ECOFLOW "00000001-0000-1000-8000-00805f9b34fb"

// Characteristic 0x0002 (Write) → 00000002-0000-1000-8000-00805f9b34fb
#define CHAR_WRITE_UUID_ECOFLOW "00000002-0000-1000-8000-00805f9b34fb"

// Characteristic 0x0003 (Notify) → 00000003-0000-1000-8000-00805f9b34fb
#define CHAR_READ_UUID_ECOFLOW "00000003-0000-1000-8000-00805f9b34fb"

// ============================================================================
// ALTERNATE UUIDS (FOR REFERENCE - NOT USED BY YOUR DEVICE)
// ============================================================================

// Standard Ecoflow UUIDs from other implementations
#define SERVICE_UUID_ALT "00001801-0000-1000-8000-00805f9b34fb"
#define CHAR_WRITE_UUID_ALT "0000ff01-0000-1000-8000-00805f9b34fb"
#define CHAR_READ_UUID_ALT "0000ff02-0000-1000-8000-00805f9b34fb"

// ============================================================================
// PROTOCOL FRAME DEFINITIONS
// ============================================================================

// Data request command - requests device status
// Frame: AA 02 01 00 00 00 00 00 ... 00 AD
static const uint8_t CMD_REQUEST_DATA[] = {
    0xAA, 0x02, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0xAD
};

// AC Outlet ON
// Frame: AA 02 01 06 00 81 00 00 ... 00 01 8F
static const uint8_t CMD_AC_ON[] = {
    0xAA, 0x02, 0x01, 0x06, 0x00, 0x81, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x8F
};

// AC Outlet OFF
// Frame: AA 02 01 06 00 81 00 00 ... 00 00 8E
static const uint8_t CMD_AC_OFF[] = {
    0xAA, 0x02, 0x01, 0x06, 0x00, 0x81, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x8E
};

// USB Outlet ON
// Frame: AA 02 01 06 00 82 00 00 ... 00 01 90
static const uint8_t CMD_USB_ON[] = {
    0xAA, 0x02, 0x01, 0x06, 0x00, 0x82, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x90
};

// USB Outlet OFF
// Frame: AA 02 01 06 00 82 00 00 ... 00 00 8F
static const uint8_t CMD_USB_OFF[] = {
    0xAA, 0x02, 0x01, 0x06, 0x00, 0x82, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x8F
};

// DC 12V Outlet ON
// Frame: AA 02 01 06 00 83 00 00 ... 00 01 91
static const uint8_t CMD_12V_ON[] = {
    0xAA, 0x02, 0x01, 0x06, 0x00, 0x83, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x01, 0x91
};

// DC 12V Outlet OFF
// Frame: AA 02 01 06 00 83 00 00 ... 00 00 90
static const uint8_t CMD_12V_OFF[] = {
    0xAA, 0x02, 0x01, 0x06, 0x00, 0x83, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x90
};

#endif // EcoflowProtocol_h
