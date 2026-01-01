# EcoFlow BLE Protocol

This document details the reverse-engineered Bluetooth Low Energy (BLE) protocol used by EcoFlow devices, specifically focusing on the Delta 3, Delta Pro 3, and Wave 2.

## Overview

The communication is packet-based and utilizes Google Protocol Buffers (Protobuf) for the payload. The transport layer is BLE, but the application layer implements a custom framing, sequencing, and encryption scheme.

## Packet Structure

### Outer Packet (Encrypted Layer)

All communication begins with an outer packet structure. If the session is authenticated, the `Payload` is AES-128-CBC encrypted.

`[PREFIX] [VERSION] [SEQ] [LEN] [PAYLOAD] [CRC16]`

| Field | Size | Description |
| :--- | :--- | :--- |
| **Prefix** | 2 bytes | Fixed `0x5A 0x5A` |
| **Version** | 1 byte | Protocol Version (e.g., `0x01`) |
| **Seq** | 2 bytes | Rolling Sequence Number |
| **Len** | 2 bytes | Length of the Payload |
| **Payload** | *N* | The inner packet (Encrypted or Plaintext) |
| **CRC16** | 2 bytes | Checksum of the entire packet |

### Inner Packet (Application Layer)

Inside the decrypted payload lies the actual application command.

`[START] [CMD_SET] [CMD_ID] [LEN] [HEADER_LEN] [PROTOBUF] [CRC8]`

*Note: The exact structure varies slightly between Protocol V1 (Delta) and V3 (Wave 2/DP3). The implementation normalizes this.*

## Handshake & Authentication

The most critical part of the connection is the initial handshake to establish a shared session key.

1.  **Public Key Exchange:** The client sends its ECDH Public Key (secp160r1). The device responds with its Public Key.
2.  **Shared Secret:** Both parties calculate the shared secret.
    *   **IV Calculation:** MD5 hash of the full 20-byte shared secret (X-coordinate).
    *   **Key Derivation:** The first 16 bytes of the shared secret become the AES Key.
3.  **Authentication:** The client proves possession of the key by sending an encrypted challenge.

## Supported Commands

### Command Sets (`cmd_set`)

*   `0x01`: Common System Commands (Ping, SN)
*   `0x02`: Power Stream / Inverter
*   `0x11`: BMS (Battery Management System)
*   `0xFE`: Data Push / Subscription

### Reading Data

The primary method for getting data is requesting a `DisplayPropertyUpload` or `BMSHeartbeat`.

*   **Command:** `cmd_set=0xFE`, `cmd_id=0x11` (Delta 3 / System)
*   **Command:** `cmd_set=0x02`, `cmd_id=0x01` (Wave 2 / Power)

## Data Structures (Protobuf)

We map the received Protobuf data into C-structs for easier handling on the microcontrollers.

### Delta 3 (`Delta3DataStruct`)

Relevant fields for the Delta 3 portable power station.

| Field | Type | Description |
| :--- | :--- | :--- |
| `batteryLevel` | `float` | Main SOC (0-100%) |
| `inputPower` | `float` | Total Watts In |
| `outputPower` | `float` | Total Watts Out |
| `acOn` | `bool` | AC Inverter State |
| `dc12vPort` | `bool` | 12V DC Port State |
| `usbOn` | `bool` | USB Port State |
| `acChargingSpeed` | `int32_t` | Custom charge speed (Watts) |

### Wave 2 (`Wave2DataStruct`)

Relevant fields for the portable air conditioner.

| Field | Type | Description |
| :--- | :--- | :--- |
| `mode` | `int32_t` | 0=Cool, 1=Heat, 2=Fan |
| `subMode` | `int32_t` | 0=Max, 1=Sleep, 2=Eco, 3=Manual |
| `setTemp` | `int32_t` | Target Temperature (16-30°C) |
| `envTemp` | `float` | Ambient Temperature |
| `fanValue` | `int32_t` | Fan Speed (0-4) |

### Delta Pro 3 (`DeltaPro3DataStruct`)

Similar to Delta 3, but distinguishes between High Voltage (HV) and Low Voltage (LV) ports.

| Field | Type | Description |
| :--- | :--- | :--- |
| `acHvPort` | `bool` | High Voltage AC Output |
| `solarHvPower` | `float` | High Voltage Solar Input |
| `dcHvInputState` | `int32_t` | State of the HV DC Input |

## Control Logic

To control the device (e.g., turn on AC), we send a `CMD_SET` packet.

*   **Set AC:** `cmd_set=0x02`, `cmd_id=0x8A`, Payload=`{"enabled": 1}` (Protobuf)
*   **Set Charge Speed:** `cmd_set=0x02`, `cmd_id=0x69`, Payload=`{"chgWatts": 400}`

## Author
*   **Lollokara**
