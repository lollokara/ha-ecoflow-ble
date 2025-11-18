#include <EcoflowESP32.h>

EcoflowESP32 ecoflow;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting EcoflowESP32 example...");
  ecoflow.begin();
}

void loop() {
  if (ecoflow.scan(5)) {
    if (ecoflow.connectToServer()) {
      Serial.println("Connected! Now reading data for 10 seconds...");
      for (int i = 0; i < 5; i++) {
        delay(2000);
        Serial.print("Battery Level: ");
        Serial.print(ecoflow.getBatteryLevel());
        Serial.println("%");

        Serial.print("Input Power: ");
        Serial.print(ecoflow.getInputPower());
        Serial.println("W");

        Serial.print("Output Power: ");
        Serial.print(ecoflow.getOutputPower());
        Serial.println("W");

        Serial.print("AC Status: ");
        Serial.println(ecoflow.isAcOn() ? "On" : "Off");

        Serial.print("DC Status: ");
        Serial.println(ecoflow.isDcOn() ? "On" : "Off");

        Serial.print("USB Status: ");
        Serial.println(ecoflow.isUsbOn() ? "On" : "Off");

        Serial.println("--------------------");
      }
    }
  }

  delay(1000);
}
