# BrainTransplant: Wiring Setup

This document details the wiring connections required between the ESP32 (Host) and the STM32F4 (Target) for the BrainTransplant system.

## Hardware Requirements

*   **ESP32 Development Board:** e.g., ESP32-S3 DevKitC-1.
*   **STM32F4 Board:** e.g., STM32F469I-DISCOVERY.
*   **Jumper Wires.**

## Pin Connections

### 1. Main UART (Data & OTA)
This is the primary high-speed link for data exchange and firmware updates.

| ESP32 Pin | STM32F4 Pin | Signal Name | Description          |
| :---      | :---        | :---        | :---                 |
| **GPIO 17** (TX) | **PG9** (RX)    | UART_TX -> UART_RX | ESP32 transmits to STM32 |
| **GPIO 18** (RX) | **PG14** (TX)   | UART_RX -> UART_TX | ESP32 receives from STM32 |
| **GND**       | **GND**         | Ground      | Common Ground reference |

*Note: STM32 Pins PG9/PG14 correspond to USART6 on the STM32F469I-DISCOVERY.*

### 2. Debug Console (STM32)
Optional but recommended for viewing the "Hello World" output and bootloader logs.

| STM32F4 Pin | USB-TTL Adapter | Description |
| :---        | :---            | :---        |
| **PB10** (TX)   | **RX**              | USART3 TX (Logs) |
| **PB11** (RX)   | **TX**              | USART3 RX        |
| **GND**         | **GND**             | Ground           |

### 3. Power
Ensure both boards are adequately powered.

*   **ESP32:** USB or 5V VIN.
*   **STM32F4:** USB or external 5V.
*   **Common Ground:** **Crucial!** Connect a ground wire between the ESP32 and STM32.

## Configuration Verification

### ESP32
Ensure `Serial1` is initialized on pins 17 (TX) and 18 (RX) in the firmware.

### STM32F4
*   **Bootloader:** Configures USART6 (PG14/PG9) for communication and USART3 (PB10/PB11) for debugging.
*   **Demo App:** Configures USART3 for the "Hello World" output.

## Troubleshooting

*   **No Communication:** Check if TX and RX are crossed (TX->RX, RX->TX).
*   **Garbage Data:** Verify the baud rate (Standard: 921600 for Main UART, 115200 for Debug).
*   **Unstable Update:** Shorten wires, ensure good ground connection, and check power supply stability.
