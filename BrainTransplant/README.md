# Standalone BrainTransplant Project

This project contains three standalone components for the BrainTransplant system.

## 1. BrainTransplantCore (Bootloader)
The STM32F469I-Discovery bootloader.
- **Role:** Handles OTA updates via UART and boots the main application.
- **Location:** Flash Bank 1 Sector 0 (0x08000000) and Flash Bank 2 Sector 0 (0x08100000).
- **Features:** Double-bank updates, rollback protection, CRC verification.

## 2. BrainTransplantESP32 (Updater)
The ESP32-S3 firmware serving as the update controller.
- **Role:** Hosts a Web Interface for uploading firmware to itself and to the STM32F4.
- **Location:** ESP32 Flash.
- **Features:** Web OTA, UART OTA bridge to STM32, "BrainTransplant" branded UI.
- **Usage:** Connect to `Ecoflow-AP` (or configured WiFi), navigate to IP address, open Settings to flash firmware.

## 3. BrainTransplantF4 (Demo App)
A minimal "Hello World" application for the STM32F469I-Discovery.
- **Role:** Demonstrates successful boot and UART communication.
- **Location:** Flash Address `0x08008000` (Sector 2).
- **Function:** Initializes System Clock (180MHz), UART3 (115200 8N1), and prints "Hello World" every second.

## Inner Workings

### Boot Process
1. **Reset:** STM32 starts at `0x08000000`.
2. **Bootloader:** Checks for valid application at `0x08008000` (Stack Pointer check).
   - If valid: Jumps to App.
   - If invalid/OTA requested: Enters OTA mode (Fast Blue LED).
3. **Application:** Runs `Hello World`.

### OTA Process
1. User uploads `firmware.bin` (compiled from BrainTransplantF4) to ESP32 Web UI.
2. ESP32 sends `CMD_OTA_START` to STM32 via UART1 (921600 baud).
3. STM32 (if in App) resets to Bootloader. Bootloader accepts OTA.
4. ESP32 streams firmware in chunks.
5. Bootloader writes to Inactive Bank.
6. Upon completion (CRC match), Bootloader swaps banks (BFB2 toggle) and resets.

## Compilation Order
1. **BrainTransplantCore:** Compile and upload to STM32 first (via ST-Link).
2. **BrainTransplantF4:** Compile. The post-build script will merge it with the Bootloader binary.
3. **BrainTransplantESP32:** Compile and upload to ESP32.

Use PlatformIO to compile each project.
