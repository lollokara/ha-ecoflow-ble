#include "EcoflowESP32.h"
#include "Credentials.h"

EcoflowESP32 ecoflow;

void handleSerialCommands() {
  if (!Serial.available()) return;

  char cmd = Serial.read();

  if (!ecoflow.isAuthenticated()) {
    Serial.println(">>> Not authenticated. Cannot send commands.");
    return;
  }

  switch (cmd) {
    case 'r':
      Serial.println(">>> Manual command: Requesting data...");
      ecoflow.requestData();
      break;
    case 'a':
      Serial.println(">>> Manual command: Turning AC ON");
      ecoflow.setAC(true);
      break;
    case 'A':
      Serial.println(">>> Manual command: Turning AC OFF");
      ecoflow.setAC(false);
      break;
    // ... (other commands)
    default:
      Serial.println(">>> Unknown command");
      break;
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  ecoflow.begin();
  ecoflow.setCredentials(ECOFLOW_USER_ID, ECOFLOW_DEVICE_SN);
}

void loop() {
    if (!ecoflow.isConnected() && !ecoflow.isConnecting()) {
        ecoflow.scan();
        ecoflow.connectToServer();
    }
  ecoflow.update();
  handleSerialCommands();
  delay(100);
}
