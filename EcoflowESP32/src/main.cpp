#include <Arduino.h>
#include "EcoflowESP32.h"
#include "Credentials.h"

EcoflowESP32 ecoflow;

// Replace with your device's BLE address
const std::string ble_address = "F8:3C:73:XX:XX:XX"; 

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        ; // wait for serial port to connect. Needed for native USB
    }
    Serial.println("Starting...");

    ecoflow.begin(ECOFLOW_USER_ID, ECOFLOW_DEVICE_SN, ble_address);
}

void loop() {
    ecoflow.update();
    delay(100);
}