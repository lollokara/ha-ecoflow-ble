# ESP32 - STM32F4 Communication Protocol

This document details the custom serial protocol used for communication between the ESP32-S3 (BLE Gateway) and the STM32F469 (UI Display) in the EcoFlow Monitor project.

## Physical Layer

-   **Interface:** UART (Universal Asynchronous Receiver-Transmitter)
-   **Baud Rate:** 115200 bps
-   **Data Bits:** 8
-   **Parity:** None
-   **Stop Bits:** 1
-   **Flow Control:** None

### Pinout

| Signal | ESP32 Pin | STM32F4 Pin | Description |
| :--- | :--- | :--- | :--- |
| **TX** | GPIO 17 | PG9 (RX) | Data from ESP32 to STM32 |
| **RX** | GPIO 18 | PG14 (TX) | Data from STM32 to ESP32 |
| **GND** | GND | GND | Common Ground |

## Packet Structure

All data is exchanged in valid packets with the following format:

`[START] [CMD] [LEN] [PAYLOAD] [CRC8]`

| Byte Index | Field | Length | Description |
| :--- | :--- | :--- | :--- |
| 0 | **START** | 1 | Start Byte (Fixed: `0xAA`) |
| 1 | **CMD** | 1 | Command ID (See below) |
| 2 | **LEN** | 1 | Length of the Payload (0 - 255) |
| 3...N | **PAYLOAD** | `LEN` | Data bytes (optional) |
| N+1 | **CRC8** | 1 | Checksum of CMD, LEN, and PAYLOAD |

### Checksum Calculation

The 8-bit CRC is calculated using the Maxim One-Wire polynomial (`0x31` or `X^8 + X^5 + X^4 + 1`) over the Command, Length, and Payload bytes.

## Communication Flow

### 1. Handshake (Startup)

When the STM32F4 boots, it initiates a handshake to establish a connection with the ESP32.

1.  **STM32F4** sends `CMD_HANDSHAKE` (`0x20`).
2.  **ESP32** responds with `CMD_HANDSHAKE_ACK` (`0x21`).
3.  **ESP32** immediately follows up by sending the full device list via `CMD_DEVICE_LIST` (`0x22`).

### 2. Device List Management

The ESP32 maintains a list of known and connected EcoFlow devices.

-   **Get List:** The STM32F4 can request the list at any time using `CMD_REQUEST_STATUS_UPDATE` (Not currently implemented for this purpose, usually relies on push). *Correction: The protocol defines `CMD_DEVICE_LIST` push from ESP32.*
-   **Update:** When a device connects or disconnects via BLE, the ESP32 pushes a `CMD_DEVICE_LIST` (`0x22`) packet to the STM32F4.
-   **Acknowledgment:** The STM32F4 acknowledges the list with `CMD_DEVICE_LIST_ACK` (`0x23`).

### 3. Device Status Polling

To keep the UI updated, the STM32F4 polls the ESP32 for the status of specific devices.

1.  **STM32F4** sends `CMD_GET_DEVICE_STATUS` (`0x25`) with the `DeviceID` in the payload.
2.  **ESP32** responds with `CMD_DEVICE_STATUS` (`0x24`) containing the full telemetry data (battery, power, etc.) for that device.

### 4. Control Commands

When the user interacts with the UI (e.g., toggles a switch), the STM32F4 sends a control command to the ESP32.

-   **Set Wave 2:** `CMD_SET_WAVE2` (`0x30`) - Payload: `[Type, Value]`
-   **Set AC:** `CMD_SET_AC` (`0x31`) - Payload: `[Enable (0/1)]`
-   **Set DC:** `CMD_SET_DC` (`0x32`) - Payload: `[Enable (0/1)]`
-   **Set Value:** `CMD_SET_VALUE` (`0x40`) - Payload: `[Type, Value (4 bytes)]` (Used for limits like AC charge speed).

## Command IDs

### ESP32 -> STM32F4 (Server to Client)

| ID | Name | Description |
| :--- | :--- | :--- |
| `0x21` | `CMD_HANDSHAKE_ACK` | Response to handshake request. |
| `0x22` | `CMD_DEVICE_LIST` | Pushes the list of known devices. |
| `0x24` | `CMD_DEVICE_STATUS` | Contains telemetry data for a device. |
| `0x61` | `CMD_DEBUG_INFO` | Response with system debug info (IP, uptime). |

### STM32F4 -> ESP32 (Client to Server)

| ID | Name | Description |
| :--- | :--- | :--- |
| `0x20` | `CMD_HANDSHAKE` | Initiates connection. |
| `0x23` | `CMD_DEVICE_LIST_ACK` | Acknowledges receipt of device list. |
| `0x25` | `CMD_GET_DEVICE_STATUS` | Requests telemetry for a specific device. |
| `0x30` | `CMD_SET_WAVE2` | Control command for Wave 2. |
| `0x31` | `CMD_SET_AC` | Toggle AC ports. |
| `0x32` | `CMD_SET_DC` | Toggle DC ports. |
| `0x40` | `CMD_SET_VALUE` | Set a numeric value (charging limit, etc). |
| `0x50` | `CMD_POWER_OFF` | Triggers ESP32 shutdown/reboot. |
| `0x60` | `CMD_GET_DEBUG_INFO` | Requests system debug info. |
| `0x62` | `CMD_CONNECT_DEVICE` | Request to connect to a specific device type. |
| `0x63` | `CMD_FORGET_DEVICE` | Request to forget a device. |

## Data Structures

### DeviceStatus Payload

The `CMD_DEVICE_STATUS` payload is a packed struct:

```c
typedef struct {
    uint8_t id;          // DeviceType (1=D3, 2=DP3, 3=W2, 4=Alt)
    uint8_t connected;   // 1=Connected, 0=Disconnected
    char name[16];       // Device Name
    uint8_t brightness;  // Ambient Light Level (10-100%)
    DeviceSpecificData data; // Union of device-specific structs
} DeviceStatus;
```

### DeviceSpecificData Union

This union holds the telemetry data corresponding to the `id`.

-   **Delta 3 (`d3`):** Battery %, Input/Output Power, Port States, Charging Limits.
-   **Wave 2 (`w2`):** Mode, SubMode, Fan Speed, Temperatures, Battery Status.
-   **Delta Pro 3 (`d3p`):** Similar to Delta 3 but with High/Low Voltage AC/DC separation.
-   **Alternator Charger (`ac`):** Battery Voltage, Alternator Input, Charger Mode.

*Note: All floating-point values are transmitted as 4-byte `float`. Integer values are standard `int32_t` or `uint32_t`.*

## Error Handling

-   **CRC Failure:** If the calculated CRC does not match the received CRC, the packet is silently discarded.
-   **Length Mismatch:** If the received length does not match the expected payload size, the packet is discarded.
-   **Timeout:** The STM32F4 UI implements a watchdog; if no data is received for a connected device for 3 seconds, it marks the device as disconnected.
