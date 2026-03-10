# ≡ AGENTS ONBOARDING & GUIDELINES // ECOFLOW CYBER DECK

**Welcome, AI Agent.** This file contains the critical context, architectural overview, and mandatory procedures you must follow when working on the `ha-ecoflow-ble` repository. This project is a multi-MCU system designed to interface directly with EcoFlow power stations offline.

---

## 1. SYSTEM ARCHITECTURE
The system operates on a Hub-and-Spoke model, consisting of three primary hardware components:

*   **ESP32-S3 Gateway (in `EcoflowESP32/`)**: The communications hub. It handles BLE connections, the complex ECDH+AES-128 cryptographic handshake, and translates proprietary protocols (Protobuf V3, Binary V2) into a simplified UART stream. Built in C++ using `NimBLE` and `nanopb`.
*   **STM32F469I-Discovery UI (in `EcoflowSTM32F4/`)**: The visual master. It runs a FreeRTOS + LVGL 8.3 graphical interface, sending commands to the ESP32 and displaying telemetry. Built primarily in C using the STM32 HAL.
*   **RP2040 Thermal Controller (in `EcoflowRP2040/`)**: Manages the environmental cooling (fans/temps) for the deck itself. Built in C++ using the Arduino Core (`earlephilhower` core).

---

## 2. CODING CONVENTIONS & MANDATORY RULES

When modifying code in this repository, you **MUST** adhere to the following rules:

*   **Language & Paradigms**: Use C++ for ESP32/RP2040 and primarily C for the STM32 project. Adhere strictly to the existing FreeRTOS task patterns and memory management (e.g., ring buffers for UART, DMA for rendering).
*   **Synchronous Duplication**: The `ecoflow_protocol.h` file is duplicated between `EcoflowSTM32F4/lib/EcoFlowComm/` and `EcoflowESP32/lib/EcoFlowComm/`. **Any modifications must be mirrored exactly in both locations** to ensure struct layouts align correctly for UART transmission.
*   **Memory Management (LVGL)**: In STM32 UI code, repeatedly calling `lv_obj_add_style` on the same object without first explicitly removing the old style causes memory leaks. Always call `lv_obj_remove_style` before re-applying dynamically changing styles.
*   **Verification (Compilation)**: **Always** compile the code to test your edits before submitting. See Section 4 for commands.
*   **Documentation Synchronization**: Every functional edit, new feature, or protocol change must be reflected in the relevant documentation within the `docs/` folder.
*   **Security**: Do not hardcode or commit any personal credentials. The ESP32 `Credentials.h` file is `.gitignore`d for a reason.

---

## 3. RAG INDEXING SYSTEM (MANDATORY UPDATE)

This repository contains a custom local Retrieval-Augmented Generation (RAG) indexing system in the `rag_index/` directory. It uses a FAISS embeddings index and python scripts to maintain codebase knowledge.

*   **Searching**: You may use `python rag_index/rag_search.py "<query>"` to quickly locate relevant context or files.
*   **[>> CRITICAL REQUIREMENT <<]**: **You must update the RAG index on every push or significant code change.**
    *   **Command**: Run `python rag_index/update_index.py` before finalizing your task to ensure the documentation embeddings reflect your new code.

---

## 4. BUILD & COMPILATION COMMANDS

The firmware for the MCUs is built using PlatformIO. You must execute these commands from the repository root to verify your changes:

*   **Compile ESP32 Gateway:**
    ```bash
    pio run -d EcoflowESP32
    ```
*   **Compile STM32 Display:**
    ```bash
    pio run -d EcoflowSTM32F4
    ```
*   **Compile RP2040 Fan Controller:**
    ```bash
    pio run -d EcoflowRP2040
    ```

If you introduce new dependencies, ensure they are correctly added to the respective `platformio.ini` files.

---

## 5. HARDWARE & PROTOCOL PITFALLS

*   **ESP32 Watchdog**: High-volume logging on the ESP32 can cause Task Watchdog (WDT) timeouts. Long UART transmissions must be chunked or delayed (e.g., `vTaskDelay(10)`).
*   **BLE Disconnects**: To prevent residual corrupted BLE data from causing immediate disconnections upon reconnect, buffers and queues must be explicitly drained (`_rxBuffer.clear()`, queue draining) on connect/disconnect events.
*   **ESP32 Serial Conflict**: `Serial.begin()` is deliberately disabled in the main loop to prevent interference with the LightSensor on GPIO 1 (ADC1 CH0). Debug logs are routed through `Stm32Serial` to the STM32 via UART.

> *END OF INSTRUCTIONS. PROCEED WITH CAUTION.*
