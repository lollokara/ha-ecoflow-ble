# ≡ STM32 INTERFACE // VISUAL CORE

> **HARDWARE:** STM32F469I-Discovery
> **ROLE:** GRAPHICAL UI & SYSTEM MASTER

The **STM32 Interface** provides the visual feedback and touch control for the Cyber Deck. Built on the STM32F469 "Discovery" kit, it utilizes the Chrom-ART Accelerator (DMA2D) to render a fluid, high-frame-rate interface on the 800x480 MIPI-DSI display.

---

## ≡ SOFTWARE STACK

### 1. Operating System
*   **FreeRTOS**: The system runs a preemptive real-time OS to manage concurrent tasks.
    *   `DisplayTask`: Handles LVGL rendering and touch input (High Priority).
    *   `UARTTask`: Processes high-speed data from ESP32 and RP2040 (Medium Priority).
    *   `FanTask`: Logic for thermal management config (Low Priority).

### 2. Graphics Engine
*   **LVGL 8.3.11**: Light and Versatile Graphics Library.
*   **Drivers**:
    *   **DMA2D**: Hardware acceleration for memory copies and blending.
    *   **DSI**: MIPI Display Serial Interface for high-bandwidth video.
    *   **FT6x06**: I2C Touch Controller driver.

### 3. BSP (Board Support Package)
Customized version of the ST Microelectronics BSP, stripped down to the bare essentials (LCD, SDRAM, Touch) to minimize compile time and binary size.

---

## ≡ UI STRUCTURE

The interface is divided into functional "Views":

### [ DASHBOARD ]
The main landing screen.
*   **Battery Arc**: A massive, animated arc showing SOC (State of Charge).
*   **Power Flow**: Real-time wattage for Input (Solar/Grid) and Output (AC/DC).
*   **Quick Toggles**: Large, touch-friendly buttons for AC and DC ports.

### [ CONNECTIONS ]
A matrix of available device slots.
*   **Dynamic Status**: Shows "CONNECTED", "SEARCHING", or "OFFLINE" for each slot.
*   **Pairing**: Allows the user to initiate connections to different device types (Delta 3, Wave 2, etc.).

### [ WAVE 2 CONTROL ]
A dedicated control panel for the Wave 2 Air Conditioner.
*   **Mode Selector**: Cool / Heat / Fan.
*   **Temp Slider**: Precision temperature control.
*   **Fan Speed**: Manual override for the internal fan.

---

## ≡ TECHNICAL DEEP DIVE

### Rendering Pipeline

To achieve 60 FPS on the STM32F469, the rendering pipeline is heavily optimized:

1.  **Double Buffering**: Two full-frame buffers in SDRAM. While one is displayed, the other is rendered.
2.  **Dirty Rectangles**: LVGL only redraws the parts of the screen that changed.
3.  **DMA Transfer**: Pixel data is moved via DMA, leaving the CPU free for logic.

```c
// ui_lvgl.c - Rendering Loop
void UI_Render(void) {
    if (needs_redraw) {
        lv_timer_handler(); // Calculate changes
        // DMA2D automatically flushes buffer to display
    }
}
```

### UART Ring Buffer

To prevent data loss at 115200 baud, the UART task uses a Ring Buffer.

*   **ISR**: Moves bytes from Hardware Register -> Ring Buffer.
*   **Task**: Moves bytes from Ring Buffer -> Linear Parser Buffer.
*   **Zero Copy**: The parser operates directly on the linear buffer where possible.

---

## ≡ WIRING & PINOUT

| Function | Pin | Description |
| :--- | :--- | :--- |
| **ESP32 RX** | PG9 | Data FROM ESP32 |
| **ESP32 TX** | PG14 | Data TO ESP32 |
| **RP2040 RX** | PA1 | Data FROM Fan Controller |
| **RP2040 TX** | PA0 | Data TO Fan Controller |
| **Debug RX** | PB11 | System Logs |
| **Debug TX** | PB10 | System Logs |

> *Note: The Display uses the internal DSI interface, not accessible via headers.*
