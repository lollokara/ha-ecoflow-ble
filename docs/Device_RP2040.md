# ≡ RP2040 THERMAL // COOLING SUBSYSTEM

> **HARDWARE:** Raspberry Pi Pico (RP2040)
> **ROLE:** THERMAL MANAGEMENT & FAN CONTROL

The **RP2040 Thermal Controller** ensures the Cyber Deck hardware remains within safe operating temperatures. It acts as an independent subsystem, autonomously reading sensors and adjusting fan speeds based on a configured curve, while reporting telemetry to the main STM32 unit.

---

## ≡ FUNCTIONALITY

### 1. PWM Fan Control
Drives up to 4 standard 4-pin PC fans (12V/5V).
*   **Frequency**: 25kHz (Standard Intel PWM Spec).
*   **Resolution**: 10-bit resolution for smooth speed transitions.
*   **Tachometer**: Reads RPM feedback to detect stalled fans.

### 2. Temperature Monitoring
Interfaces with DS18B20 digital sensors via OneWire.
*   **Precision**: 12-bit resolution (0.0625°C).
*   **Bus**: Multiple sensors on a single GPIO pin.

### 3. Logic & Config
*   **Curve**: Linear interpolation between `MinTemp` and `MaxTemp`.
*   **Failsafe**: If UART comms are lost or sensors fail, fans default to 100% speed.
*   **Storage**: Config (Min/Max temps, RPM limits) is saved in emulated EEPROM.

---

## ≡ COMMUNICATION

The RP2040 communicates with the STM32 via UART (115200 baud).

### Packet Format
Simple binary protocol: `[START] [CMD] [LEN] [PAYLOAD] [CRC]`

| Command | ID | Description |
| :--- | :--- | :--- |
| `CMD_REPORT` | `0x10` | Sends current RPM and Temp. |
| `CMD_SET_CFG`| `0x20` | Updates fan curve settings. |
| `CMD_GET_CFG`| `0x21` | Requests current settings. |

---

## ≡ WIRING MAP

### Fan Headers (Typical)
| Signal | GPIO | Notes |
| :--- | :--- | :--- |
| **PWM 1** | GP10 | Fan Group A Speed |
| **PWM 2** | GP11 | Fan Group B Speed |
| **PWM 3** | GP12 | Aux Fan |
| **PWM 4** | GP13 | Aux Fan |
| **TACH 1** | GP14 | RPM Input |
| **TACH 2** | GP15 | RPM Input |

### Sensors & Comms
| Signal | GPIO | Notes |
| :--- | :--- | :--- |
| **OneWire**| GP4 | DS18B20 Data Line |
| **UART TX**| GP0 | To STM32 RX (PA1) |
| **UART RX**| GP1 | From STM32 TX (PA0) |

---

## ≡ CODE STRUCTURE

### `main.cpp`
The core loop runs on a simple scheduler (no OS).

```cpp
void loop() {
    // 1. Read Temperatures (Every 1s)
    if (timer_temp.check()) read_sensors();

    // 2. Calculate Fan Speed
    int pwm = calculate_curve(current_temp);

    // 3. Update Hardware
    set_pwm(pwm);

    // 4. Report to STM32
    if (timer_report.check()) send_telemetry();
}
```

> *Caution: Ensure external 12V power is isolated from the RP2040 3.3V logic.*
