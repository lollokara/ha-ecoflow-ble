# ESP32-S3 Firmware

The ESP32-S3 serves as the **Communications Gateway**. Its primary responsibility is to bridge the proprietary Bluetooth Low Energy (BLE) protocol of the EcoFlow devices to a standard serial interface for the display.

## Key Features

*   **BLE Client:** Scans for, connects to, and authenticates with EcoFlow devices.
*   **Protocol Decoding:** Handles the complex Protobuf deserialization and AES decryption.
*   **Device Management:** Maintains a list of known devices and their connection states.
*   **Secure Storage:** Stores authentication keys (ECDH Public/Private keys) in NVS.

## Code Structure

*   `src/main.cpp`: Entry point. Setup and Main Loop.
*   `src/DeviceManager.cpp`: Singleton class that orchestrates scanning and connection logic.
*   `src/Stm32Serial.cpp`: Handles the UART communication with the display.
*   `lib/EcoFlowComm/`: Shared library containing protocol definitions and serialization logic.

## Logic Flow

1.  **Boot:** Initialize NVS, Serial, and BLE.
2.  **Scan:** The `DeviceManager` continuously scans for BLE advertisements matching the EcoFlow Service UUID.
3.  **Connect:** When a known or new device is found, it initiates a connection.
4.  **Handshake:**
    *   Generates ephemeral keys.
    *   Exchanges keys with the device.
    *   Derives the Session Key.
    *   Authenticates.
5.  **Loop:**
    *   **BLE Task:** Receives notifications, decrypts them, and updates the internal `DeviceStatus` struct.
    *   **Serial Task:** Listens for commands from the STM32 (e.g., `CMD_GET_DEVICE_STATUS`).
    *   **Response:** When polled, serializes the `DeviceStatus` struct and sends it over UART.

## Libraries Used

*   **NimBLE-Arduino:** For robust and memory-efficient BLE operations.
*   **mbedtls:** For ECDH key exchange and AES-128-CBC encryption.
*   **Nanopb:** For decoding Protocol Buffers.

## Configuration

*   **Board:** `esp32-s3-devkitc-1`
*   **Framework:** Arduino
*   **Platform:** `espressif32 @ 6.6.0`

## Author
*   **Lollokara**
