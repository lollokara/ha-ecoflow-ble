#include <EcoflowESP32.h>

EcoflowESP32 ecoflow;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting EcoflowESP32 Comprehensive example...");

  ecoflow.begin();

  Serial.println("Scanning for Ecoflow device for 5 seconds...");
  NimBLEAdvertisedDevice* pDevice = ecoflow.scan(5);

  if (pDevice != nullptr) {
    Serial.print("Found device: ");
    Serial.println(pDevice->getAddress().toString().c_str());
    if (ecoflow.connectToDevice(pDevice)) {
      Serial.println("Connected to device. Waiting for data...");
    } else {
      Serial.println("Failed to connect to device.");
    }
  } else {
    Serial.println("No Ecoflow device found.");
  }
}

void loop() {
  Serial.println("--- Reading Data ---");
  Serial.print("Battery Level: ");
  Serial.print(ecoflow.getBatteryLevel());
  Serial.println("%");
  Serial.print("Input Power: ");
  Serial.print(ecoflow.getInputPower());
  Serial.println("W");
  Serial.print("Output Power: ");
  Serial.print(ecoflow.getOutputPower());
  Serial.println("W");
  Serial.print("AC On: ");
  Serial.println(ecoflow.isAcOn());
  Serial.print("DC On: ");
  Serial.println(ecoflow.isDcOn());
  Serial.print("USB On: ");
  Serial.println(ecoflow.isUsbOn());

  Serial.println("\n--- Controlling Outputs ---");
  Serial.println("Turning AC on for 5 seconds...");
  ecoflow.setAC(true);
  delay(5000);
  Serial.println("Turning AC off.");
  ecoflow.setAC(false);

  Serial.println("Turning DC on for 5 seconds...");
  ecoflow.setDC(true);
  delay(5000);
  Serial.println("Turning DC off.");
  ecoflow.setDC(false);

  Serial.println("Turning USB on for 5 seconds...");
  ecoflow.setUSB(true);
  delay(5000);
  Serial.println("Turning USB off.");
  ecoflow.setUSB(false);

  delay(10000);
}
