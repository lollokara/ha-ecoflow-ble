#include "EcoflowESP32.h"

EcoflowESP32 ecoflow;

void setup() {
    Serial.begin(115200);
    delay(2000); // Extra time for serial to stabilize

    Serial.println("\n\n");
    Serial.println("╔════════════════════════════════════════╗");
    Serial.println("║ Ecoflow Delta 3 Remote Control Panel ║");
    Serial.println("║ ESP32 BLE Interface ║");
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
    static unsigned long lastConnectionAttempt = 0;
    static unsigned long lastStatusTime = 0;
    static uint8_t connectionFailureCount = 0;
    static const uint8_t MAX_FAILURES_BEFORE_RESCAN = 5;
    static const unsigned long RETRY_DELAY = 2000;  // 2 seconds between retries

    // Phase 1: Scan for device (only if not found or after many failures)
    if (!deviceFound) {
        if (millis() - lastScanTime > 3000) {  // Scan every 3 seconds if not found
            Serial.println("\n>>> Scanning for Ecoflow...");
            if (ecoflow.scan(5)) {
                Serial.println(">>> ✓ Device found! Proceeding to connect...");
                deviceFound = true;
                connectionFailureCount = 0;
                lastConnectionAttempt = millis();
            }
            lastScanTime = millis();
        }
    }

    // Phase 2: Connect to device (with backoff, not rescan)
    if (deviceFound && !connected) {
        // Only attempt connection after delay to avoid spam
        if (millis() - lastConnectionAttempt > RETRY_DELAY) {
            Serial.println(">>> Attempting connection…");
            if (ecoflow.connectToServer()) {
                Serial.println("✓ Successfully connected!");
                connected = true;
                connectionFailureCount = 0;
            } else {
                connectionFailureCount++;
                Serial.print(">>> Connection attempt ");
                Serial.print(connectionFailureCount);
                Serial.print("/");
                Serial.println(MAX_FAILURES_BEFORE_RESCAN);
                
                // After many failures, rescan
                if (connectionFailureCount >= MAX_FAILURES_BEFORE_RESCAN) {
                    Serial.println(">>> Max retries reached, rescanning...");
                    deviceFound = false;
                    connectionFailureCount = 0;
                }
            }
            lastConnectionAttempt = millis();
        }
    }

    // Phase 3: Display status if connected
    if (connected) {
        if (millis() - lastStatusTime > 15000) {  // Every 15 seconds
            Serial.println("\n╔════════════════════════════════════════╗");
            Serial.println("║ Ecoflow Delta 3 Status ║");
            Serial.print("║ Battery: ");
            Serial.print(ecoflow.getBatteryLevel());
            Serial.println("% ║");
            Serial.print("║ Input: ");
            Serial.print(ecoflow.getInputPower());
            Serial.println("W ║");
            Serial.print("║ Output: ");
            Serial.print(ecoflow.getOutputPower());
            Serial.println("W ║");
            Serial.print("║ AC: ");
            Serial.print(ecoflow.isAcOn() ? "ON" : "OFF");
            Serial.println(" ║");
            Serial.print("║ USB: ");
            Serial.print(ecoflow.isUsbOn() ? "ON" : "OFF");
            Serial.println(" ║");
            Serial.print("║ DC: ");
            Serial.print(ecoflow.isDcOn() ? "ON" : "OFF");
            Serial.println(" ║");
            Serial.println("╚════════════════════════════════════════╝");
            lastStatusTime = millis();
        }
    }
    
    // Handle unexpected disconnects
    if (connected && !ecoflow.isConnected()) {
        Serial.println(">>> Device disconnected unexpectedly!");
        connected = false;
        deviceFound = false;
    }
}
