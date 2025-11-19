#ifndef EcoflowProtocol_h
#define EcoflowProtocol_h

#include <Arduino.h>
#include "pb_encode.h"
#include "pb_decode.h"
#include "generated/ecoflow_p2_ble.pb.h"

// Manufacturer ID for Ecoflow devices
#define ECOFLOW_MANUFACTURER_ID 46517 // 0xB5D5

// ============================================================================
// SERVICE & CHARACTERISTICS - PROTOCOL V2
// ============================================================================
#define SERVICE_UUID_ECOFLOW      "00000001-0000-1000-8000-00805f9b34fb"
#define CHAR_WRITE_UUID_ECOFLOW   "00000002-0000-1000-8000-00805f9b34fb"
#define CHAR_READ_UUID_ECOFLOW    "00000003-0000-1000-8000-00805f9b34fb"


// ============================================================================
// PROTOCOL FRAME HELPERS (V2)
// ============================================================================

// Creates a complete command frame for a given protobuf message
// Returns the size of the generated frame, or 0 on failure.
size_t create_command_frame(uint8_t* buffer, size_t buffer_size, const ToDevice& message);

// Generates a heartbeat message
ToDevice create_heartbeat_message(uint32_t seq);

// Generates a request for all data
ToDevice create_request_data_message(uint32_t seq);

// Generates a command to set a parameter (e.g., AC on/off)
ToDevice create_set_data_message(uint32_t seq, const char* key, int32_t value);


#endif // EcoflowProtocol_h