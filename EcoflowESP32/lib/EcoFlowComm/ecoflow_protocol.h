#ifndef ECOFLOW_PROTOCOL_H
#define ECOFLOW_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Protocol constants
#define START_BYTE 0xAA
#define MAX_PAYLOAD_LEN 255

// Message Format: [START][CMD][LEN][PAYLOAD][CRC8]

// ESP32 -> F4 Command IDs
#define CMD_BATTERY_STATUS 0x01
#define CMD_TEMPERATURE 0x02
#define CMD_CONNECTION_STATE 0x03

// F4 -> ESP32 Command IDs
#define CMD_REQUEST_STATUS_UPDATE 0x10

#pragma pack(push, 1)

typedef struct {
    uint8_t soc;
    int16_t power_w;
    uint16_t voltage_v;
    uint8_t connected;
    char device_name[16]; // Fixed size for simplicity
} BatteryStatus;

typedef struct {
    int8_t temp_c;
    int8_t temp_min;
    int8_t temp_max;
} Temperature;

typedef struct {
    uint8_t state;
    int8_t signal_db;
} UartConnectionState;

#pragma pack(pop)

uint8_t calculate_crc8(const uint8_t *data, uint8_t len);
int pack_battery_status_message(uint8_t *buffer, const BatteryStatus *status);
int unpack_battery_status_message(const uint8_t *buffer, BatteryStatus *status);
int pack_request_status_message(uint8_t *buffer);

#ifdef __cplusplus
}
#endif

#endif // ECOFLOW_PROTOCOL_H
