#ifndef STM32_SERIAL_H
#define STM32_SERIAL_H

#include <Arduino.h>
#include "BrainTransplantProtocol.h"
#include <freertos/semphr.h>

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
