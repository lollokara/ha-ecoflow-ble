# System Protocols (Inter-Device)

This section details the custom serial protocols used for communication between the subsystems of the EcoFlow Monitor project.

---

## 1. ESP32 <-> STM32 Protocol

The **ESP32 (Gateway)** and **STM32F4 (Display)** communicate via a high-speed UART link. This protocol handles the transmission of telemetry data, device lists, and user control commands.

*   **Baud Rate:** 115200 bps
*   **Packet Format:** `[START] [CMD] [LEN] [PAYLOAD] [CRC8]`
*   **Start Byte:** `0xAA`

### Command Flow

1.  **Handshake:** On boot, the STM32 sends `CMD_HANDSHAKE` (0x20). The ESP32 replies with `CMD_HANDSHAKE_ACK` (0x21).
2.  **Discovery:** The ESP32 pushes the list of discovered BLE devices using `CMD_DEVICE_LIST` (0x22).
3.  **Polling:** The STM32 requests data for a specific device ID using `CMD_GET_DEVICE_STATUS` (0x25). The ESP32 replies with `CMD_DEVICE_STATUS` (0x24).
4.  **Control:** User actions trigger set commands like `CMD_SET_AC` (0x31) or `CMD_SET_VALUE` (0x40).

### Key Structures

#### DeviceStatus (CMD_DEVICE_STATUS)

```c
typedef struct {
    uint8_t id;          // 1=D3, 2=DP3, 3=W2
    uint8_t connected;   // Connection Flag
    char name[16];       // Device Name
    uint8_t brightness;  // Ambient Light (0-100)
    DeviceSpecificData data; // Union of D3/W2/DP3 structs
} DeviceStatus;
```

---

## 2. STM32 <-> RP2040 Protocol (Fan Control)

The **STM32F4 (Display)** controls the **RP2040 (Fan Controller)** to manage cabinet cooling.

*   **Baud Rate:** 115200 bps
*   **Packet Format:** `[START] [CMD] [LEN] [PAYLOAD] [CRC8]`
*   **Start Byte:** `0xBB` (Note: Distinct from the ESP32 protocol)

### Commands

| ID | Name | Direction | Payload | Description |
| :--- | :--- | :--- | :--- | :--- |
| `0x01` | `FAN_CMD_STATUS` | RP2040 -> STM32 | `FanStatus` | Broadcasts current temps and RPMs. |
| `0x02` | `FAN_CMD_SET_CONFIG` | STM32 -> RP2040 | `FanConfig` | Updates fan curves (Min/Max Temp/RPM). |
| `0x03` | `FAN_CMD_GET_CONFIG` | STM32 -> RP2040 | None | Requests current configuration. |
| `0x04` | `FAN_CMD_CONFIG_RESP` | RP2040 -> STM32 | `FanConfig` | Response to GET_CONFIG. |

### Data Structures

#### FanStatus

Sent periodically by the RP2040 (every 1s).

```c
typedef struct {
    float amb_temp;        // Cabinet Temperature (C)
    uint16_t fan_rpm[4];   // RPM of the 4 fans
} FanStatus;
```

#### FanConfig

Stored in RP2040 EEPROM.

```c
typedef struct {
    uint16_t min_speed;   // RPM at start_temp
    uint16_t max_speed;   // RPM at max_temp
    uint8_t start_temp;   // Temp to start fans
    uint8_t max_temp;     // Temp to reach max speed
} FanGroupConfig;

typedef struct {
    FanGroupConfig group1; // Fans 1 & 2
    FanGroupConfig group2; // Fans 3 & 4
} FanConfig;
```

## CRC Algorithm

Both protocols use the **Maxim One-Wire 8-bit CRC** polynomial (`X^8 + X^5 + X^4 + 1`).
The Checksum is calculated over the `CMD`, `LEN`, and `PAYLOAD` fields.

## Author
*   **Lollokara**
