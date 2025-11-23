# Architecture Overview

The EcoflowESP32 library is designed with a clear, singleton-based architecture to manage device connections and interactions. The core components are the `DeviceManager` and the `EcoflowESP32` class.

## Class Diagram

This diagram illustrates the relationship between the key classes in the library.

```mermaid
classDiagram
    direction LR

    class DeviceManager {
        <<Singleton>>
        +getInstance() DeviceManager&
        +initialize() void
        +update() void
        +getDevice(DeviceType) EcoflowESP32*
        -slotD3: DeviceSlot
        -slotW2: DeviceSlot
        -d3: EcoflowESP32
        -w2: EcoflowESP32
    }

    class EcoflowESP32 {
        +begin(userId, sn, address) bool
        +update() void
        +isConnected() bool
        +getBatteryLevel() int
        +setAC(bool on) bool
        -handlePacket(Packet* pkt) void
        -startAuthentication() void
        -_crypto: EcoflowCrypto
        -_data: EcoflowData
        -_pClient: NimBLEClient*
    }

    class EcoflowCrypto {
        +generateKeys() void
        +encrypt(data) vector~uint8_t~
        +decrypt(data) vector~uint8_t~
    }

    class EcoflowData {
        +parse(payload) void
        +batteryLevel: int
        +inputPower: int
    }

    class Packet {
      +fromBytes(data) Packet*
      +toBytes() vector~uint8_t~
    }

    class EncPacket {
      +parsePackets(data) vector~Packet~
      +toBytes(crypto) vector~uint8_t~
    }

    DeviceManager "1" -- "many" EcoflowESP32 : Manages
    EcoflowESP32 "1" -- "1" EcoflowCrypto : Uses
    EcoflowESP32 "1" -- "1" EcoflowData : Stores
    EcoflowESP32 "1" -- "many" Packet : Handles
    EcoflowESP32 "1" -- "many" EncPacket : Handles
```

## Core Components

### `DeviceManager`

The `DeviceManager` is the central nervous system of the library. As a **singleton**, it ensures that there is only one instance managing all device connections. Its primary responsibilities are:

-   **Discovery:** Scanning for and discovering specified EcoFlow devices.
-   **Connection Management:** Handling the connection and reconnection logic for multiple devices.
-   **State Caching:** Storing and retrieving device credentials from non-volatile storage.
-   **Device Access:** Providing a single point of access to the individual `EcoflowESP32` instances.

### `EcoflowESP32`

This is the main class that represents a single EcoFlow device. Each instance is responsible for:

-   **BLE Client:** Managing the underlying NimBLE client and its connection state.
-   **Authentication:** Executing the entire authentication handshake.
-   **Command Interface:** Providing the public API for sending commands (e.g., `setAC()`) and receiving data (e.g., `getBatteryLevel()`).
-   **Packet Handling:** Parsing incoming `EncPacket`s and `Packet`s and directing them to the appropriate handlers.

### `EcoflowCrypto`

This class abstracts all cryptographic operations, including:

-   Generating the ECDH public/private key pair.
-   Calculating the shared secret.
-   Deriving the AES key and IV.
-   Performing AES-128-CBC encryption and decryption.

### `Packet` and `EncPacket`

These classes are data structures that represent the two layers of the EcoFlow BLE protocol. They contain the logic for serializing and deserializing the packets to and from byte arrays, including checksum and CRC validation.
