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
    static uint32_t last_check = 0;
    static uint32_t last_dc_toggle = 0;
    static uint32_t last_usb_toggle = 0;
    static uint32_t last_ac_toggle = 0;
    static bool dc_on = false;
    static bool usb_on = false;
    static bool ac_on = false;

    ecoflow.update();

    if (millis() - last_check > 5000) {
        last_check = millis();
        if (ecoflow.isAuthenticated()) {
            // Print sensor data
            ESP_LOGI("main", "--- Sensor Data ---");
            ESP_LOGI("main", "Battery: %d%%", ecoflow.getBatteryLevel());
            ESP_LOGI("main", "Input: %dW, Output: %dW", ecoflow.getInputPower(), ecoflow.getOutputPower());
            ESP_LOGI("main", "Solar Input: %dW", ecoflow.getSolarInputPower());
            ESP_LOGI("main", "AC Output: %dW", ecoflow.getAcOutputPower());
            ESP_LOGI("main", "DC Output: %dW", ecoflow.getDcOutputPower());
            ESP_LOGI("main", "Cell Temp: %dC", ecoflow.getCellTemperature());
            ESP_LOGI("main", "State: AC=%d, DC=%d, USB=%d", ecoflow.isAcOn(), ecoflow.isDcOn(), ecoflow.isUsbOn());
            ESP_LOGI("main", "-------------------");
        }
    }

    // Test toggling routines
    if (millis() - last_dc_toggle > 20000) {
        last_dc_toggle = millis();
        if (ecoflow.isAuthenticated()) {
            dc_on = !dc_on;
            ESP_LOGI("main", "Toggling DC to %s", dc_on ? "ON" : "OFF");
            ecoflow.setDC(dc_on);
        }
    }

    if (millis() - last_usb_toggle > 25000) {
        last_usb_toggle = millis();
        if (ecoflow.isAuthenticated()) {
            usb_on = !usb_on;
            ESP_LOGI("main", "Toggling USB to %s", usb_on ? "ON" : "OFF");
            ecoflow.setUSB(usb_on);
        }
    }

    if (millis() - last_ac_toggle > 30000) {
        last_ac_toggle = millis();
        if (ecoflow.isAuthenticated()) {
            ac_on = !ac_on;
            ESP_LOGI("main", "Toggling AC to %s", ac_on ? "ON" : "OFF");
            ecoflow.setAC(ac_on);
        }
    }

    delay(100);
}
