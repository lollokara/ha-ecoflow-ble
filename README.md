# EcoflowESP32: Local BLE Control for EcoFlow Power Stations

![Project Status: Somewhat Stable](https://img.shields.io/badge/status-somewhat%20stable-orange.svg)

Unlock your EcoFlow power station from the cloud! This library provides a powerful C++ interface for the ESP32, allowing you to monitor and control your EcoFlow devices directly over Bluetooth LE. No internet connection, no servers, just pure local control.

This project is a C++ port of the excellent Python reverse-engineering work done by the community, designed to be a robust and reliable solution for custom integrations with platforms like Home Assistant or your own standalone projects.

## Features

- **Direct BLE Communication:** No reliance on the EcoFlow cloud or MQTT.
- **Real-time Monitoring:** Get instant access to battery level, input/output power, temperature, and more.
- **Full Control:** Toggle AC, DC, and USB ports on and off.
- **Device Management:** Handles multiple devices, automatic reconnection, and state management.
- **Extensible:** Built with a clear separation between the protocol layer and device-specific logic.

## Getting Started

Integrating this library into your PlatformIO project is straightforward.

1.  **Add as a Library:** Add this repository as a library dependency in your `platformio.ini`.
2.  **Include Credentials:** Create a `Credentials.h` file in your `src` directory with your device's unique information.
3.  **Instantiate and Run:** Use the `DeviceManager` to initialize and connect to your devices.

```cpp
#include <Arduino.h>
#include "DeviceManager.h"

void setup() {
  Serial.begin(115200);
  DeviceManager::getInstance().initialize();
}

void loop() {
  DeviceManager::getInstance().update();
  delay(10);
}
```

## 🚀 Dive Deeper into the Wiki!

This README is just the beginning. For the full story, including in-depth protocol details, architectural diagrams, and advanced usage examples, head over to our GitHub Wiki!

[**Explore the Full Documentation on the Wiki &raquo;**](https://github.com/lollokara/ha-ecoflow-ble/wiki)
