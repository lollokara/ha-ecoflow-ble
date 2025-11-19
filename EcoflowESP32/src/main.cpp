#include <Arduino.h>
#include "EcoflowESP32.h"
#include "credentials.h"

EcoflowESP32 ecoflow;

void printData() {
    Serial.println("--- Device Status ---");
    Serial.printf("Battery: %d%%\n", ecoflow.getBatteryLevel());
    Serial.printf("Input Power: %d W\n", ecoflow.getInputPower());
    Serial.printf("Output Power: %d W\n", ecoflow.getOutputPower());
    Serial.printf("AC: %s\n", ecoflow.isAcOn() ? "ON" : "OFF");
    Serial.printf("DC: %s\n", ecoflow.isDcOn() ? "ON" : "OFF");
    Serial.printf("USB: %s\n", ecoflow.isUsbOn() ? "ON" : "OFF");
    Serial.println("---------------------");
}

void handleSerialCommand(String cmd) {
    cmd.trim();
    if (cmd == "h") {
        Serial.println("--- Help ---");
        Serial.println("h - this help");
        Serial.println("s - status");
        Serial.println("ac on/off");
        Serial.println("dc on/off");
        Serial.println("usb on/off");
        Serial.println("------------");
    } else if (cmd == "s") {
        printData();
    } else if (cmd.startsWith("ac")) {
        bool on = (cmd.indexOf("on") > -1);
        ecoflow.setAC(on);
    } else if (cmd.startsWith("dc")) {
        bool on = (cmd.indexOf("on") > -1);
        ecoflow.setDC(on);
    } else if (cmd.startsWith("usb")) {
        bool on = (cmd.indexOf("on") > -1);
        ecoflow.setUSB(on);
    }
}


void setup() {
    Serial.begin(115200);
    Serial.println("Starting Ecoflow ESP32 Client");

    ecoflow.setCredentials(ECOFLOW_USER_ID, ECOFLOW_DEVICE_SN);
    ecoflow.begin();
}

void loop() {
    if (!ecoflow.isConnected()) {
        Serial.println("Scanning for Ecoflow device...");
        if (ecoflow.scan(5)) {
            Serial.println("Device found, connecting...");
            if (ecoflow.connectToServer()) {
                Serial.println("Connection successful!");
            } else {
                Serial.println("Connection failed. Retrying in 10s...");
                delay(10000);
            }
        } else {
            Serial.println("No device found. Retrying in 10s...");
            delay(10000);
        }
    } else {
        if (Serial.available()) {
            String cmd = Serial.readString();
            handleSerialCommand(cmd);
        }
    }

    ecoflow.update();
    delay(100);
}