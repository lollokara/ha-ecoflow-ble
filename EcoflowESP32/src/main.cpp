#include "EcoflowESP32.h"

EcoflowESP32 ecoflow;

void setup() {
    Serial.begin(115200);
    delay(2000);  // Extra time for serial to stabilize
    
    Serial.println("\n\n");
    Serial.println("╔════════════════════════════════════════╗");
    Serial.println("║  Ecoflow Delta 3 Remote Control Panel  ║");
    Serial.println("║         ESP32 BLE Interface            ║");
    Serial.println("╚════════════════════════════════════════╝\n");
    
    if (!ecoflow.begin()) {
        Serial.println("✗ FATAL: Failed to initialize BLE!");
        return;
    }
    
    Serial.println("✓ BLE initialization complete\n");
}

void loop() {
    static bool deviceFound = false;
    static bool connected = false;
    static unsigned long lastScanTime = 0;
    static unsigned long lastStatusTime = 0;
    
    // Phase 1: Scan for device (only if no device found yet)
    if (!deviceFound) {
        if (millis() - lastScanTime > 3000) {  // Increased to 3 seconds
            Serial.println("\n>>> Scanning for Ecoflow...");
            if (ecoflow.scan(5)) {
                Serial.println(">>> ✓ Device found! Proceeding to connect...");
                deviceFound = true;
                lastScanTime = millis();
                delay(500);  // Give time for everything to settle
                return;
            } else {
                Serial.println(">>> ✗ No device found, will retry...");
                lastScanTime = millis();
            }
        }
    }
    // Phase 2: Device found, now try to connect
    else if (deviceFound && !connected) {
        Serial.println("\n>>> Attempting connection...");
        if (ecoflow.connectToServer()) {
            Serial.println(">>> ✓ Successfully connected!\n");
            connected = true;
            lastScanTime = millis();
            lastStatusTime = millis();
            delay(1000);  // Let connection stabilize
        } else {
            Serial.println(">>> ✗ Connection failed, will retry...");
            deviceFound = false;  // Reset and rescan
            lastScanTime = millis();
            delay(2000);
        }
    }
    // Phase 3: Connected - display status
    else if (connected) {
        if (ecoflow.isConnected()) {
            // Print status every 15 seconds
            if (millis() - lastStatusTime > 15000) {
                Serial.println("\n╔════════════════════════════════════════╗");
                Serial.println("║        Ecoflow Delta 3 Status          ║");
                Serial.print("║ Battery:      ");
                Serial.print(ecoflow.getBatteryLevel());
                Serial.println("%                              ║");
                Serial.print("║ Input Power:  ");
                Serial.print(ecoflow.getInputPower());
                Serial.println("W                            ║");
                Serial.print("║ Output Power: ");
                Serial.print(ecoflow.getOutputPower());
                Serial.println("W                            ║");
                Serial.print("║ AC Output:    ");
                Serial.print(ecoflow.isAcOn() ? "✓ ON " : "✗ OFF");
                Serial.println("                            ║");
                Serial.print("║ USB Output:   ");
                Serial.print(ecoflow.isUsbOn() ? "✓ ON " : "✗ OFF");
                Serial.println("                            ║");
                Serial.print("║ DC Output:    ");
                Serial.print(ecoflow.isDcOn() ? "✓ ON " : "✗ OFF");
                Serial.println("                            ║");
                Serial.println("╚════════════════════════════════════════╝\n");
                lastStatusTime = millis();
            }
            delay(500);  // Prevent loop from running too fast
        } else {
            Serial.println("\n>>> Connection lost, will attempt reconnection...");
            connected = false;
            deviceFound = false;
            lastScanTime = millis();
            delay(2000);
        }
    }
}

/*
 * COMMAND EXAMPLES:
 * 
 * Once connected and status is displaying, uncomment to test:
 * 
 * In Phase 3 section, add this to toggle AC every 30 seconds:
 * 
 * static unsigned long lastToggle = 0;
 * if (millis() - lastToggle > 30000) {
 *     static bool acState = false;
 *     Serial.println(acState ? ">>> Turning AC OFF" : ">>> Turning AC ON");
 *     ecoflow.setAC(!acState);
 *     acState = !acState;
 *     lastToggle = millis();
 * }
 */
