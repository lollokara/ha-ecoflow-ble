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
    static unsigned long last_battery_print_time = 0;
    unsigned long current_time = millis();

    ecoflow.update();

    if (ecoflow.isAuthenticated() && (current_time - last_battery_print_time > 5000)) {
        last_battery_print_time = current_time;
        int battery_level = ecoflow.getBatteryLevel();
        Serial.printf("Battery Level: %d%%\n", battery_level);
    }

    delay(100);
}
