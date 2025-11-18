#include <EcoflowESP32.h>

EcoflowESP32 ecoflow;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting EcoflowESP32 example...");
  ecoflow.begin();
}

void loop() {
  if (ecoflow.scan(10)) {
    ecoflow.connectToServer();
  }
  Serial.println("Scan finished. Waiting 10 seconds to restart...");
  delay(10000);
}