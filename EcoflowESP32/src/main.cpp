
// ... (includes)
#include <Arduino.h>
#include "EcoflowESP32.h"
#include "Credentials.h"
#include "DeviceManager.h"
#include "CmdUtils.h"
#include "WebServer.h"
#include "LightSensor.h"
#include "ecoflow_protocol.h"
#include "Stm32Serial.h"

// Timings for button press detection (in milliseconds)
// #define DEBOUNCE_DELAY 50
// #define HOLD_PRESS_TIME 1000
static const char* TAG = "Main";

#define POWER_LATCH_PIN 16

void checkSerial() {
    static String inputBuffer = "";
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n') {
            CmdUtils::processInput(inputBuffer);
            inputBuffer = "";   
        } else if (c >= 32 && c <= 126) {
            inputBuffer += c;
        }
    }
}

void setup() {
    pinMode(POWER_LATCH_PIN, INPUT_PULLUP);

    Serial.begin(115200);
    Serial.println("Starting Ecoflow Controller...");

    LightSensor::getInstance().begin();
    DeviceManager::getInstance().initialize();

    WebServer::begin();

    // Init the new serial handler
    Stm32Serial::getInstance().begin();
}

void loop() {
    static uint32_t last_data_refresh = 0;
    static uint32_t last_device_list_update = 0;

    LightSensor::getInstance().update();
    DeviceManager::getInstance().update();
    checkSerial();

    // Process UART with the new handler
    Stm32Serial::getInstance().update();

    if (millis() - last_data_refresh > 2000) {
        last_data_refresh = millis();

        EcoflowESP32* d3 = DeviceManager::getInstance().getDevice(DeviceType::DELTA_3);
        if (d3 && d3->isAuthenticated()) d3->requestData();
        EcoflowESP32* w2 = DeviceManager::getInstance().getDevice(DeviceType::WAVE_2);
        if (w2 && w2->isAuthenticated()) w2->requestData();
        EcoflowESP32* d3p = DeviceManager::getInstance().getDevice(DeviceType::DELTA_PRO_3);
        if (d3p && d3p->isAuthenticated()) d3p->requestData();
    }

    // Periodically update device list (every 5 seconds) to handle disconnects/reconnects
    if (millis() - last_device_list_update > 5000) {
        last_device_list_update = millis();
        Stm32Serial::getInstance().sendDeviceList();
    }
}
