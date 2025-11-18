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
  // Data is updated via notifications, so we can just read the latest values here.
  Serial.print("Battery Level: ");
  Serial.print(ecoflow.getBatteryLevel());
  Serial.println("%");

  delay(5000);
}
