# RP2040 Firmware Documentation

**Author:** Lollokara

## Overview

The **EcoflowRP2040** firmware runs on a **Raspberry Pi Pico**. It acts as a dedicated Thermal Controller, ensuring that the batteries and inverters housed in the cabinet remain within safe operating temperatures.

## Hardware Interface

| Pin | Function | Description |
| :--- | :--- | :--- |
| **GP0** | **UART TX** | Sends telemetry to STM32 (RX). |
| **GP1** | **UART RX** | Receives config from STM32 (TX). |
| **GP4** | **OneWire** | Data line for DS18B20 Temp Sensors. |
| **GP10** | **PWM 1** | Fan 1 Control Signal (25kHz). |
| **GP11** | **PWM 2** | Fan 2 Control Signal. |
| **GP12** | **PWM 3** | Fan 3 Control Signal. |
| **GP13** | **PWM 4** | Fan 4 Control Signal. |
| **GP14** | **TACH 1** | Fan 1 Speed Sensor (Input). |
| **GP15** | **TACH 2** | Fan 2 Speed Sensor. |
| **GP16** | **TACH 3** | Fan 3 Speed Sensor. |
| **GP17** | **TACH 4** | Fan 4 Speed Sensor. |

## Control Logic

The firmware implements a linear thermal control loop:

1.  **Read Temperature:** Queries the DS18B20 sensor chain.
2.  **Calculate Speed:**
    *   If `Temp < StartTemp`: **0% PWM** (Silent).
    *   If `Temp > MaxTemp`: **100% PWM** (Full Blast).
    *   In-between: Linear interpolation between `MinSpeed` and `MaxSpeed`.
3.  **Drive PWM:** Updates the duty cycle on pins GP10-GP13.
4.  **Monitor RPM:** Counts tachometer pulses to detect stalled fans.

## Communication Protocol

The RP2040 uses a custom binary protocol to talk to the STM32.

**Packet:** `[0xBB] [CMD] [LEN] [PAYLOAD] [CRC8]`

### Commands
*   **`0x01` STATUS:** Sent every 1s. Contains current Temp (float) and 4x RPMs (uint16).
*   **`0x02` SET_CONFIG:** Writes new Start/Max temps and Min/Max speeds to EEPROM.
*   **`0x03` GET_CONFIG:** Requests current configuration.
*   **`0x04` CONFIG_RESP:** Returns the configuration struct.
