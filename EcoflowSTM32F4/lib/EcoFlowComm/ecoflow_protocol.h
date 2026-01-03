#ifndef ECOFLOW_PROTOCOL_H
#define ECOFLOW_PROTOCOL_H

/**
 * @file ecoflow_protocol.h
 * @author Lollokara
 * @brief Shared protocol definitions for ESP32 and STM32F4 communication.
 *
 * This file defines the packet structure, command IDs, and data structures
 * used for the UART communication between the BLE Gateway (ESP32) and the
 * Display Controller (STM32F4).
 *
 * @note This file MUST be identical in both projects.
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Protocol constants
#define START_BYTE 0xAA      ///< Packet Start Byte
#define MAX_PAYLOAD_LEN 255  ///< Maximum payload size
#define MAX_DEVICES 4        ///< Maximum supported devices

// Message Format: [START][CMD][LEN][PAYLOAD][CRC8]

// --- ESP32 -> F4 Command IDs ---
#define CMD_BATTERY_STATUS 0x01      ///< Legacy: Simple Battery Status
#define CMD_TEMPERATURE 0x02         ///< Legacy: Temperature
#define CMD_CONNECTION_STATE 0x03    ///< Legacy: Connection State

#define CMD_HANDSHAKE_ACK 0x21       ///< Handshake Acknowledgment
#define CMD_DEVICE_LIST 0x22         ///< Push Device List to STM32
#define CMD_DEVICE_STATUS 0x24       ///< Send Device Telemetry Data
#define CMD_DEBUG_INFO 0x61          ///< Send Debug Info (IP, uptime)

#define CMD_OTA_START 0xA0           ///< Start OTA Update
#define CMD_OTA_CHUNK 0xA1           ///< OTA Data Chunk
#define CMD_OTA_END   0xA2           ///< End OTA Update
#define CMD_OTA_APPLY 0xA3           ///< Apply OTA Update

// --- F4 -> ESP32 Command IDs ---
#define CMD_REQUEST_STATUS_UPDATE 0x10 ///< Request immediate update (Generic)

#define CMD_HANDSHAKE 0x20           ///< Initiate Handshake
#define CMD_DEVICE_LIST_ACK 0x23     ///< Acknowledge Device List reception
#define CMD_GET_DEVICE_STATUS 0x25   ///< Request Status for specific device
#define CMD_GET_DEBUG_INFO 0x60      ///< Request Debug Info
#define CMD_CONNECT_DEVICE 0x62      ///< Request to connect to a device type
#define CMD_FORGET_DEVICE 0x63       ///< Request to forget a device

#define CMD_OTA_ACK  0x06            ///< OTA Acknowledge
#define CMD_OTA_NACK 0x15            ///< OTA Negative Acknowledge

// --- Control Commands (F4 -> ESP32) ---
#define CMD_SET_WAVE2 0x30           ///< Control Wave 2 (Temp, Mode)
#define CMD_SET_AC 0x31              ///< Toggle AC Ports
#define CMD_SET_DC 0x32              ///< Toggle DC Ports
#define CMD_SET_VALUE 0x40           ///< Set Numeric Value (Limits)
#define CMD_POWER_OFF 0x50           ///< Trigger System Power Off

// Wave 2 Set Types (Renamed to avoid conflict with DisplayAction enum)
#define W2_PARAM_TEMP 1
#define W2_PARAM_MODE 2
#define W2_PARAM_SUB_MODE 3
#define W2_PARAM_FAN 4
#define W2_PARAM_POWER 5

// Set Value Types
#define SET_VAL_AC_LIMIT 1
#define SET_VAL_MAX_SOC 2
#define SET_VAL_MIN_SOC 3
// Alternator Charger Set Values
#define SET_VAL_ALT_START_VOLTAGE 4
#define SET_VAL_ALT_MODE 5
#define SET_VAL_ALT_PROD_LIMIT 6
#define SET_VAL_ALT_REV_LIMIT 7
#define SET_VAL_ALT_CHG_LIMIT 8

// Device Types (matching types.h)
#define DEV_TYPE_DELTA_3 1
#define DEV_TYPE_DELTA_PRO_3 2
#define DEV_TYPE_WAVE_2 3
#define DEV_TYPE_ALT_CHARGER 4


#pragma pack(push, 1)

// --- Shared Data Structures (POD versions of EcoflowData.h) ---

/**
 * @brief Telemetry data for Delta 3.
 */
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

