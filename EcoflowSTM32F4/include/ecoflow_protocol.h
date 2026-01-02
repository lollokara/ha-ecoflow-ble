#ifndef ECOFLOW_PROTOCOL_H
#define ECOFLOW_PROTOCOL_H

#include <stdint.h>

/* --- Commands (Shared between ESP32 and STM32) --- */
// Command Structure: [START][CMD][LEN][PAYLOAD][CRC8]

#define START_BYTE          0xAA

// System Commands
#define CMD_HANDSHAKE           0x20
#define CMD_HANDSHAKE_ACK       0x21
#define CMD_DEVICE_LIST         0x22
#define CMD_DEVICE_LIST_ACK     0x23
#define CMD_DEVICE_STATUS       0x24
#define CMD_GET_DEVICE_STATUS   0x25
#define CMD_GET_DEVICE_LIST     0x26
#define CMD_OTA_START           0xF0 // New: OTA Start

// Control Commands
#define CMD_SET_WAVE2           0x30
#define CMD_SET_AC              0x31
#define CMD_SET_DC              0x32
#define CMD_SET_VALUE           0x40
#define CMD_POWER_OFF           0x50

// Debug
#define CMD_GET_DEBUG_INFO      0x60
#define CMD_DEBUG_INFO          0x61
#define CMD_CONNECT_DEVICE      0x62
#define CMD_FORGET_DEVICE       0x63

// Payload Length Max (255 bytes limit by protocol structure)
#define MAX_PAYLOAD_LEN         250
#define MAX_DEVICES             4

/* --- Data Structures --- */

// Wave 2 Enums
typedef enum {
    W2_PARAM_MODE = 0,
    W2_PARAM_SUBMODE = 1,
    W2_PARAM_FAN_SPEED = 2,
    W2_PARAM_TEMP = 3
} W2ParamType;

typedef struct {
    W2ParamType type;
    int value;
} Wave2SetMsg;

// Device Types
#define DEV_TYPE_UNKNOWN    0x00
#define DEV_TYPE_DELTA_3    0x01
#define DEV_TYPE_DELTA_PRO_3 0x02
#define DEV_TYPE_WAVE_2     0x03
#define DEV_TYPE_ALT_CHARGER 0x04

// Generic Device Info
typedef struct {
    uint8_t id;
    uint8_t type;
    uint8_t connected;
    uint8_t paired; // Added to distinguish connected vs just paired
    char name[20];
    char sn[20];
} DeviceInfo;

typedef struct {
    uint8_t count;
    DeviceInfo devices[MAX_DEVICES];
} DeviceList;

// Device Status Data (Union to support multiple device types)
// Must be packed to ensure byte-alignment over UART?
// No, serialization handles packing. This is the unpacked struct.
typedef struct {
    uint8_t id;
    uint8_t type;
    int batteryLevel;
    int inputPower;
    int outputPower;
    int remainTime;

    // Flags
    uint8_t acEnabled;
    uint8_t dcEnabled;
    uint8_t usbEnabled;

    // Wave 2 Specific
    uint8_t w2_mode;     // 0=Cool, 1=Heat, 2=Fan
    uint8_t w2_subMode;  // 0=Max, 1=Sleep, 2=Eco, 3=Auto
    int     w2_setTemp;
    int     w2_envTemp;
    int     w2_fanSpeed;

    // Delta Pro 3 Specific
    int     dp3_solarInput;
    int     dp3_acInput;
    int     dp3_acOutput;
    int     dp3_dc12vOutput;
    int     dp3_usbOutput;

    // Alt Charger Specific
    int     alt_batVoltage;
    int     alt_chgCurrent;
    int     alt_altVoltage;
    int     alt_temp;

    // Common Env
    uint8_t brightness; // 10-100

} DeviceStatus;

// Debug Info
typedef struct {
    char wifiSSID[32];
    char ipAddress[16];
    int8_t rssi;
    uint32_t heapFree;
    uint32_t uptime;
    uint8_t clientCount;
    uint8_t bleConnected;
} DebugInfo;


/* --- Function Prototypes --- */
uint8_t calculate_crc8(const uint8_t *data, uint16_t len);

int pack_handshake_message(uint8_t *buffer);
int pack_handshake_ack_message(uint8_t *buffer);
int pack_device_list_message(uint8_t *buffer, DeviceList *list);
int pack_device_list_ack_message(uint8_t *buffer);
int pack_get_device_status_message(uint8_t *buffer, uint8_t id);
int pack_device_status_message(uint8_t *buffer, DeviceStatus *status);
int pack_set_wave2_message(uint8_t *buffer, uint8_t type, int value);
int pack_set_ac_message(uint8_t *buffer, uint8_t enable);
int pack_set_dc_message(uint8_t *buffer, uint8_t enable);
int pack_set_value_message(uint8_t *buffer, uint8_t type, int value);
int pack_power_off_message(uint8_t *buffer);
int pack_get_debug_info_message(uint8_t *buffer);
int pack_debug_info_message(uint8_t *buffer, DebugInfo *info);
int pack_connect_device_message(uint8_t *buffer, uint8_t device_type);
int pack_forget_device_message(uint8_t *buffer, uint8_t device_type);

int unpack_device_list_message(uint8_t *buffer, DeviceList *list);
int unpack_device_status_message(uint8_t *buffer, DeviceStatus *status);
int unpack_debug_info_message(uint8_t *buffer, DebugInfo *info);

#endif
