#include <EcoflowESP32.h>

EcoflowESP32 ecoflow;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting EcoflowESP32 example...");
  ecoflow.begin();
}

void loop() {
  Serial.println("Scanning...");
  if (ecoflow.scan(5)) {
    Serial.println("Device found, attempting to connect...");
    if (ecoflow.connectToServer()) {
      Serial.println("Connection process started...");
      // Keep the connection alive and let the notification callback handle the data
      while(true) {
        delay(1000);
      }
    } else {
        Serial.println("connectToServer() returned false");
    }
  } else {
    Serial.println("No device found in scan.");
  }
  Serial.println("Scan finished. Restarting in 1 second...");
  delay(1000);
}
