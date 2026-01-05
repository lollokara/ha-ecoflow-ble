# Wiring Setup

## Connection between ESP32 and STM32F469I-Discovery

| ESP32-S3 Pin | STM32F4 Pin | Function | Description |
|--------------|-------------|----------|-------------|
| GPIO 18 (RX) | PG9 (TX)    | UART RX  | ESP32 receives data from STM32 (USART6) |
| GPIO 17 (TX) | PG14 (RX)   | UART TX  | ESP32 transmits data to STM32 (USART6) |
| GND          | GND         | Ground   | Common Ground |
| 5V / 3V3     | 5V / 3V3    | Power    | Power supply (if shared) |

**Note:** The Demo App (BrainTransplantF4) uses `USART3` (PB10/PB11) for "Hello World" debug output, which is connected to the ST-Link Virtual COM Port on the Discovery board.

The OTA Update uses `USART6` (PG9/PG14) on the STM32 side (handled by Bootloader). Ensure these pins are connected to the ESP32.

## Debugging
- **STM32 Debug:** Connect USB to ST-Link port. Open serial terminal at 115200 baud.
- **ESP32 Debug:** Connect USB to ESP32 port. Open serial terminal at 115200 baud.
