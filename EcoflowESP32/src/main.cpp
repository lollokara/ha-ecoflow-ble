#include <Arduino.h>
#include "EcoflowESP32.h"
#include "Credentials.h"

EcoflowESP32 ecoflow;

// Replace with your device's BLE address
const std::string ble_address = "7c:2c:67:44:a4:3e";

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        ; // wait for serial port to connect. Needed for native USB
    }
    Serial.println("Starting...");

    ecoflow.begin(ECOFLOW_USER_ID, ECOFLOW_DEVICE_SN, ble_address);
}

void loop() {
    static uint32_t last_battery_check = 0;
    static uint32_t last_ac_toggle = 0;
    static bool ac_on = false;

    ecoflow.update();

    if (millis() - last_battery_check > 5000) {
        last_battery_check = millis();
        if (ecoflow.isAuthenticated()) {
            ecoflow.getBatteryLevel();
        }
    }

    if (millis() - last_ac_toggle > 15000) {
        last_ac_toggle = millis();
        if (ecoflow.isAuthenticated()) {
            ac_on = !ac_on;
            ESP_LOGI("main", "Toggling AC to %s", ac_on ? "ON" : "OFF");
            ecoflow.setAC(ac_on);
        }
    }

    delay(100);
}
