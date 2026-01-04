#ifndef STM32SERIAL_H
#define STM32SERIAL_H

#include <Arduino.h>
#include <vector>
#include "EcoflowProtocol.h" // C++ Classes
#include "ecoflow_protocol.h" // C Structs & Macros

// Use macros from ecoflow_protocol.h
// Clean up Redundant Definitions

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
    // Removed inline CRC usage to fix scope error

    static void otaTask(void* parameter);
    bool _otaRunning;
    SemaphoreHandle_t _txMutex;
};

// Functions are now declared in ecoflow_protocol.h, so we rely on that.
// Ensure calculate_crc8 is available or removed if provided by lib.
// The lib header declares `uint8_t calculate_crc8(const uint8_t *data, uint8_t len);`
// So we don't redeclare it.

#endif
