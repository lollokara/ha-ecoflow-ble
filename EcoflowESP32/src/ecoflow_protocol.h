#ifndef ECOFLOW_PROTOCOL_H
#define ECOFLOW_PROTOCOL_H

#include <stdint.h>

// Protocol Macros (Shared between ESP32 and STM32)

#define PROTOCOL_START_BYTE 0xAA
#define PROTOCOL_CMD_HANDSHAKE 0x20
#define PROTOCOL_CMD_HANDSHAKE_ACK 0x21
#define PROTOCOL_CMD_DEVICE_LIST 0x22
#define PROTOCOL_CMD_DEVICE_LIST_ACK 0x23
#define PROTOCOL_CMD_DEVICE_STATUS 0x24
#define PROTOCOL_CMD_GET_DEVICE_STATUS 0x25
#define PROTOCOL_CMD_GET_DEVICE_LIST 0x26
#define PROTOCOL_CMD_SET_WAVE2 0x30
#define PROTOCOL_CMD_SET_AC 0x31
#define PROTOCOL_CMD_SET_DC 0x32
#define PROTOCOL_CMD_SET_VALUE 0x40
#define PROTOCOL_CMD_POWER_OFF 0x50

#define PROTOCOL_CMD_DEBUG_INFO 0x60
#define PROTOCOL_CMD_DEBUG_INFO_DATA 0x61
#define PROTOCOL_CMD_CONNECT_DEVICE 0x62
#define PROTOCOL_CMD_FORGET_DEVICE 0x63

// OTA Commands
#define PROTOCOL_CMD_OTA_START 0x80
#define PROTOCOL_CMD_OTA_CHUNK 0x81
#define PROTOCOL_CMD_OTA_END   0x82
#define PROTOCOL_CMD_OTA_ACK   0x83
#define PROTOCOL_CMD_OTA_NACK  0x84

// Device Types
#define DEVICE_TYPE_NONE 0x00
#define DEVICE_TYPE_DELTA_3 0x01
#define DEVICE_TYPE_DELTA_PRO_3 0x02
#define DEVICE_TYPE_WAVE_2 0x03
#define DEVICE_TYPE_ALTERNATOR_CHARGER 0x04

// Max number of devices
#define MAX_DEVICES 4

// Structures

// Device List Item (Packed)
typedef struct __attribute__((packed)) {
    uint8_t slot;       // 0-3
    uint8_t type;       // DeviceType
    char sn[16];        // Serial Number (Fixed 16 chars)
    uint8_t is_connected;
    uint8_t battery_level;
    uint8_t paired;     // 1 if paired/known
} DeviceListItem;

// Device List Message (Packed)
typedef struct __attribute__((packed)) {
    uint8_t count;
    DeviceListItem devices[MAX_DEVICES];
} DeviceListMessage;

// Delta 3 Data Structure (Binary optimized for UART)
typedef struct __attribute__((packed)) {
    int32_t totalInputPower;
    int32_t totalOutputPower;
    int32_t solarInputPower;
    int32_t acOutputPower;
    int32_t dc12vOutputPower;
    int32_t usbcOutputPower;
    int32_t usbc2OutputPower;
    int32_t usbaOutputPower;
    int32_t usba2OutputPower;
    uint8_t acSwitch;
    uint8_t dcSwitch;
    uint8_t usbSwitch;
    uint16_t acChgLimit;
    uint8_t maxChgSoc;
    uint8_t minDsgSoc;
    uint8_t cellTemperature;
} Delta3DataStruct;

// Wave 2 Data Structure (Binary V2)
typedef struct __attribute__((packed)) {
    float envTemp;
    float outLetTemp;
    float setTemp;
    uint8_t mode; // 0=Cool, 1=Heat, 2=Fan
    uint8_t subMode; // 0=Max, 1=Sleep, 2=Eco, 3=Auto
    uint8_t fanValue;
    uint8_t powerMode; // 1=Main ON, 2=Main OFF
    uint8_t wteFthEn; // Drain
    uint8_t rgbState;
    uint8_t beepEnable;
    int32_t batPwrWatt;
    int32_t mpptPwrWatt;
    int32_t psdrPwrWatt;
} Wave2DataStruct;

// Delta Pro 3 Data Structure
typedef struct __attribute__((packed)) {
    int32_t solarLvPower;
    int32_t solarHvPower;
    int32_t dc12vOutputPower;
    int32_t acLvOutputPower;
    int32_t acHvOutputPower;
    int32_t dcLvInputPower; // Alternator input
    uint8_t acHvPort; // AC Output State
    uint8_t dc12vPort; // DC Output State
    uint8_t energyBackup; // Backup Enabled
    uint8_t energyBackupBatteryLevel; // Backup Level
    uint8_t cellTemperature;
    uint8_t gfiMode;
} DeltaPro3DataStruct;

// Alternator Charger Data Structure
typedef struct __attribute__((packed)) {
    float carBatteryVoltage;
    uint32_t powerLimit;
    uint8_t chargerOpen;
    uint8_t chargerMode; // 0=Idle, 1=Charge, 2=Maintenance, 3=Reverse
} AlternatorChargerDataStruct;


// Union of Device Specific Data
typedef union {
    Delta3DataStruct delta3;
    Wave2DataStruct wave2;
    DeltaPro3DataStruct deltaPro3;
    AlternatorChargerDataStruct alternatorCharger;
} DeviceSpecificData;

// Device Status Message (Packed)
typedef struct __attribute__((packed)) {
    uint8_t slot;
    uint8_t type;
    uint8_t battery_level;
    uint8_t brightness; // 0-100% ambient light
    DeviceSpecificData data;
} DeviceStatusMessage;

// Handshake Message
typedef struct __attribute__((packed)) {
    uint32_t magic; // 0xDEADBEEF
} HandshakeMessage;

// Helper function declarations (implemented in .c/.cpp)
#ifdef __cplusplus
extern "C" {
#endif

void pack_device_list_message(const DeviceListMessage* msg, uint8_t* buffer);
void unpack_device_list_message(const uint8_t* buffer, DeviceListMessage* msg);

void pack_device_status_message(const DeviceStatusMessage* msg, uint8_t* buffer);
void unpack_device_status_message(const uint8_t* buffer, DeviceStatusMessage* msg);

#ifdef __cplusplus
}
#endif

#endif // ECOFLOW_PROTOCOL_H
