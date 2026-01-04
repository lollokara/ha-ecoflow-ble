#ifndef ECOFLOW_PROTOCOL_H
#define ECOFLOW_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

// Protocol Constants
#define PROTOCOL_START_BYTE 0xAA
#define PROTOCOL_CMD_HANDSHAKE 0x01
#define PROTOCOL_CMD_HANDSHAKE_ACK 0x02
#define PROTOCOL_CMD_DEVICE_LIST 0x03
#define PROTOCOL_CMD_DEVICE_DATA 0x04 // Stm32Serial uses this for CMD_GET_DEVICE_STATUS response? No, wait.
// Stm32Serial.cpp uses:
// CMD_HANDSHAKE = 0x01
// CMD_DEVICE_LIST = 0x02 (Wait, check Stm32Serial.cpp again)
// CMD_GET_DEVICE_STATUS = 0x24
// CMD_DEVICE_STATUS (Response) = ?? Stm32Serial sends pack_device_status_message.
// Let's verify pack_device_status_message implementation if visible, or assume 0x04 based on my previous overwrite if I was guessing,
// BUT I must match existing. The file EcoflowProtocol.h in ESP32 (C++) defines these? No, it defines Packet class.
// The constants are likely in a shared header or defined locally.
// Let's check Stm32Serial.h if available, or infer from Stm32Serial.cpp usage.

// Stm32Serial.cpp usage:
// cmd == CMD_HANDSHAKE (0x01)
// cmd == CMD_OTA_ACK (0x06)
// cmd == CMD_OTA_NACK (0x15)
// cmd == CMD_GET_DEVICE_STATUS (0x24)
// cmd == CMD_SET_WAVE2 (0x20)
// cmd == CMD_SET_AC (0x21)
// cmd == CMD_SET_DC (0x22)
// cmd == CMD_SET_VALUE (0x40)
// cmd == CMD_POWER_OFF (0x99)
// cmd == CMD_GET_DEBUG_INFO (0x50)
// cmd == CMD_CONNECT_DEVICE (0x60)
// cmd == CMD_FORGET_DEVICE (0x61)

// Stm32Serial.cpp sends:
// pack_handshake_ack_message -> CMD_HANDSHAKE_ACK (0x02) ?
// pack_device_list_message -> CMD_DEVICE_LIST (0x03) ?
// pack_device_status_message -> CMD_DEVICE_STATUS (0x04) ?

// I need to be sure about 0x02, 0x03, 0x04.
// Let's assume standard incrementing or check if I can read `common/protocol.h` or similar if it exists.
// Actually, `EcoflowSTM32F4/lib/EcoFlowComm/ecoflow_protocol.h` IS the shared header for STM32.
// The ESP32 side seems to have hardcoded values or uses a different header `Stm32Serial.h`.
// I'll read `Stm32Serial.h` to get the EXACT values.

// Device Types (match ESP32 DeviceType enum)
#define DEV_TYPE_UNKNOWN 0
#define DEV_TYPE_DELTA_3 1
#define DEV_TYPE_DELTA_PRO_3 2
#define DEV_TYPE_WAVE_2 3
#define DEV_TYPE_ALTERNATOR_CHARGER 4 // Matches types.h

// --- Protocol Structures (Must match ESP32 definitions) ---

#pragma pack(push, 1)

typedef struct {
    uint8_t type;
    uint8_t is_connected;
    uint8_t is_paired;
    char name[32]; // Not used yet
} DeviceListEntry;

typedef struct {
    uint8_t count;
    DeviceListEntry devices[4];
} DeviceListMessage;

typedef struct {
    float batteryLevel;
    float inputPower;
    float outputPower;
    float acInputPower;
    float acOutputPower;
    float dcPortInputPower;
    float solarInputPower;
    float batteryInputPower;
    float batteryOutputPower;
    float dc12vOutputPower;
    float usbcOutputPower;
    float usbc2OutputPower;
    float usbaOutputPower;
    float usba2OutputPower;
    int32_t batteryChargeLimitMin;
    int32_t batteryChargeLimitMax;
    int32_t acChargingSpeed;
    int32_t dcPortState;
    int32_t energyBackup;
    int32_t energyBackupBatteryLevel;
    int32_t cellTemperature;
    uint8_t acOn;
    uint8_t dcOn;
    uint8_t usbOn;
    uint8_t pluggedInAc;
    uint8_t dc12vPort;
    uint8_t acPorts;
} Delta3DataStruct;

