/**
 * @file BareMinimum.ino
 * @author Jules
 * @brief A minimal example demonstrating how to connect to an EcoFlow device.
 *
 * This example shows the essential steps to initialize the DeviceManager,
 * scan for a specific EcoFlow device (in this case, a Delta 3), and print
 * its battery level to the Serial monitor once connected.
 *
 * Before running, make sure to:
 * 1. Fill in your credentials in `EcoflowESP32/src/Credentials.h`.
 * 2. Place this file in your Arduino sketch folder or open it with PlatformIO.
 */

#include <Arduino.h>
#include "DeviceManager.h"
#include "Credentials.h" // Make sure to fill this file with your data

void setup() {
  Serial.begin(115200);
  Serial.println("Starting EcoflowESP32 BareMinimum example...");

  // Initialize the DeviceManager. This will also handle BLE initialization.
  DeviceManager::getInstance().initialize();

  // Start scanning for a specific device type.
  // The manager will automatically try to connect if it finds a device
  // that matches the serial number prefix for that type.
  Serial.println("Scanning for Delta 3 device...");
  DeviceManager::getInstance().scanAndConnect(DeviceType::DELTA_3);
}

void loop() {
  // The DeviceManager handles all BLE updates, including reconnections.
  // We just need to call its update loop.
  DeviceManager::getInstance().update();

  // Get the device instance from the manager.
  EcoflowESP32* delta3 = DeviceManager::getInstance().getDevice(DeviceType::DELTA_3);

  // Check if the device is fully connected and authenticated.
  if (delta3 && delta3->isAuthenticated()) {
    // Once authenticated, you can get data from the device.
    Serial.print("Battery Level: ");
    Serial.print(delta3->getBatteryLevel());
    Serial.println("%");

    Serial.print("Input Power: ");
    Serial.print(delta3->getInputPower());
    Serial.println("W");

    Serial.print("Output Power: ");
    Serial.print(delta3->getOutputPower());
    Serial.println("W");
  } else {
    Serial.println("Device not authenticated, waiting...");
  }

  // Wait for 5 seconds before printing the next update.
  delay(5000);
}
