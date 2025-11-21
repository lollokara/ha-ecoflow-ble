/**
 * @file Comprehensive.ino
 * @author Jules
 * @brief A comprehensive example demonstrating advanced features of the EcoflowESP32 library.
 *
 * This example covers:
 * - Managing connections to multiple devices (Delta 3 and Wave 2).
 * - Reading a wide range of data points from each device.
 * - Toggling the AC, DC, and USB outputs.
 * - Setting charging limits.
 *
 * Before running, make sure to:
 * 1. Fill in your credentials in `EcoflowESP32/src/Credentials.h`.
 * 2. Place this file in your Arduino sketch folder or open it with PlatformIO.
 */

#include <Arduino.h>
#include "DeviceManager.h"
#include "Credentials.h"

void printDeviceData(DeviceType type);

void setup() {
  Serial.begin(115200);
  Serial.println("Starting EcoflowESP32 Comprehensive example...");

  // Initialize the DeviceManager. It will load any saved devices from preferences.
  DeviceManager::getInstance().initialize();

  // You can initiate a scan for a specific device if it's not already saved.
  // The manager will automatically try to reconnect to saved devices.
  // Let's assume we want to find both a Delta 3 and a Wave 2.
  // The manager will handle scanning for both if they aren't connected.
  Serial.println("DeviceManager initialized. It will now automatically scan for and connect to saved devices.");
  Serial.println("If a device is not connecting, you can call scanAndConnect() to find it.");

  // Example: If you want to force a scan for a new Delta 3 device:
  // DeviceManager::getInstance().scanAndConnect(DeviceType::DELTA_3);
}

void loop() {
  // The DeviceManager's update loop handles all BLE communication,
  // connection state management, and automatic reconnections.
  DeviceManager::getInstance().update();

  // Print data for both devices every 5 seconds.
  static uint32_t last_print_time = 0;
  if (millis() - last_print_time > 5000) {
    last_print_time = millis();
    printDeviceData(DeviceType::DELTA_3);
    printDeviceData(DeviceType::WAVE_2);
  }

  // --- Example of Controlling a Device ---
  // This block will run once, 30 seconds after startup, to demonstrate control commands.
  static bool control_action_done = false;
  if (!control_action_done && millis() > 30000) {
    EcoflowESP32* delta3 = DeviceManager::getInstance().getDevice(DeviceType::DELTA_3);
    if (delta3 && delta3->isAuthenticated()) {
      Serial.println("\n--- Performing Control Actions on Delta 3 ---");

      Serial.println("Toggling AC port ON...");
      delta3->setAC(true);
      delay(5000); // Wait 5 seconds
      Serial.println("Toggling AC port OFF...");
      delta3->setAC(false);

      Serial.println("Setting AC Charge Limit to 500W...");
      delta3->setAcChargingLimit(500);
      delay(2000);

      Serial.println("Setting Battery SOC limits to 90% max, 10% min...");
      delta3->setBatterySOCLimits(90, 10);

      Serial.println("--- Control Actions Complete ---\n");
    }
    control_action_done = true; // Ensure this only runs once
  }
}

/**
 * @brief A helper function to print the status and data for a given device type.
 * @param type The DeviceType to print data for.
 */
void printDeviceData(DeviceType type) {
  DeviceSlot* slot = DeviceManager::getInstance().getSlot(type);
  EcoflowESP32* device = DeviceManager::getInstance().getDevice(type);

  if (!slot || !device) return;

  Serial.printf("\n--- Status for %s ---\n", slot->name.c_str());

  if (device->isAuthenticated()) {
    Serial.println("Status: Connected & Authenticated");
    Serial.printf("  Battery Level: %d %%\n", device->getBatteryLevel());
    Serial.printf("  Input Power: %d W\n", device->getInputPower());
    Serial.printf("  Output Power: %d W\n", device->getOutputPower());
    Serial.printf("  Solar Input: %d W\n", device->getSolarInputPower());
    Serial.printf("  AC On: %s\n", device->isAcOn() ? "Yes" : "No");
    Serial.printf("  DC On: %s\n", device->isDcOn() ? "Yes" : "No");
    Serial.printf("  USB On: %s\n", device->isUsbOn() ? "Yes" : "No");
    Serial.printf("  AC Charge Limit: %d W\n", device->getAcChgLimit());
    Serial.printf("  Max Charge SOC: %d %%\n", device->getMaxChgSoc());
    Serial.printf("  Min Discharge SOC: %d %%\n", device->getMinDsgSoc());
  } else if (device->isConnecting()) {
    Serial.println("Status: Connecting...");
  } else if (!slot->macAddress.empty()) {
    Serial.println("Status: Disconnected, will try to reconnect.");
  } else {
    Serial.println("Status: Not configured. Call scanAndConnect() to find this device.");
  }
}
