# STM32F4 Firmware

The STM32F469I-Discovery board powers the **User Interface**. It runs a real-time operating system (FreeRTOS) to manage the display, touch input, and serial communications concurrently.

## Key Features

*   **LVGL UI:** A modern, touch-friendly interface using LVGL 8.3.
*   **Real-Time Multitasking:** FreeRTOS separates UI rendering, data parsing, and fan control into distinct tasks.
*   **Hardware Acceleration:** Utilizes the STM32 Chrom-ART (DMA2D) for high-performance graphics.
*   **Dual-UART Bridge:** Communicates with the ESP32 (Upstream) and RP2040 (Downstream).

## Code Structure

*   `src/main.c`: Hardware initialization and FreeRTOS scheduler start.
*   `src/ui/`: Contains all LVGL UI code (`ui_lvgl.c`) and screen definitions (`ui_view_*.c`).
*   `src/uart_task.c`: Handles high-speed data exchange with the ESP32.
*   `src/fan_task.c`: Manages the connection to the RP2040 Fan Controller.
*   `src/display_task.c`: The main UI thread, handling LVGL timers and rendering.

## FreeRTOS Tasks

| Task Name | Priority | Stack Size | Description |
| :--- | :--- | :--- | :--- |
| `display_task` | High | 8192 | Runs the LVGL `lv_timer_handler`. Handles touch input and screen updates. |
| `uart_task` | Normal | 2048 | Polls the UART Ring Buffer. Parses packets from ESP32. Dispatches events to UI. |
| `fan_task` | Low | 1024 | Periodically polls the RP2040 for temp/rpm and sends config updates. |

## Memory Management

*   **Display Buffer:** Double-buffered frame buffer in external SDRAM.
*   **Queues:** FreeRTOS Queues are used to pass data between the UART task and the Display task safely.
*   **Semaphores:** Used to protect shared resources like the UART transmission line.

## User Interface (LVGL)

The UI is built using the **Light and Versatile Graphics Library**.

*   **Theme:** Custom Dark Theme with Material Design Icons.
*   **Screens:**
    *   **Connections:** Scan and select devices.
    *   **Dashboard:** Main view with battery circle, power flow, and quick toggles.
    *   **Settings:** Configuration sliders for charging limits.
    *   **Fan Control:** Interactive curves for the cabinet cooling.
    *   **Debug:** Raw data inspection.

## Build Environment

*   **Board:** `disco_f469ni`
*   **Framework:** STM32Cube (via PlatformIO)
*   **Middleware:** FreeRTOS, LVGL

## Author
*   **Lollokara**
