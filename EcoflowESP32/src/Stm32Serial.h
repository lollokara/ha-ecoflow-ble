#ifndef STM32_SERIAL_H
#define STM32_SERIAL_H

#include <Arduino.h>
#include "ecoflow_protocol.h"

class Stm32Serial {
public:
    static Stm32Serial& getInstance() {
        static Stm32Serial instance;
        return instance;
    }

    void begin();
    void update();
    void sendDeviceList();
    void sendDeviceStatus(uint8_t device_id);

private:
    Stm32Serial() {}
    void processPacket(uint8_t* buf, uint8_t len);
    uint8_t calculateBrightness();
};

#endif
