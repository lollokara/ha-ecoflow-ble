#ifndef STM32SERIAL_H
#define STM32SERIAL_H

#include <Arduino.h>
#include <vector>
#include "EcoflowProtocol.h"

// Command IDs
#define CMD_HANDSHAKE 0x01
#define CMD_HANDSHAKE_ACK 0x02
#define CMD_DEVICE_LIST 0x03
#define CMD_DEVICE_STATUS 0x04 // Used for sending status to STM32

#define CMD_OTA_START 0xA0
#define CMD_OTA_CHUNK 0xA1
#define CMD_OTA_END   0xA2
#define CMD_OTA_APPLY 0xA3
#define CMD_OTA_ACK   0x06
#define CMD_OTA_NACK  0x15

#define CMD_SET_WAVE2 0x20
#define CMD_SET_AC    0x21
#define CMD_SET_DC    0x22
#define CMD_GET_DEVICE_STATUS 0x24 // Used for requesting status from ESP32

#define CMD_SET_VALUE 0x40

#define CMD_GET_DEBUG_INFO 0x50
#define CMD_CONNECT_DEVICE 0x60
#define CMD_FORGET_DEVICE  0x61

#define CMD_POWER_OFF 0x99

#define START_BYTE 0xBB // Updated Start Byte

typedef struct {
    char ip[16];
    uint8_t devices_connected;
    uint8_t devices_paired;
} DebugInfo;

typedef struct {
    uint8_t id;
    char name[16];
    uint8_t connected;
    uint8_t paired;
} DeviceEntry;

typedef struct {
    uint8_t count;
    DeviceEntry devices[4];
} DeviceList;

class Stm32Serial {
public:
    static Stm32Serial& getInstance() {
        static Stm32Serial instance;
        return instance;
    }

    void begin();
    void update();
    void sendData(const uint8_t* data, size_t len);

    void sendDeviceList();
    void sendDeviceStatus(uint8_t device_id);

    void startOta(const String& filename);
    bool isOtaRunning() { return _otaRunning; }

private:
    Stm32Serial() : _otaRunning(false), _txMutex(NULL) {}
    void processPacket(uint8_t* data, uint8_t len);

    // Packet packing helpers (implemented in .cpp or inline)
    int pack_handshake_ack_message(uint8_t* buf) {
        buf[0] = START_BYTE; buf[1] = CMD_HANDSHAKE_ACK; buf[2] = 0; buf[3] = calculate_crc8(&buf[1], 2);
        return 4;
    }
    // ... other pack functions are assumed to be implemented or macros

    static void otaTask(void* parameter);
    bool _otaRunning;
    SemaphoreHandle_t _txMutex;
};

// CRC Helper
uint8_t calculate_crc8(const uint8_t *data, size_t len);

// Prototypes for pack/unpack (usually in a separate c/cpp file or helper)
int pack_device_list_message(uint8_t* buffer, DeviceList* list);
int pack_device_status_message(uint8_t* buffer, void* status); // void* to avoid circular dependency with DeviceStatus struct if not defined here
int pack_debug_info_message(uint8_t* buffer, DebugInfo* info);
int pack_ota_start_message(uint8_t* buffer, uint32_t size);
int pack_ota_chunk_message(uint8_t* buffer, uint32_t offset, uint8_t* data, uint8_t len);
int pack_ota_end_message(uint8_t* buffer, uint32_t crc);
int pack_ota_apply_message(uint8_t* buffer);

int unpack_get_device_status_message(uint8_t* buffer, uint8_t* dev_id);
int unpack_set_wave2_message(uint8_t* buffer, uint8_t* type, uint8_t* value);
int unpack_set_ac_message(uint8_t* buffer, uint8_t* enable);
int unpack_set_dc_message(uint8_t* buffer, uint8_t* enable);
int unpack_set_value_message(uint8_t* buffer, uint8_t* type, int* value);
int unpack_connect_device_message(uint8_t* buffer, uint8_t* type);
int unpack_forget_device_message(uint8_t* buffer, uint8_t* type);

#endif
