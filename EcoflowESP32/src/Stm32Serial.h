#ifndef STM32_SERIAL_H
#define STM32_SERIAL_H

#include <Arduino.h>
#include "OtaManager.h" // Forward declaration issue might arise

class OtaManager;

class Stm32Serial {
public:
    Stm32Serial(HardwareSerial* serial, OtaManager* ota);
    void begin(unsigned long baud);
    void handle();

    // OTA Methods
    void sendOtaStart(uint32_t size);
    void sendOtaChunk(uint32_t offset, uint8_t* data, uint8_t len);
    void sendOtaEnd();
    void sendOtaApply();

    // Basic Packet
    void sendPacket(uint8_t cmd, uint8_t* payload, uint8_t len);

private:
    HardwareSerial* _serial;
    OtaManager* _ota;
    uint8_t crc8(uint8_t *data, int len);
    uint8_t lastSentCmd = 0;
};

#endif
