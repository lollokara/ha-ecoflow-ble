# EcoFlow BLE Home Monitor ⚡🔋
### The Ultimate Local Control Dashboard

![Project Banner](https://img.shields.io/badge/Status-Active-brightgreen) ![License](https://img.shields.io/badge/License-MIT-blue) ![Author](https://img.shields.io/badge/Author-Lollokara-orange)

> "Control is not about power; it's about knowing where your power goes."

Welcome to the future of off-grid energy management. This project completely liberates your EcoFlow devices from the cloud, providing a **100% local**, privacy-focused, and ultra-responsive control center.

By reverse-engineering the proprietary BLE protocol, we've built a robust bridge between your Power Station and a custom-built STM32 Touchscreen Dashboard, bypassing the need for phone apps or internet servers.

---

## 🚀 Key Features

*   **Zero Cloud Dependency:** All data stays within your local hardware. No APIs, no servers, no lag.
*   **Universal Compatibility:** Supports **Delta 3**, **Wave 2**, **Delta Pro 3**, and **Alternator Charger**.
*   **Real-Time Telemetry:** View battery health, input/output wattage, and temperatures with millisecond precision.
*   **Active Control:** Toggle AC/DC inverters, adjust charge limits, and control Wave 2 modes directly from the screen.
*   **Advanced Thermal Management:** Integrated RP2040 Fan Controller for cabinet/enclosure cooling.
*   **Futuristic UI:** A slick, high-performance LVGL interface running on the STM32F469I-Discovery.

---

## 🛠️ The Hardware Trinity

This project uses a distributed multi-MCU architecture to ensure stability and performance:

1.  **The Gateway (ESP32-S3):** Handles the complex BLE encryption, authentication, and protocol decoding. It acts as the bridge to the EcoFlow device.
2.  **The Brain (STM32F469):** Runs the FreeRTOS kernel and LVGL graphics engine. It renders the UI and manages the system state.
3.  **The Coolant (RP2040):** A dedicated coprocessor monitoring rack temperatures and driving 4x PWM fans to keep your gear cool.

---

## 📚 Documentation

We believe code is only as good as its documentation. We have prepared a comprehensive Wiki detailing every byte of the protocol and every line of logic.

*   [**Home**](Wiki/Home.md) - Project Overview & Architecture
*   [**EcoFlow Protocol**](Wiki/EcoFlow-Protocol.md) - The reverse-engineered specification.
*   [**ESP32 Firmware**](Wiki/ESP32-Firmware.md) - BLE Authentication & Data Parsing.
*   [**STM32 Firmware**](Wiki/STM32-Firmware.md) - FreeRTOS Tasks & LVGL UI.
*   [**RP2040 Firmware**](Wiki/RP2040-Firmware.md) - Thermal Control Logic.

---

## 💿 Installation

### Prerequisites
*   [PlatformIO Core](https://docs.platformio.org/en/latest/core/index.html)
*   Visual Studio Code (recommended)

### Build Instructions

1.  **Clone the Repo:**
    ```bash
    git clone https://github.com/lollokara/ha-ecoflow-ble.git
    cd ha-ecoflow-ble
    ```

2.  **Compile & Upload (ESP32):**
    *   Create `EcoflowESP32/src/Credentials.h` with your device SN and keys.
    *   `pio run -d EcoflowESP32 -t upload`

3.  **Compile & Upload (STM32):**
    *   `pio run -d EcoflowSTM32F4 -t upload`

4.  **Compile & Upload (RP2040):**
    *   `pio run -d EcoflowRP2040 -t upload`

---

## 🛡️ License & Credits

**Author:** Lollokara

This project is open-source under the MIT License.
*Disclaimer: This project is not affiliated with EcoFlow. Use at your own risk.*
