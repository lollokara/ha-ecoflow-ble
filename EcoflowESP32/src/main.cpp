#include <EcoflowESP32.h>
#include "EcoflowProtocol.h"

EcoflowESP32 ecoflow;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting EcoflowESP32 example...");
  ecoflow.begin();
}

void loop() {
    if (ecoflow.scan(10)) {
        if (ecoflow.connectToServer()) {
            Serial.println("Sending command to request data...");
            ecoflow.sendCommand(CMD_REQUEST_DATA, sizeof(CMD_REQUEST_DATA));
        }
    }
    Serial.println("Scan finished. Waiting 10 seconds to restart...");
    delay(10000);
}