/**
 * @brief Telemetry data for Wave 2.
 */
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

/**
 * @brief Telemetry data for Delta Pro 3.
 */
typedef struct {
    float batteryLevel;
    float batteryLevelMain; // Added field
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

/**
 * @brief Telemetry data for Alternator Charger.
 */
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


/**
 * @brief Legacy Battery Status (Fallback).
 */
typedef struct {
    uint8_t soc;
    int16_t power_w;
    uint16_t voltage_v;
    uint8_t connected;
    char device_name[16];
} BatteryStatus;

/**
 * @brief Union to hold data based on device type.
 */
typedef union {
    Delta3DataStruct d3;
    Wave2DataStruct w2;
    DeltaPro3DataStruct d3p;
    AlternatorChargerDataStruct ac;
    BatteryStatus legacy; // Fallback
} DeviceSpecificData;

/**
 * @brief Payload for CMD_DEVICE_STATUS.
 */
typedef struct {
    uint8_t id;          // DeviceType enum
    uint8_t connected;
    char name[16];
    uint8_t brightness;  // 10-100%
    DeviceSpecificData data;
} DeviceStatus;

/**
 * @brief Payload for CMD_DEVICE_LIST.
 */
typedef struct {
    uint8_t count;
    struct {
        uint8_t id;
        char name[16];
        uint8_t connected;
        uint8_t paired;
    } devices[MAX_DEVICES];
} DeviceList;

/**
 * @brief Payload for CMD_SET_WAVE2.
 */
typedef struct {
    uint8_t type;  // W2_PARAM_TEMP, etc.
    uint8_t value;
} Wave2SetMsg;

/**
 * @brief Payload for CMD_DEBUG_INFO.
 */
typedef struct {
    char ip[16];
    uint8_t wifi_connected;
    uint8_t devices_connected;
    uint8_t devices_paired;
} DebugInfo;

// OTA Payload Structures
typedef struct {
    uint32_t total_size;
} OtaStartMsg;

typedef struct {
    uint32_t offset;
    // Data follows
} OtaChunkHeader;

#pragma pack(pop)

// --- API Functions (Serialization/Deserialization) ---

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

int pack_set_wave2_message(uint8_t *buffer, uint8_t type, uint8_t value);
int unpack_set_wave2_message(const uint8_t *buffer, uint8_t *type, uint8_t *value);

int pack_set_ac_message(uint8_t *buffer, uint8_t enable);
int unpack_set_ac_message(const uint8_t *buffer, uint8_t *enable);

int pack_set_dc_message(uint8_t *buffer, uint8_t enable);
int unpack_set_dc_message(const uint8_t *buffer, uint8_t *enable);

int pack_set_value_message(uint8_t *buffer, uint8_t type, int value);
int unpack_set_value_message(const uint8_t *buffer, uint8_t *type, int *value);

int pack_power_off_message(uint8_t *buffer);

int pack_get_debug_info_message(uint8_t *buffer);
int pack_debug_info_message(uint8_t *buffer, const DebugInfo *info);
int unpack_debug_info_message(const uint8_t *buffer, DebugInfo *info);

int pack_connect_device_message(uint8_t *buffer, uint8_t device_type);
int unpack_connect_device_message(const uint8_t *buffer, uint8_t *device_type);

int pack_forget_device_message(uint8_t *buffer, uint8_t device_type);
int unpack_forget_device_message(const uint8_t *buffer, uint8_t *device_type);

int pack_ota_start_message(uint8_t *buffer, uint32_t total_size);
int pack_ota_chunk_message(uint8_t *buffer, uint32_t offset, const uint8_t *data, uint8_t len);
int pack_ota_end_message(uint8_t *buffer, uint32_t crc32);
int pack_ota_apply_message(uint8_t *buffer);

#ifdef __cplusplus
}
#endif

#endif // ECOFLOW_PROTOCOL_H