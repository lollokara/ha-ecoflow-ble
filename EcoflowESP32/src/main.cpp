#include "EcoflowESP32.h"
#include "Credentials.h"

EcoflowESP32 ecoflow;

// Debug command handler for serial input
void handleSerialCommands() {
  if (!Serial.available()) {
    return;
  }
  
  char cmd = Serial.read();
  
  if (!ecoflow.isConnected()) {
    Serial.println(">>> Not connected. Cannot send commands.");
    // Flush any remaining serial data
    while (Serial.available()) {
      Serial.read();
      delay(1);
    }
    return;
  }
  
  switch (cmd) {
    case 'r':
    case 'R':
      Serial.println(">>> Manual command: Requesting data...");
      ecoflow.requestData();
      break;
      
    case 'a':
      Serial.println(">>> Manual command: Turning AC ON");
      ecoflow.setAC(true);
      delay(1000);
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
      Serial.println("║ r = Request device data ║");
      Serial.println("║ a/A = AC ON / AC OFF ║");
      Serial.println("║ u/U = USB ON / USB OFF ║");
      Serial.println("║ d/D = DC 12V ON / OFF ║");
      Serial.println("║ h/? = Show this help ║");
      Serial.println("╚════════════════════════════════════════╝\n");
      break;
      
    default:
      Serial.println(">>> Unknown command. Type 'h' for help.");
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);
  
  Serial.println("\n\n");
  Serial.println("╔════════════════════════════════════════╗");
  Serial.println("║ Ecoflow Delta 3 Remote Control Panel ║");
  Serial.println("║ ESP32 BLE Interface - With AUTH ║");
  Serial.println("╚════════════════════════════════════════╝\n");
  
  if (!ecoflow.begin()) {
    Serial.println("✗ FATAL: Failed to initialize BLE!");
    return;
  }
  
  // Set your credentials from Credentials.h
  ecoflow.setCredentials(ECOFLOW_USER_ID, ECOFLOW_DEVICE_SN);
  
  Serial.println("✓ BLE initialization complete");
  Serial.println("\nDebug Mode Instructions:");
  Serial.println("- Device will connect and authenticate");
  Serial.println("- Keep-alive every 3 seconds (with polling)");
  Serial.println("- Type 'h' for manual command options\n");
}

void loop() {
  static bool deviceFound = false;
  static bool connected = false;
  static unsigned long lastScan = 0;
  static unsigned long lastConnect = 0;
  static unsigned long lastStatus = 0;
  static uint8_t connectionFailures = 0;
  
  const uint8_t MAX_FAILURES_BEFORE_RESCAN = 3;
  const unsigned long RETRY_DELAY_MS = 2000;
  
  // Phase 1: Scan for device
  if (!deviceFound) {
    if (millis() - lastScan > 3000) {
      Serial.println(">>> Scanning for Ecoflow…");
      if (ecoflow.scan(5)) {
        Serial.println(">>> ✓ Device found! Proceeding to connect…");
        deviceFound = true;
        connectionFailures = 0;
        lastConnect = millis();
      }
      lastScan = millis();
    }
  }
  
  // Phase 2: Connect with backoff
  if (deviceFound && !connected) {
    if (millis() - lastConnect > RETRY_DELAY_MS) {
      Serial.println(">>> Attempting connection…");
      if (ecoflow.connectToServer()) {
        Serial.println("✓ Successfully connected!");
        connected = true;
        connectionFailures = 0;
        lastStatus = millis();
        Serial.println("\nReady for manual commands. Type 'h' for help.\n");
      } else {
        connectionFailures++;
        Serial.print(">>> Connection attempt ");
        Serial.print(connectionFailures);
        Serial.print("/");
        Serial.println(MAX_FAILURES_BEFORE_RESCAN);
        
        if (connectionFailures >= MAX_FAILURES_BEFORE_RESCAN) {
          Serial.println(">>> Max retries reached, rescanning…");
          deviceFound = false;
          connectionFailures = 0;
        }
      }
      lastConnect = millis();
    }
  }
  
  // Phase 3: Periodic status dump (if data available)
  if (connected) {
    if (millis() - lastStatus > 15000) {
      Serial.println("\n╔════════════════════════════════════════╗");
      Serial.println("║ Ecoflow Delta 3 Status (if available) ║");
      
      if (ecoflow.getBatteryLevel() > 0 || ecoflow.getInputPower() > 0) {
        Serial.print("║ Battery: ");
        Serial.print(ecoflow.getBatteryLevel());
        Serial.println("% ║");
        
        Serial.print("║ Input: ");
        Serial.print(ecoflow.getInputPower());
        Serial.println(" W ║");
        
        Serial.print("║ Output: ");
        Serial.print(ecoflow.getOutputPower());
        Serial.println(" W ║");
        
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
        Serial.println("║ (No data received yet - send 'r') ║");
      }
      
      Serial.println("╚════════════════════════════════════════╝\n");
      lastStatus = millis();
    }
  }
  
  // Detect unexpected disconnect
  if (connected && !ecoflow.isConnected()) {
    Serial.println(">>> Device disconnected unexpectedly!");
    Serial.println(">>> Attempting reconnection…");
    connected = false;
    deviceFound = false;
    connectionFailures = 0;
  }
  
  // Handle serial input for manual commands
  handleSerialCommands();
  
  delay(100);
}
