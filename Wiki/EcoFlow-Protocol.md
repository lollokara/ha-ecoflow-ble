# EcoFlow Protocol Specification

**Author:** Lollokara

This document details the reverse-engineered communication protocols used in this project. It covers two distinct layers:
1.  **Inter-MCU UART Protocol:** How the ESP32 communicates with the STM32.
2.  **EcoFlow BLE Protocol:** The encrypted wireless protocol used by the Power Station.

---

## 1. Inter-MCU UART Protocol (ESP32 <-> STM32)

To offload the heavy BLE encryption and Protobuf parsing from the UI controller, the ESP32 acts as a "modem". It communicates with the STM32 using a custom binary protocol over UART (115200 baud).

### Packet Structure
All packets sent between the ESP32 and STM32 follow this format:

| Byte | Field | Description |
| :--- | :--- | :--- |
| 0 | **START** | Fixed `0xAA` |
| 1 | **CMD** | Command ID (see below) |
| 2 | **LEN** | Length of the Payload (0-255) |
| 3... | **PAYLOAD** | Variable data |
| N | **CRC** | 8-bit CRC of the packet (excluding Start & CRC) |

### Command Table

#### STM32 -> ESP32 (Requests)
| CMD ID | Name | Description |
| :--- | :--- | :--- |
| `0x20` | `CMD_HANDSHAKE` | Initiates connection. ESP32 responds with `ACK` (0x21). |
| `0x25` | `CMD_GET_DEVICE_STATUS` | Request the latest telemetry for a specific device ID. |
| `0x26` | `CMD_GET_DEVICE_LIST` | Request the list of found/connected devices. |
| `0x30` | `CMD_SET_WAVE2` | Control Wave 2 (Set Mode, Temp, Fan). |
| `0x31` | `CMD_SET_AC` | Toggle AC Inverter (On/Off). |
| `0x32` | `CMD_SET_DC` | Toggle DC Ports (On/Off). |
| `0x40` | `CMD_SET_VALUE` | Set numeric limits (Max Charge, Input Watts). |
| `0x50` | `CMD_POWER_OFF` | Turn off the EcoFlow device. |

#### ESP32 -> STM32 (Responses & Events)
| CMD ID | Name | Description |
| :--- | :--- | :--- |
| `0x21` | `CMD_HANDSHAKE_ACK` | Confirms the UART link is active. |
| `0x22` | `CMD_DEVICE_LIST` | Sends the list of devices (Count, IDs, Names). |
| `0x24` | `CMD_DEVICE_STATUS` | Sends the full telemetry struct for a device. |
| `0x61` | `CMD_DEBUG_INFO` | Sends ESP32 WiFi IP and connection stats. |

---

## 2. Data Structures

The raw Protobuf data from the EcoFlow device is parsed by the ESP32 into C-compatible structs (POD - Plain Old Data) before being sent to the STM32. This simplifies the STM32 firmware significantly.

### Device Status Wrapper
The `DeviceStatus` struct is the main payload for `CMD_DEVICE_STATUS` (0x24).

```c
typedef struct {
    uint8_t id;          // DeviceType enum (1=Delta3, 2=DeltaPro3, 3=Wave2)
    uint8_t connected;   // 1=Connected, 0=Disconnected
    char name[16];       // Device Name
    uint8_t brightness;  // Ambient Light Level (10-100%)
    DeviceSpecificData data; // Union of specific device structs
} DeviceStatus;
```

### Delta 3 Data (`Delta3DataStruct`)
Used for Delta 3 and Delta 3 Plus devices.
*   **batteryLevel** (float): 0-100%
*   **inputPower** (float): Total Watts In
*   **outputPower** (float): Total Watts Out
*   **acOn / dcOn** (bool): State of inverters
*   **remainingTime** (int): Minutes until empty/full

### Wave 2 Data (`Wave2DataStruct`)
Used for the Wave 2 Portable Air Conditioner.
*   **mode** (int): 0=Cool, 1=Heat, 2=Fan
*   **setTemp** (int): Target temperature (16-30°C)
*   **fanValue** (int): Fan Speed
*   **envTemp** (float): Ambient Temperature
*   **outLetTemp** (float): Air Outlet Temperature

---

## 3. EcoFlow BLE Protocol (Wireless)

The communication between the ESP32 and the EcoFlow device uses a complex, encrypted protocol.

### Authentication Flow (Simplified)
1.  **Handshake:** The Client (ESP32) sends a `Handshake` packet.
2.  **Key Exchange:** The Device responds with its Public Key (ECDH).
3.  **Shared Secret:** The Client calculates a Shared Secret using its Private Key and the Device's Public Key.
4.  **Session Key:** An AES-128 Session Key is derived from the Shared Secret.
5.  **Authentication:** The Client sends an encrypted "Auth" packet to prove it holds the correct credentials (User ID, Serial Number).

### Packet Wrapping
Wireless packets are wrapped in a proprietary frame format:

`[0xAA] [FrameType] [Len] [Seq] [Src] [Dest] [Payload] [CRC16]`

*   **FrameType:** `0x01` (Unencrypted) or `0x5A` (Encrypted).
*   **Payload:** If encrypted, this contains the AES-128-CBC encrypted Protobuf message.

### Protobuf Messages
The payload itself is a serialized Google Protobuf message.
*   **pd335_sys.proto:** Contains most system definitions (Battery, Power, Config).
*   **mr521.proto:** Used for specific sub-systems or newer devices.

The ESP32 uses `nanopb` to decode these messages into the C structs defined above.
