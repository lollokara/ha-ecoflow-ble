# ESP32 Firmware Documentation

**Author:** Lollokara

## Overview

The **EcoflowESP32** firmware transforms a standard ESP32-S3 into a specialized "BLE Gateway". Its primary purpose is to abstract away the complexity of the EcoFlow BLE protocol, presenting a simple, clean UART interface to the main display controller (STM32).

## Key Components

### 1. `EcoflowESP32` Class
This is the core engine of the firmware.
*   **File:** `src/EcoflowESP32.cpp`
*   **Role:** Manages the entire lifecycle of a BLE connection.
*   **Features:**
    *   **Scanning:** Filters BLE advertisements for EcoFlow Service UUIDs.
    *   **State Machine:** Handles the transition from `SCANNING` -> `CONNECTING` -> `AUTHENTICATING` -> `CONNECTED`.
    *   **Cryptography:** Uses `mbedtls` to handle ECDH key exchange and AES-128 decryption.
    *   **Proto Decoding:** Deserializes incoming `pd335_sys` protobuf messages into `EcoflowData` structs.

### 2. `DeviceManager` Class
*   **File:** `src/DeviceManager.cpp`
*   **Role:** Maintains a registry of known devices.
*   **Logic:** It allows the system to remember paired devices and automatically reconnect to them when they come within range. It supports up to 4 concurrent device slots.

### 3. `Stm32Serial` Class
*   **File:** `src/Stm32Serial.cpp`
*   **Role:** Handles the physical communication with the STM32.
*   **Logic:**
    *   Listens on `Serial1` (Pins 17 TX, 18 RX).
    *   Implements the Packet Parser (Start Byte `0xAA` check, CRC validation).
    *   Dispatches valid commands (`CMD_SET_AC`, etc.) to the `EcoflowESP32` instance.

## Authentication Process (The "Handshake")

The most critical part of the firmware is the `_handleAuthPacket` logic.

1.  **Public Key Exchange:** When the device sends its public key (header `0x01` inside payload), the ESP32 extracts the raw 40-byte key (X, Y coordinates).
2.  **Shared Secret Derivation:** The ESP32 uses its own private key to compute the ECDH Shared Secret.
3.  **AES Key Gen:** The first 16 bytes of the Shared Secret become the AES Key. The MD5 hash of the Shared Secret becomes the IV.
4.  **Verification:** The ESP32 sends a packet encrypted with this new key. If the device responds with valid data, the session is authenticated.

## Setup & Configuration

### Credentials.h
You must create a `src/Credentials.h` file (not included in git) to define your identity:

```cpp
#define ECOFLOW_USER_ID "1234567890123456" // From EcoFlow App Request
#define ECOFLOW_DEVICE_SN "R331Zxxxxxxxxx" // Target Device Serial
#define ECOFLOW_KEYDATA {0x01, 0x02...}    // Your Private Key (32 bytes)
```

*Note: Generating these keys requires capturing the initial pairing request from the official EcoFlow app, which is outside the scope of this firmware.*
