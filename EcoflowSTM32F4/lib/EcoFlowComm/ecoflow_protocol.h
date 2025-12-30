#ifndef ECOFLOW_PROTOCOL_H
#define ECOFLOW_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

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
#define CMD_SET_VALUE 0x40

// Device Types (matching types.h)
#define DEV_TYPE_DELTA_3 1
#define DEV_TYPE_DELTA_PRO_3 2
#define DEV_TYPE_WAVE_2 3
#define DEV_TYPE_ALT_CHARGER 4


#pragma pack(push, 1)

// Shared Data Structures (POD versions of EcoflowData.h)

typedef struct {
    float batteryLevel;
    float acInputPower;
    float acOutputPower;
    float inputPower;
    float outputPower;
    float dc12vOutputPower;
    float dcPortInputPower;
    int32_t dcPortState;
    float usbcOutputPower;
    float usbc2OutputPower;
    float usbaOutputPower;
    float usba2OutputPower;
    bool pluggedInAc;
    bool energyBackup;
    int32_t energyBackupBatteryLevel;
    float batteryInputPower;
    float batteryOutputPower;
    int32_t batteryChargeLimitMin;
    int32_t batteryChargeLimitMax;
    int32_t cellTemperature;
    bool dc12vPort;
    bool acPorts;
    float solarInputPower;
    int32_t acChargingSpeed;
    int32_t maxAcChargingPower;
    bool acOn;
    bool dcOn;
    bool usbOn;
} Delta3DataStruct;

typedef struct {
    int32_t mode;
    int32_t subMode;
    int32_t setTemp;
    int32_t fanValue;
    float envTemp;
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
    int32_t psdrPwrWatt;
    int32_t batPwrWatt;
    int32_t mpptPwrWatt;
    uint32_t batDsgRemainTime;
    uint32_t batChgRemainTime;
    int32_t batSoc;
    int32_t batChgStatus;
    float outLetTemp;
    int32_t mpptWork;
    int32_t bmsErr;
    int32_t rgbState;
    int32_t waterValue;
    int32_t bmsBoundFlag;
    int32_t bmsUndervoltage;
    int32_t ver;
    int32_t remainingTime;
} Wave2DataStruct;

typedef struct {
    float batteryLevel;
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
    float usbcOutputPower;
    float usbc2OutputPower;
    float usbaOutputPower;
    float usba2OutputPower;
    int32_t acChargingSpeed;
    int32_t maxAcChargingPower;
    bool pluggedInAc;
    bool energyBackup;
    int32_t energyBackupBatteryLevel;
    int32_t batteryChargeLimitMin;
    int32_t batteryChargeLimitMax;
    int32_t cellTemperature;
    bool dc12vPort;
    bool acLvPort;
    bool acHvPort;
    float solarLvPower;
    float solarHvPower;
    bool gfiMode;
} DeltaPro3DataStruct;

typedef struct {
    float batteryLevel;
    float batteryTemperature;
    float dcPower;
    float carBatteryVoltage;
    float startVoltage;
    int32_t startVoltageMin;
    int32_t startVoltageMax;
    int32_t chargerMode;
    bool chargerOpen;
    int32_t powerLimit;
    int32_t powerMax;
    float reverseChargingCurrentLimit;
    float chargingCurrentLimit;
    float reverseChargingCurrentMax;
    float chargingCurrentMax;
} AlternatorChargerDataStruct;


// Legacy BatteryStatus for backward compatibility (if needed) or simple views
typedef struct {
    uint8_t soc;
    int16_t power_w;
    uint16_t voltage_v;
    uint8_t connected;
    char device_name[16];
} BatteryStatus;

// Union to hold data based on device type
typedef union {
    Delta3DataStruct d3;
    Wave2DataStruct w2;
    DeltaPro3DataStruct d3p;
    AlternatorChargerDataStruct ac;
    BatteryStatus legacy; // Fallback
} DeviceSpecificData;

// The payload sent over UART
typedef struct {
    uint8_t id;          // DeviceType enum
    uint8_t connected;
    char name[16];
    DeviceSpecificData data;
} DeviceStatus;

typedef struct {
    uint8_t device_type;
    uint8_t command_id;
    uint8_t value_type; // 0=int, 1=float
    union {
        int32_t int_val;
        float float_val;
    } value;
} SetValueCommand;

// Set Value Command IDs
// Generic / Delta 3
#define SET_CMD_AC_ENABLE 1
#define SET_CMD_DC_ENABLE 2
#define SET_CMD_USB_ENABLE 3
// Wave 2
#define SET_CMD_W2_POWER 10
#define SET_CMD_W2_TEMP 11
#define SET_CMD_W2_MODE 12
#define SET_CMD_W2_SUBMODE 13
#define SET_CMD_W2_FAN 14

typedef struct {
    uint8_t count;
    struct {
        uint8_t id;
        char name[16];
        uint8_t connected;
    } devices[MAX_DEVICES];
} DeviceList;

#pragma pack(pop)

// API Functions
uint8_t calculate_crc8(const uint8_t *data, uint8_t len);

int pack_handshake_message(uint8_t *buffer);
int pack_handshake_ack_message(uint8_t *buffer);

int pack_device_list_message(uint8_t *buffer, const DeviceList *list);
int unpack_device_list_message(const uint8_t *buffer, DeviceList *list);
int pack_device_list_ack_message(uint8_t *buffer);

int pack_get_device_status_message(uint8_t *buffer, uint8_t device_id);
int unpack_get_device_status_message(const uint8_t *buffer, uint8_t *device_id);

int pack_device_status_message(uint8_t *buffer, const DeviceStatus *status);
int unpack_device_status_message(const uint8_t *buffer, DeviceStatus *status);

int pack_set_value_message(uint8_t *buffer, const SetValueCommand *cmd);
int unpack_set_value_message(const uint8_t *buffer, SetValueCommand *cmd);


#ifdef __cplusplus
}
#endif

#endif // ECOFLOW_PROTOCOL_H
