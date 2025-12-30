#ifndef ECOFLOW_PROTOCOL_H
#define ECOFLOW_PROTOCOL_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Protocol constants
#define START_BYTE 0xAA
#define MAX_PAYLOAD_LEN 255
#define MAX_DEVICES 4

// Message Format: [START][CMD][LEN][PAYLOAD][CRC8]

// ESP32 -> F4 Command IDs
#define CMD_BATTERY_STATUS 0x01
#define CMD_TEMPERATURE 0x02
#define CMD_CONNECTION_STATE 0x03

#define CMD_HANDSHAKE_ACK 0x21
#define CMD_DEVICE_LIST 0x22
#define CMD_DEVICE_STATUS 0x24


// F4 -> ESP32 Command IDs
#define CMD_REQUEST_STATUS_UPDATE 0x10

#define CMD_HANDSHAKE 0x20
#define CMD_DEVICE_LIST_ACK 0x23
#define CMD_GET_DEVICE_STATUS 0x25
#define CMD_GET_DEVICE_LIST 0x26


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

typedef struct {
    uint8_t id;
    char name[16];
    uint8_t connected;
} DeviceInfo;

typedef struct {
    uint8_t count;
    DeviceInfo devices[MAX_DEVICES];
} DeviceList;

typedef struct {
    uint8_t id;
    BatteryStatus status;
} DeviceStatus;

#pragma pack(pop)

uint8_t calculate_crc8(const uint8_t *data, uint8_t len);
int pack_battery_status_message(uint8_t *buffer, const BatteryStatus *status);
int unpack_battery_status_message(const uint8_t *buffer, BatteryStatus *status);
int pack_request_status_message(uint8_t *buffer);

// New packing/unpacking functions
int pack_handshake_message(uint8_t *buffer);
int pack_handshake_ack_message(uint8_t *buffer);
int pack_device_list_message(uint8_t *buffer, const DeviceList *list);
int unpack_device_list_message(const uint8_t *buffer, DeviceList *list);
int pack_device_list_ack_message(uint8_t *buffer);
int pack_get_device_list_message(uint8_t *buffer);
int pack_get_device_status_message(uint8_t *buffer, uint8_t device_id);
int unpack_get_device_status_message(const uint8_t *buffer, uint8_t *device_id);
int pack_device_status_message(uint8_t *buffer, const DeviceStatus *status);
int unpack_device_status_message(const uint8_t *buffer, DeviceStatus *status);


#ifdef __cplusplus
}
#endif

#endif // ECOFLOW_PROTOCOL_H
