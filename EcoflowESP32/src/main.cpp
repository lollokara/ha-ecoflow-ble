#include <EcoflowESP32.h>

EcoflowESP32 ecoflow;

void setup() {
    Serial.begin(115200);
    Serial.println("Starting EcoflowESP32 example...");
    ecoflow.begin();
}

void loop() {
    Serial.println("Scanning for Ecoflow device...");
    if (ecoflow.scan(5)) {
        Serial.println("Device found, attempting to connect...");
        if (ecoflow.connectToServer()) {
            Serial.println("Connected to Ecoflow device!");

            // Example usage:
            // 1. Turn on AC output
            ecoflow.setAC(true);
            delay(2000);

            // 2. Request data and print status
            while (ecoflow.isConnected()) {
                ecoflow.requestData();
                delay(5000);
                Serial.printf("Battery: %d%%, Input: %dW, Output: %dW, AC: %s, DC: %s, USB: %s\n",
                              ecoflow.getBatteryLevel(),
                              ecoflow.getInputPower(),
                              ecoflow.getOutputPower(),
                              ecoflow.isAcOn() ? "On" : "Off",
                              ecoflow.isDcOn() ? "On" : "Off",
                              ecoflow.isUsbOn() ? "On" : "Off");
            }
        } else {
            Serial.println("Failed to connect to Ecoflow device.");
        }
    } else {
        Serial.println("No Ecoflow device found.");
    }

    Serial.println("Restarting scan in 10 seconds...");
    delay(10000);
}
