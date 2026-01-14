#include <Arduino.h>
#include "DeviceManager.h"
#include "Credentials.h" // Make sure to fill this file with your data

void setup() {
  Serial.begin(115200);
  Serial.println("Starting EcoflowESP32 example...");

  // Initialize the DeviceManager.
  DeviceManager::getInstance().initialize();

  // Start scanning for a Delta 3 device.
  // After this the device will be paired
  DeviceManager::getInstance().scanAndConnect(DeviceType::BLADE);
}

void loop() {
  // The DeviceManager handles all BLE updates.
  DeviceManager::getInstance().update();

  // Get the device instance from the manager.
  EcoflowESP32* blade = DeviceManager::getInstance().getDevice(DeviceType::BLADE);

  // Check if the device is fully connected and authenticated.
  if (blade && blade->isAuthenticated()) {
    Serial.print("Battery Level: ");
    Serial.print(blade->getBatteryLevel());
    Serial.println("%");
  } else {
    Serial.println("Device not authenticated, waiting...");
  }

  delay(5000);
}