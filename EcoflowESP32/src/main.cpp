#include <Arduino.h>
#include "EcoflowESP32.h"
#include "Credentials.h"

EcoflowESP32 ecoflow;

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        ; // wait for serial port to connect. Needed for native USB
    }
    Serial.println("Starting...");

    ecoflow.begin();
    ecoflow.setCredentials(ECOFLOW_USER_ID, ECOFLOW_DEVICE_SN);
}

void loop() {
    if (!ecoflow.isConnected()) {
        Serial.println("Scanning for Ecoflow device...");
        if (ecoflow.scan(5)) {
            Serial.println("Device found! Connecting...");
            ecoflow.connectToServer();
        }
    }
    delay(1000);
}
