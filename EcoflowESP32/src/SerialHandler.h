#ifndef SERIAL_HANDLER_H
#define SERIAL_HANDLER_H

#include <Arduino.h>
#include "ecoflow_protocol.h"
#include "DeviceManager.h"
#include "LightSensor.h"

class SerialHandler {
public:
    SerialHandler();
    void begin();
    void update();
    void sendDeviceStatus(uint8_t device_id);

private:
    void checkUart();
    void sendDeviceList();
    void handlePacket(uint8_t* buffer, uint8_t len);

    // Buffers and State
    uint8_t rx_buf[1024];
    uint16_t rx_idx;
    uint8_t expected_len;
    bool collecting;
    uint32_t last_device_list_update;

    DeviceType currentViewDevice;
};

#endif // SERIAL_HANDLER_H
