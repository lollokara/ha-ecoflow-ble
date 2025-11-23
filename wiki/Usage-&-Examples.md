# Usage & Examples

This page provides practical examples to help you get started with the EcoflowESP32 library in your own projects.

## Basic Setup (Single Device)

This is the simplest use case: connecting to a single EcoFlow device.

1.  **Create `Credentials.h`:** Before compiling, you must create a `Credentials.h` file in your `src/` directory. This file is excluded from version control and should contain your unique device information.

    ```cpp
    // src/Credentials.h
    #pragma once

    #define ECOFLOW_USER_ID "YOUR_ECOFLOW_USER_ID"
    #define ECOFLOW_DEVICE_SN "YOUR_DEVICE_SERIAL_NUMBER"
    #define ECOFLOW_KEYDATA "YOUR_DEVICE_KEY_DATA"
    ```

2.  **Main Sketch:** Your main `.cpp` file should initialize and update the `DeviceManager`.

    ```cpp
    #include <Arduino.h>
    #include "DeviceManager.h"
    #include "types.h" // Required for DeviceType enum

    void setup() {
      Serial.begin(115200);
      DeviceManager::getInstance().initialize();

      // Optional: Start a scan immediately if no device is saved
      if (DeviceManager::getInstance().getSlot(DeviceType::DELTA3) == nullptr) {
          DeviceManager::getInstance().scanAndConnect(DeviceType::DELTA3);
      }
    }

    void loop() {
      DeviceManager::getInstance().update();
      delay(10); // Small delay to prevent busy-waiting
    }
    ```

## Accessing Device Data

Once a device is connected and authenticated, you can easily access its data.

```cpp
#include "DeviceManager.h"
#include "types.h"

// ... in your loop() or another function ...

EcoflowESP32* delta3 = DeviceManager::getInstance().getDevice(DeviceType::DELTA3);

if (delta3 && delta3->isAuthenticated()) {
    int battery = delta3->getBatteryLevel();
    int inputPower = delta3->getInputPower();
    int outputPower = delta3->getOutputPower();

    Serial.printf("Battery: %d%%, Input: %d W, Output: %d W\n", battery, inputPower, outputPower);
}
```

## Controlling the Device

Sending commands is just as simple. The library handles the necessary packet creation and encryption.

```cpp
#include "DeviceManager.h"
#include "types.h"

// ... in a function, perhaps triggered by a button press ...

void toggleAcPower() {
    EcoflowESP32* delta3 = DeviceManager::getInstance().getDevice(DeviceType::DELTA3);

    if (delta3 && delta3->isAuthenticated()) {
        bool currentAcState = delta3->isAcOn();
        Serial.printf("AC is currently %s. Turning it %s...\n",
            currentAcState ? "ON" : "OFF",
            !currentAcState ? "ON" : "OFF");

        bool success = delta3->setAC(!currentAcState);

        if (success) {
            Serial.println("Command sent successfully!");
        } else {
            Serial.println("Failed to send command.");
        }
    }
}
```
