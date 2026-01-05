# BrainTransplant: Inner Workings

## Overview

BrainTransplant is a standalone OTA (Over-The-Air) firmware update system designed for a dual-MCU architecture consisting of an ESP32 (Host) and an STM32F4 (Target). The system allows the ESP32 to update its own firmware as well as the firmware of the connected STM32F4 device via a UART connection.

## System Architecture

### 1. ESP32 (Host)
The ESP32 acts as the main controller and gateway. It hosts a Web Interface for user interaction and manages the update process.

*   **Web Interface:** A Cyberpunk-themed web dashboard served by the ESP32 allows users to upload `.bin` firmware files for both the ESP32 and the STM32.
*   **OTA Manager:** A dedicated C++ class (`OtaManager`) handles the state machine for updates.
*   **UART Bridge:** The ESP32 communicates with the STM32 over a high-speed UART connection (921600 baud).

### 2. STM32F4 (Target)
The STM32F4 runs a custom Bootloader and the Application firmware.

*   **Bootloader (Sector 0):** Resides at `0x08000000`. It initializes the hardware, checks for valid application code, and handles the firmware update protocol.
*   **Application (Sector 2):** Resides at `0x08008000`. This is the user's main program. The demo application provided prints "Hello World" to the debug console.

## Memory Map (STM32F469NI)

The STM32F469NI has 2MB of Flash memory, organized into two banks (Bank 1 and Bank 2). The system utilizes a Dual Bank update strategy for safety (though the simplified demo might target specific sectors).

| Address      | Sector | Usage              |
| :---         | :---   | :---               |
| `0x08000000` | 0      | **Bootloader**     |
| `0x08004000` | 1      | *Reserved/Config*  |
| `0x08008000` | 2+     | **Active App**     |
| `0x08100000` | 12+    | **Update Staging** |

*Note: The actual implementation leverages the STM32's hardware aliasing. The active bank is always aliased to 0x08000000. Updates are written to the inactive bank (e.g., 0x08100000), and then the `BFB2` (Boot Flash Bank 2) option bit is toggled to swap banks on the next reset.*

## OTA Protocol

The communication between ESP32 and STM32 uses a custom packet-based protocol.

### Packet Structure
`[START] [CMD] [LEN] [PAYLOAD] [CRC8]`

*   **START:** `0xAA`
*   **CMD:** Command ID
*   **LEN:** Payload Length
*   **PAYLOAD:** Data
*   **CRC8:** Checksum

### Update Sequence

1.  **Handshake:** ESP32 sends `CMD_OTA_START` (0xA0). STM32 Bootloader (or App) acknowledges and prepares for update. If the App is running, it sets a flag and reboots into the Bootloader.
2.  **Chunk Transfer:** Firmware is sent in 240-byte chunks using `CMD_OTA_CHUNK` (0xA1). Each chunk includes the offset and data.
3.  **Verification:** STM32 writes chunks to flash and verifies them.
4.  **Completion:** ESP32 sends `CMD_OTA_END` (0xA2) with the full CRC32 of the firmware.
5.  **Apply:** If CRC matches, STM32 swaps banks (or jumps to the new app) and resets.

## Safety Features

*   **Watchdog Timer:** The IWDG is active during the update to prevent hangs. The bootloader refreshes it periodically.
*   **CRC Verification:** Both individual chunks and the final full binary are checksummed to ensure integrity.
*   **Dual Bank / Rollback:** By writing to the inactive bank, the existing firmware remains safe until the verification passes and the bank swap is committed.
