#ifndef STM32_SERIAL_H
#define STM32_SERIAL_H

/**
 * @file Stm32Serial.h
 * @author Lollokara
 * @brief Header for the Stm32Serial class.
 *
 * Defines the singleton class responsible for managing the UART connection
 * to the STM32F4 bootloader.
 */

#include <Arduino.h>
#include <freertos/semphr.h>

// Protocol Constants
#define START_BYTE 0xAA
#define CMD_HANDSHAKE 0x01
#define CMD_OTA_START 0xA0
#define CMD_OTA_CHUNK 0xA1
#define CMD_OTA_END 0xA2
#define CMD_OTA_APPLY 0xA3
#define CMD_OTA_ACK 0x06
#define CMD_OTA_NACK 0x15

class Stm32Serial {
public:
    static Stm32Serial& getInstance() {
        static Stm32Serial instance;
        return instance;
    }

    void begin();
    void update();
    void startOta(const String& filename);
    bool isOtaInProgress() const { return _otaRunning; }
    void sendData(const uint8_t* data, size_t len);

private:
    Stm32Serial() {}
    void processPacket(uint8_t* buf, uint8_t len);
    static void otaTask(void* parameter);

    bool _otaRunning = false;
    SemaphoreHandle_t _txMutex = NULL;
};

#endif
