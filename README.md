# ⧉ ECOFLOW-ESP32 // CYBER_DECK

![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg?style=for-the-badge)
![Platform: ESP32](https://img.shields.io/badge/Platform-ESP32-blue.svg?style=for-the-badge&logo=espressif)
![Platform: STM32](https://img.shields.io/badge/Platform-STM32-orange.svg?style=for-the-badge&logo=stmicroelectronics)
![Status: ONLINE](https://img.shields.io/badge/Status-ONLINE-brightgreen.svg?style=for-the-badge&blink=true)

> **WARNING:** UNAUTHORIZED CLOUD DISCONNECTION IMMINENT.
> **TARGET:** LOCAL CONTROL ESTABLISHED.

**EcoflowESP32** is a futuristic, reverse-engineered C++ library for the ESP32 that enables **direct, offline control** of EcoFlow power stations via Bluetooth Low Energy (BLE). No clouds, no servers, just you and your hardware.

This project implements a complete **Cyber Deck** solution, combining an ESP32 Gateway, an STM32 Touch Display, and an RP2040 Fan Controller into a unified, air-gapped control terminal.

---

## ≡ SYSTEM CAPABILITIES

*   **[>> COMPLETE LOCAL CONTROL]**: Bypass the cloud. Toggle AC/DC/USB ports directly from your microcontroller.
*   **[>> REAL-TIME TELEMETRY]**: Monitor battery levels, input/output wattage, and temperatures with millisecond precision.
*   **[>> CRYPTOGRAPHIC SECURITY]**: Full implementation of the EcoFlow ECDH + AES-128 handshake.
*   **[>> MULTI-DEVICE SUPPORT]**: Seamlessly manage Delta 3, Wave 2, and Delta Pro 3 devices simultaneously.
*   **[>> MODULAR ARCHITECTURE]**: Distributed processing across ESP32 (Comms), STM32 (UI), and RP2040 (Thermal).

---

## ≡ SUPPORTED HARDWARE

| DEVICE | ID | STATUS | PROTOCOL |
| :--- | :--- | :--- | :--- |
| **EcoFlow Delta 3** | `D3` | **[FULL ACCESS]** | V3 (Protobuf) |
| **EcoFlow Wave 2** | `W2` | **[PARTIAL]** | V2 (Binary) |
| **EcoFlow Delta Pro 3** | `D3P` | **[BETA]** | V3 (Protobuf) |
| **Alternator Charger** | `AC` | **[BETA]** | V3 (Protobuf) |

---

## ≡ QUICK DEPLOYMENT

### 1. INITIALIZE PROTOCOL
Clone the repo into your PlatformIO `lib/` directory.

```bash
git clone https://github.com/lollokara/ha-ecoflow-ble.git lib/EcoflowESP32
```

### 2. CONFIGURE CREDENTIALS
Create `EcoflowESP32/src/Credentials.h`:
```cpp
#define ECOFLOW_USER_ID "USER_ID_FROM_APP"
#define ECOFLOW_DEVICE_SN "DEVICE_SERIAL_NUMBER"
#define ECOFLOW_KEYDATA "APP_KEY_HEX_STRING"
```

### 3. FLASH FIRMWARE
Deploy the `EcoflowESP32` project to your ESP32-S3 and `EcoflowSTM32F4` to your Discovery board.

---

## ≡ DOCUMENTATION MATRIX

Access the classified technical archives for deep implementation details.

| [SYSTEM ARCHITECTURE](docs/Home.md) | [PROTOCOL REFERENCE](docs/Protocol.md) |
| :---: | :---: |
| ![Architecture](https://img.shields.io/badge/VIEW-BLUEPRINT-blue?style=flat-square) | ![Protocol](https://img.shields.io/badge/VIEW-PACKETS-red?style=flat-square) |
| **Class Diagrams & Logic Flow** | **Byte-level V2/V3 Analysis** |

| [ESP32 GATEWAY](docs/Device_ESP32.md) | [STM32 INTERFACE](docs/Device_STM32.md) | [RP2040 THERMAL](docs/Device_RP2040.md) |
| :---: | :---: | :---: |
| ![ESP32](https://img.shields.io/badge/CORE-COMMS-blue?style=flat-square) | ![STM32](https://img.shields.io/badge/CORE-UI-orange?style=flat-square) | ![RP2040](https://img.shields.io/badge/CORE-FAN-green?style=flat-square) |
| **BLE & Bridge Logic** | **LVGL & Drivers** | **PWM & Sensors** |

---

## ≡ SYSTEM OVERVIEW

```mermaid
graph LR
    USER[User / Touch UI] -->|UART 115k| ESP{ESP32 CORE}
    ESP -->|BLE 4.2| DEV[EcoFlow Device]
    RP[RP2040 Fan Ctrl] -->|UART 115k| STM[STM32 Display]
    STM -->|UART 115k| ESP

    subgraph "EcoflowESP32 Library"
        ESP --> MGR[DeviceManager]
        MGR --> CRYPTO[Crypto Engine]
        MGR --> PROTO[Protocol V3]
    end

    DEV -->|Notifications| ESP
    ESP -->|Telemetry| STM
```

---

> *END OF LINE.*
