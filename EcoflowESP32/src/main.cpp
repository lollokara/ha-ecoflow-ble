#include "EcoflowESP32.h"

EcoflowESP32 ecoflow;

// Debug command handler for serial input
void handleSerialCommands() {
    if (Serial.available()) {
        char cmd = Serial.read();
        
        if (ecoflow.isConnected()) {
            switch(cmd) {
                case 'r':
                case 'R':
                    Serial.println(">>> Manual command: Requesting data...");
                    ecoflow.requestData();
                    break;
                    
                case 'a':
                    Serial.println(">>> Manual command: Turning AC ON");
                    ecoflow.setAC(true);
                    delay(1000); // Wait for response
                    break;
                    
                case 'A':
                    Serial.println(">>> Manual command: Turning AC OFF");
                    ecoflow.setAC(false);
                    delay(1000);
                    break;
                    
                case 'u':
                    Serial.println(">>> Manual command: Turning USB ON");
                    ecoflow.setUSB(true);
                    delay(1000);
                    break;
                    
                case 'U':
                    Serial.println(">>> Manual command: Turning USB OFF");
                    ecoflow.setUSB(false);
                    delay(1000);
                    break;
                    
                case 'd':
                    Serial.println(">>> Manual command: Turning DC 12V ON");
                    ecoflow.setDC(true);
                    delay(1000);
                    break;
                    
                case 'D':
                    Serial.println(">>> Manual command: Turning DC 12V OFF");
                    ecoflow.setDC(false);
                    delay(1000);
                    break;
                    
                case 'h':
                case 'H':
                case '?':
                    Serial.println("\n╔════════════════════════════════════════╗");
                    Serial.println("║ Ecoflow Delta 3 - Debug Commands ║");
                    Serial.println("╠════════════════════════════════════════╣");
                    Serial.println("║ r    = Request device data ║");
                    Serial.println("║ a/A  = AC ON / AC OFF ║");
                    Serial.println("║ u/U  = USB ON / USB OFF ║");
                    Serial.println("║ d/D  = DC 12V ON / DC 12V OFF ║");
                    Serial.println("║ h    = Show this help ║");
                    Serial.println("╚════════════════════════════════════════╝\n");
                    break;
                    
                default:
                    Serial.println(">>> Unknown command. Type 'h' for help.");
                    break;
            }
        } else {
            Serial.println(">>> Not connected. Cannot send commands.");
        }
        
        // Flush any remaining serial data
        while (Serial.available()) {
            Serial.read();
            delay(1);
        }
    }
}

void setup() {
    Serial.begin(115200);
    delay(2000); // Extra time for serial to stabilize

    Serial.println("\n\n");
    Serial.println("╔════════════════════════════════════════╗");
    Serial.println("║ Ecoflow Delta 3 Remote Control Panel ║");
    Serial.println("║ ESP32 BLE Interface - DEBUG MODE ║");
    Serial.println("╚════════════════════════════════════════╝\n");

    if (!ecoflow.begin()) {
        Serial.println("✗ FATAL: Failed to initialize BLE!");
        return;
    }

    Serial.println("✓ BLE initialization complete");
    Serial.println("\nDebug Mode Instructions:");
    Serial.println("- Device will connect but will NOT automatically poll");
    Serial.println("- Type 'h' for manual command options");
    Serial.println("- Monitor connection stability (should NOT disconnect)");
    Serial.println("- If reason=531 appears, the Delta 3 is closing the link\n");
}

void loop() {
    static bool deviceFound = false;
    static bool connected = false;
    static unsigned long lastScanTime = 0;
    static unsigned long lastConnectionAttempt = 0;
    static unsigned long lastStatusTime = 0;
    static uint8_t connectionFailureCount = 0;
    static const uint8_t MAX_FAILURES_BEFORE_RESCAN = 3;
    static const unsigned long RETRY_DELAY = 2000; // 2 seconds between retries

    // Phase 1: Scan for device (only if not found or after many failures)
    if (!deviceFound) {
        if (millis() - lastScanTime > 3000) { // Scan every 3 seconds if not found
            Serial.println(">>> Scanning for Ecoflow...");
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
                lastStatusTime = millis();
                Serial.println("\nReady for manual commands. Type 'h' for help.\n");
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
        if (millis() - lastStatusTime > 15000) { // Every 15 seconds
            Serial.println("\n╔════════════════════════════════════════╗");
            Serial.println("║ Ecoflow Delta 3 Status (if available) ║");
            
            if (ecoflow.getBatteryLevel() > 0 || ecoflow.getInputPower() > 0) {
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
            } else {
                Serial.println("║ (No data received yet - send 'r' command) ║");
            }
            Serial.println("╚════════════════════════════════════════╝\n");
            lastStatusTime = millis();
        }

        // Handle unexpected disconnects
        if (connected && !ecoflow.isConnected()) {
            Serial.println(">>> Device disconnected unexpectedly!");
            Serial.println(">>> Attempting reconnection...");
            connected = false;
            deviceFound = false;
            connectionFailureCount = 0;
        }
    }

    // Handle serial input for manual commands
    handleSerialCommands();
    
    delay(100); // Small delay to prevent busy-waiting
}
