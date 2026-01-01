# STM32 Firmware Documentation

**Author:** Lollokara

## Overview

The **EcoflowSTM32F4** firmware runs on the **STM32F469I-Discovery** board. It is the user-facing "Frontend" of the system, providing a responsive touch interface and coordinating the other microcontrollers.

## Architecture

The firmware is built on **FreeRTOS** and uses **LVGL 8.3** for the GUI.

### FreeRTOS Tasks

1.  **`StartDisplayTask` (Priority: HIGH)**
    *   **Stack:** 8192 Words.
    *   **Role:** The GUI thread. It handles Touch Input (polling FT6x06), runs the LVGL Timer Handler (`lv_timer_handler`), and renders the frame.
    *   **Optimization:** Uses Direct Memory Access (DMA2D / Chrom-ART) for hardware-accelerated rendering.

2.  **`StartUARTTask` (Priority: NORMAL)**
    *   **Stack:** 2048 Words.
    *   **Role:** The Communication thread.
    *   **Logic:**
        *   Polls the UART Ring Buffer for incoming packets from the ESP32.
        *   Parses packets (`CMD_DEVICE_STATUS`, `CMD_DEVICE_LIST`).
        *   Updates the global `device_cache`.
        *   Sends outgoing commands (`CMD_SET_AC`, `CMD_GET_DEVICE_LIST`) to the ESP32.

3.  **`StartFanTask` (Priority: LOW)**
    *   **Stack:** 1024 Words.
    *   **Role:** Manages the RP2040 Fan Controller.
    *   **Logic:** Polls the RP2040 every second for temperature/RPM data and updates the UI state.

## User Interface (UI)

The UI is implemented in C using the LVGL library.

### File Structure (`src/ui/`)
*   **`ui_lvgl.c`:** The main UI entry point. Contains the `UI_Init` function and the main update loop.
*   **`ui_view_connections.c`:** The "Connections" screen, listing found devices.
*   **`ui_view_wave2.c`:** The specialized control panel for the Wave 2 (Mode, Temp, Fan).
*   **`ui_view_fan.c`:** The configuration screen for the RP2040 Fan Controller.

### Performance Optimizations
*   **Double Buffering:** Prevents screen tearing by writing to a back buffer while the front buffer is displayed.
*   **Partial Redraw:** LVGL tracks "dirty" areas and only redraws pixels that have changed.
*   **Asset Caching:** Fonts and Icons are compiled into the firmware (C arrays) to avoid slow file system access.

## Hardware Abstraction Layer (BSP)

The project includes a stripped-down Board Support Package (BSP) in `lib/bsp/` to minimize bloat.
*   **Drivers Used:** LTDC (LCD Controller), DSI (Display Interface), I2C (Touchscreen), SDRAM (Frame Buffer).
*   **Removed:** Audio, QSPI, Camera, Ethernet (unused).
