#include <EcoflowESP32.h>

EcoflowESP32 ecoflow;

void setup() {
  Serial.begin(115200);
  Serial.println("Starting EcoflowESP32 BareMinimum example...");

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
  // put your main code here, to run repeatedly:
  delay(1000);
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
