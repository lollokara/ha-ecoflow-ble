#include <Arduino.h>
#include "EcoflowESP32.h"
#include "Credentials.h"
#include "DeviceManager.h"
#include "CmdUtils.h"
#include "WebServer.h"
#include "LightSensor.h"
#include "SerialHandler.h"

#define POWER_LATCH_PIN 12

static const char* TAG = "Main";

SerialHandler serialHandler;

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
    pinMode(POWER_LATCH_PIN, OUTPUT);
    digitalWrite(POWER_LATCH_PIN, HIGH);

    Serial.begin(115200);
    Serial.println("Starting Ecoflow Controller...");

    LightSensor::getInstance().begin();
    DeviceManager::getInstance().initialize();

    WebServer::begin();
    serialHandler.begin();
}

void loop() {
    static uint32_t last_data_refresh = 0;

    LightSensor::getInstance().update();
    DeviceManager::getInstance().update();
    checkSerial();
    serialHandler.update();

    if (millis() - last_data_refresh > 2000) {
        last_data_refresh = millis();

        EcoflowESP32* d3 = DeviceManager::getInstance().getDevice(DeviceType::DELTA_3);
        if (d3 && d3->isAuthenticated()) d3->requestData();
        EcoflowESP32* w2 = DeviceManager::getInstance().getDevice(DeviceType::WAVE_2);
        if (w2 && w2->isAuthenticated()) w2->requestData();
        EcoflowESP32* d3p = DeviceManager::getInstance().getDevice(DeviceType::DELTA_PRO_3);
        if (d3p && d3p->isAuthenticated()) d3p->requestData();
    }
}