typedef struct {
    float batteryLevel;
    float batteryLevelMain;
    float acInputPower;
    float acLvOutputPower;
    float acHvOutputPower;
    float inputPower;
    float outputPower;
    float dc12vOutputPower;
    float dcLvInputPower;
    float dcHvInputPower;
    int32_t dcLvInputState;
    int32_t dcHvInputState;
    float solarLvPower;
    float solarHvPower;
    float usbaOutputPower;
    float usba2OutputPower;
    float usbcOutputPower;
    float usbc2OutputPower;
    int32_t acChargingSpeed;
    int32_t maxAcChargingPower;
    int32_t energyBackup;
    int32_t energyBackupBatteryLevel;
    int32_t batteryChargeLimitMin;
    int32_t batteryChargeLimitMax;
    int32_t cellTemperature;
    uint8_t dc12vPort;
    uint8_t acLvPort;
    uint8_t acHvPort;
    uint8_t gfiMode;

    // New fields
    float expansion1_power;
    float expansion2_power;
    int32_t ac_in_status;
    float bms_batt_soh;
    uint32_t bms_dsg_rem_time;
    uint32_t bms_chg_rem_time;
} DeltaPro3DataStruct;

typedef struct {
    int32_t mode;
    int32_t subMode;
    int32_t setTemp;
    int32_t fanValue;
    float envTemp;
    float outLetTemp;
    int32_t batSoc;
    int32_t batChgStatus;
    uint32_t batChgRemainTime;
    uint32_t batDsgRemainTime;
    uint32_t remainingTime;
    int32_t batPwrWatt;
    int32_t mpptPwrWatt;
    int32_t psdrPwrWatt;
    int32_t tempSys;
    int32_t displayIdleTime;
    int32_t displayIdleMode;
    int32_t timeEn;
    int32_t timeSetVal;
    int32_t timeRemainVal;
    int32_t beepEnable;
    uint32_t errCode;
    int32_t refEn;
    int32_t bmsPid;
    int32_t wteFthEn;
    int32_t tempDisplay;
    int32_t powerMode;
    int32_t powerSrc;
    int32_t mpptWork;
    int32_t bmsErr;
    int32_t rgbState;
    int32_t waterValue;
    int32_t bmsBoundFlag;
    int32_t bmsUndervoltage;
    int32_t ver;
} Wave2DataStruct;

typedef struct {
    float batteryLevel;
    int32_t batteryTemperature;
    float dcPower;
    float carBatteryVoltage;
    float startVoltage;
    int32_t chargerMode;
    uint8_t chargerOpen;
    int32_t powerLimit;
    int32_t powerMax;
    float reverseChargingCurrentLimit;
    float chargingCurrentLimit;
    float reverseChargingCurrentMax;
    float chargingCurrentMax;
} AlternatorChargerDataStruct;

typedef struct {
    uint8_t deviceType;
    union {
        Delta3DataStruct d3;
        DeltaPro3DataStruct d3p;
        Wave2DataStruct w2;
        AlternatorChargerDataStruct ac;
    } data;
    uint8_t brightness; // Added based on Stm32Serial.cpp sendDeviceStatus
} DeviceStatus;

// --- Command Macros ---
// Delta 3 / Pro 3
#define SET_VAL_AC_ENABLED 1
#define SET_VAL_DC_ENABLED 2
#define SET_VAL_USB_ENABLED 3 // D3 only
#define SET_VAL_AC_CHG_SPEED 4
#define SET_VAL_AC_ALWAYS_ON 5 // Not used yet
#define SET_VAL_UP_LIMIT 6
#define SET_VAL_DOWN_LIMIT 7
#define SET_VAL_BACKUP_RESERVE 8

// Alternator Charger
#define SET_VAL_ALT_ENABLE 9
#define SET_VAL_ALT_MODE 10
#define SET_VAL_ALT_START_VOLTAGE 11
#define SET_VAL_ALT_REV_CHG_CURRENT 12
#define SET_VAL_ALT_CHG_CURRENT 13
#define SET_VAL_ALT_POWER_LIMIT 14

// Wave 2
#define W2_PARAM_POWER 20
#define W2_PARAM_MODE 21
#define W2_PARAM_TEMP 22
#define W2_PARAM_FAN 23
#define W2_PARAM_SUBMODE 24

#pragma pack(pop)

#endif // ECOFLOW_PROTOCOL_H
