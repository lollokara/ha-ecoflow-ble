#include <Arduino.h>
#include <esp_task_wdt.h>
#include "EcoflowESP32.h"
#include "Credentials.h"
#include "DeviceManager.h"
#include "CmdUtils.h"
#include "WebServer.h"
#include "LightSensor.h"
#include "ecoflow_protocol.h"
#include "Stm32Serial.h"
#include "OtaManager.h"

// Hardware Pin Definitions
#define POWER_LATCH_PIN 16

// Logging Tag
static const char* TAG = "Main";

// Global Instances
Stm32Serial* stm32Serial = nullptr;
OtaManager* otaManager = nullptr;

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
    esp_task_wdt_init(10, true);
    esp_task_wdt_add(NULL);

    pinMode(POWER_LATCH_PIN, OUTPUT);
    digitalWrite(POWER_LATCH_PIN, LOW);

    Serial.begin(115200);
    Serial.println("Starting Ecoflow Controller...");

    LightSensor::getInstance().begin();
    DeviceManager::getInstance().initialize();
    WebServer::begin();

    // Instantiate Serial and OTA Manager
    stm32Serial = new Stm32Serial(&Serial1, nullptr);
    otaManager = new OtaManager(stm32Serial);
    stm32Serial->setOtaManager(otaManager);

    stm32Serial->begin(115200);
}

void loop() {
    esp_task_wdt_reset();

    static uint32_t last_data_refresh = 0;
    static uint32_t last_device_list_update = 0;

    LightSensor::getInstance().update();
    DeviceManager::getInstance().update();
    checkSerial();

    // Update Stm32Serial and OtaManager
    if (stm32Serial) stm32Serial->handle();
    if (otaManager) otaManager->handle();

    // Poll connected devices for data every 2 seconds
    if (millis() - last_data_refresh > 2000) {
        last_data_refresh = millis();

        EcoflowESP32* d3 = DeviceManager::getInstance().getDevice(DeviceType::DELTA_3);
        if (d3 && d3->isAuthenticated()) d3->requestData();

        EcoflowESP32* w2 = DeviceManager::getInstance().getDevice(DeviceType::WAVE_2);
        if (w2 && w2->isAuthenticated()) w2->requestData();

        EcoflowESP32* d3p = DeviceManager::getInstance().getDevice(DeviceType::DELTA_PRO_3);
        if (d3p && d3p->isAuthenticated()) d3p->requestData();
    }

    // Periodically broadcast the device list (every 5 seconds)
    if (millis() - last_device_list_update > 5000) {
        last_device_list_update = millis();
        // Pause explicit broadcast if OTA is running to avoid UART contention
        if (otaManager && otaManager->getState() == OtaManager::OTA_IDLE) {
             if (stm32Serial) stm32Serial->sendDeviceList();
        }
    }
}
