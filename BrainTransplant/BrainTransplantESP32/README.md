# BrainTransplant ESP32 Updater

This is the ESP32 firmware for the BrainTransplant system. It acts as a controller and update bridge for the STM32F4.

## Features
- **Web Interface:** Access via browser to upload firmware.
- **OTA Updates:**
  - Updates itself (ESP32) via Web Upload.
  - Updates the attached STM32F4 via UART protocol.
- **Minimal:** stripped down to just the updater functionality.

## Usage
1. Compile and Flash to ESP32-S3.
2. Connect to WiFi (Credentials in `src/Credentials.h`).
3. Navigate to IP address.
4. Upload `firmware.bin` for ESP32 or `factory_firmware.bin` (or app binary) for STM32.
