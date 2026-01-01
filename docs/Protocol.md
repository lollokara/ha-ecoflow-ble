# ≡ PROTOCOL REFERENCE // DEEP DIVE

> **ACCESS LEVEL:** CLASSIFIED
> **SUBJECT:** COMMUNICATION PROTOCOLS

This document details the communication layers used within the Cyber Deck ecosystem, specifically the **Inter-MCU Protocol** (ESP32 <-> STM32) and the **EcoFlow BLE Protocol** (ESP32 <-> Device).

---

## ≡ PART 1: INTER-MCU PROTOCOL (UART)

The ESP32 and STM32 communicate via a high-speed UART link (115200 baud). This protocol is designed for reliability and low overhead.

### PACKET STRUCTURE
All frames follow this binary format:

`[START] [CMD] [LEN] [PAYLOAD] [CRC8]`

| Offset | Field | Description |
| :--- | :--- | :--- |
| 0x00 | **START** | Fixed `0xAA`. Sync byte. |
| 0x01 | **CMD** | Command ID (Operation Code). |
| 0x02 | **LEN** | Length of the `PAYLOAD` field (0-255). |
| 0x03 | **PAYLOAD** | Variable length data. |
| 0x03+N | **CRC8** | Maxim One-Wire CRC of CMD, LEN, and PAYLOAD. |

### COMMAND SET

#### 1. System Commands
| ID | Name | Direction | Description |
| :--- | :--- | :--- | :--- |
| `0x20` | `CMD_HANDSHAKE` | STM -> ESP | Initiates connection. |
| `0x21` | `CMD_HANDSHAKE_ACK` | ESP -> STM | Confirms connection. |
| `0x22` | `CMD_DEVICE_LIST` | ESP -> STM | Pushes list of discovered BLE devices. |
| `0x24` | `CMD_DEVICE_STATUS` | ESP -> STM | Pushes full telemetry struct for a device. |

#### 2. Control Commands
| ID | Name | Direction | Description |
| :--- | :--- | :--- | :--- |
| `0x30` | `CMD_SET_WAVE2` | STM -> ESP | Sets Wave 2 params (Mode, Temp, Fan). |
| `0x31` | `CMD_SET_AC` | STM -> ESP | Payload: `[0/1]`. Toggles AC Inverter. |
| `0x32` | `CMD_SET_DC` | STM -> ESP | Payload: `[0/1]`. Toggles DC/USB Ports. |
| `0x40` | `CMD_SET_VALUE` | STM -> ESP | Sets scalar limits (Charge Speed, SOC). |

### DATA STRUCTURES

#### DeviceStatus (CMD_DEVICE_STATUS)
The core telemetry packet. It contains a header and a union of device-specific data.

```c
struct DeviceStatus {
    uint8_t id;          // 1=D3, 2=DP3, 3=W2
    uint8_t connected;   // Boolean
    char name[16];       // "Delta 3"
    uint8_t brightness;  // 10-100%
    DeviceSpecificData data; // Union
};
```

---

## ≡ PART 2: ECOFLOW BLE PROTOCOL

The EcoFlow devices use two main protocol versions over BLE. The ESP32 abstracts these differences.

### AUTHENTICATION HANDSHAKE (V3)
Modern devices (Delta 3, Delta Pro 3) require an encrypted handshake.

1.  **Key Exchange (ECDH)**:
    *   Client sends public key (secp160r1).
    *   Device responds with its public key + random salt.
    *   Shared Secret is derived.
2.  **Secret Derivations**:
    *   **AES Key**: First 16 bytes of Shared Secret.
    *   **IV**: MD5 Hash of the full 20-byte Shared Secret.
3.  **Verification**:
    *   Client encrypts a known challenge.
    *   Device validates and unlocks the data stream.

### PACKET STRUCTURE (V3 PROTOBUF)
V3 packets are wrapped in an encrypted container.

```mermaid
graph LR
    P[Raw Packet] -->|Protobuf Serialize| PB[Protobuf Bytes]
    PB -->|Add Header| FR[Frame]
    FR -->|AES-128-CBC| ENC[Encrypted Payload]
    ENC -->|Add Preamble 0xAA| BLE[BLE Characteristic]
```

*   **Preamble**: `0xAA`
*   **Header**: Contains version, sequence number, source/dest IDs.
*   **Payload**: Protobuf message (e.g., `pd335_sys_DisplayPropertyUpload`).
*   **CRC**: CRC16 checksum.

### COMMAND MAPPING

| Action | Command ID | Protocol |
| :--- | :--- | :--- |
| **Get Status** | `0x11` | V3 (Protobuf) |
| **Set AC** | `0x84` | V3 (Write Config) |
| **Set DC** | `0x84` | V3 (Write Config) |

---

## ≡ IMPLEMENTATION NOTES

*   **Endianness**: All multi-byte integers are Little-Endian.
*   **Floating Point**: Transmitted as standard IEEE 754 4-byte floats.
*   **Safety**: The ESP32 sanitizes all floats (checking for `NaN`/`Inf`) before sending to STM32 to prevent FPU faults.

> *Authorized Personnel Only.*
