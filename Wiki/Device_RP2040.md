# RP2040 Firmware

The RP2040 (Raspberry Pi Pico) acts as the **Thermal Controller**. It operates independently to ensure the safety of the equipment cabinet, even if the main display crashes or disconnects.

## Key Features

*   **PWM Fan Control:** Drives 4x PC Case Fans (12V PWM) with a 25kHz signal.
*   **Tachometer Input:** Reads RPM feedback from all 4 fans to detect failures.
*   **Temperature Sensing:** Reads a DS18B20 OneWire temperature sensor.
*   **Linear Curve:** Implements a configurable linear interpolation between Min/Max temperature and speed.
*   **Failsafe:** Defaults to a safe profile if no configuration is received.

## Code Structure

The firmware is a single-file Arduino sketch (`src/main.cpp`) optimized for reliability.

*   `setup()`: Initializes hardware (PWM, Interrupts, UART) and loads config from EEPROM.
*   `loop()`:
    1.  **Read Temp:** Queries the DS18B20 (Non-blocking where possible).
    2.  **Calc PWM:** Maps Temperature -> RPM -> PWM Duty Cycle.
    3.  **Update Fans:** Writes new PWM values.
    4.  **Report:** Sends status packet to STM32.
    5.  **Listen:** Checks UART for new configuration commands.

## Hardware Interfaces

*   **PWM:** Pins 10, 11, 12, 13 (25kHz)
*   **Tach:** Pins 14, 15, 16, 17 (Interrupt-driven pulse counting)
*   **OneWire:** Pin 4 (DS18B20)
*   **UART:** Pins 0 (TX), 1 (RX) - Connected to STM32 UART4.

## Control Algorithm

The fan speed is calculated based on two points:
*   **(StartTemp, MinSpeed)**
*   **(MaxTemp, MaxSpeed)**

```cpp
Factor = (CurrentTemp - StartTemp) / (MaxTemp - StartTemp);
TargetRPM = MinSpeed + (Factor * (MaxSpeed - MinSpeed));
```

The resulting RPM is then mapped to a 0-255 PWM value.

## Configuration Persistence

Configuration (Fan Curves) is stored in the emulated EEPROM of the RP2040. This allows the settings to survive power cycles.

## Author
*   **Lollokara**
