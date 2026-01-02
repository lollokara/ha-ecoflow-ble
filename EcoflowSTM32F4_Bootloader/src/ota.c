#include "stm32f4xx_hal.h"
#include "flash_ops.h"
#include "uart_protocol.h"
#include <stdio.h>
#include <string.h>

extern UART_HandleTypeDef huart6;
extern UART_HandleTypeDef huart3;

// CRC8 Implementation
uint8_t calculate_crc8(const uint8_t *data, size_t length) {
    uint8_t crc = 0;
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31; // Polynomial 0x31
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

// LED Helpers (External)
extern void LED_SetStatus(int status); // To be implemented in main

void SendAck(uint8_t cmd) {
    uint8_t packet[5];
    packet[0] = START_BYTE;
    packet[1] = OTA_ACK;
    packet[2] = 1;
    packet[3] = cmd; // Ack for which command
    packet[4] = calculate_crc8(&packet[1], 3);
    HAL_UART_Transmit(&huart6, packet, 5, 100);
}

void ProcessOTAMessage(void) {
    uint8_t header[3]; // START, CMD, LEN
    uint8_t payload[256];
    uint8_t crc;

    // Determine Target Address
    // If Active is Bank 1 (0x0800...), we write to Bank 2 (0x0810...)
    // If Active is Bank 2 (0x0810... mapped to 0x0000), we write to Bank 1 (0x0810... mapped?)
    // Wait, Flash_GetActiveBank tells us physical bank.
    // Flash_ToggleBank swaps them.
    // We should always write to the "Inactive" bank.
    // Which is logically mapped to 0x08100000 on F469 when BFB2 is handled correctly?
    // Actually, simply:
    // If BFB2=0: Active=1, Target=2 (0x08100000)
    // If BFB2=1: Active=2, Target=1 (0x08100000 because swap?)
    // NO. If BFB2=1:
    // 0x08000000 -> Bank 2 (Active)
    // 0x08100000 -> Bank 1 (Inactive)
    // So target is ALWAYS 0x08100000 to hit the Inactive bank!

    uint32_t target_base = 0x08100000;

    // However, we must skip the Bootloader area (Sector 12/0 and 13/1).
    // The App Binary starts at offset 0x8000.
    // So write_pointer = 0x08108000.

    uint32_t write_ptr = target_base + 0x8000;

    while (1) {
        // Read Header
        if (HAL_UART_Receive(&huart6, header, 1, 1000) != HAL_OK) continue; // Byte 1: Start
        if (header[0] != START_BYTE) continue;

        HAL_UART_Receive(&huart6, &header[1], 2, 100); // CMD, LEN

        uint8_t cmd = header[1];
        uint8_t len = header[2];

        // Read Payload
        if (HAL_UART_Receive(&huart6, payload, len, 500) != HAL_OK) continue;

        // Read CRC
        if (HAL_UART_Receive(&huart6, &crc, 1, 100) != HAL_OK) continue;

        // Verify CRC
        // CRC is over CMD, LEN, PAYLOAD
        // We can reconstruct a temp buffer or calc iteratively
        // calc_crc = calculate_crc8(header+1, 2); // CMD, LEN
        // calc_crc = update_crc8(calc_crc, payload, len);
        // Let's just flatten it for simplicity (bootloader size concern?)
        // Small buffer approach
        uint8_t check_buf[300];
        check_buf[0] = cmd;
        check_buf[1] = len;
        memcpy(&check_buf[2], payload, len);
        if (calculate_crc8(check_buf, len + 2) != crc) {
            printf("CRC Fail\r\n");
            continue;
        }

        // Process Command
        if (cmd == OTA_CMD_START) {
            printf("OTA Start\r\n");
            LED_SetStatus(1); // OTA Active (Blue)

            // Erase Application Area in Inactive Bank
            // Start from Sector 14 (0x08108000) to 23?
            // Or just erase as we go?
            // Erase full bank is safer/cleaner but takes time (Watchdog!)

            // Let's erase Sector 14 onwards.
            // Bank 2 starts at Sector 12.
            // 12 (16k), 13 (16k) -> Reserved for Bootloader copy
            // 14 (16k), 15 (64k), 16-23 (128k) -> Application

            // Unlock Flash
            Flash_Unlock();

            // Erase Sectors 14 to 23
            // Note: If BFB2 is swapped, GetSector(0x0810xxxx) returns 2-11 or something?
            // No, my GetSector handles mapping to physical sectors.
            // 0x08108000 -> Sector 14 (or Sector 2 if swapped)

            for (uint32_t addr = target_base + 0x8000; addr < target_base + 0x100000; ) {
                 uint32_t sec = GetSector(addr);
                 printf("Erasing Sector %d...\r\n", (int)sec);
                 LED_SetStatus(3); // Blink Orange
                 Flash_EraseSector(sec);

                 // Advance address based on sector size
                 if (sec < 4 || (sec >= 12 && sec < 16)) addr += 0x4000; // 16k
                 else if (sec == 4 || sec == 16) addr += 0x10000; // 64k
                 else addr += 0x20000; // 128k
            }

            write_ptr = target_base + 0x8000; // Reset Ptr
            SendAck(OTA_CMD_START);
        }
        else if (cmd == OTA_CMD_DATA) {
            // Payload structure?
            // User says: "Main App... receives firmware... writes it".
            // Here Bootloader receives firmware.
            // Assume Payload is just raw data stream?
            // Or [Offset(4)][Data...]?
            // Let's assume sequential stream for simplicity unless specified.
            // Wait, packet loss?
            // Better to have [Offset(4)][Data].
            // If len < 4, error.

            // For now, assume User sends chunks sequentially.
            // ESP logic: Read file, send chunk, wait ACK.

            if (write_ptr >= (target_base + 0x100000)) {
                // Overflow
                 printf("Overflow\r\n");
                 LED_SetStatus(2); // Error (Red)
            } else {
                LED_SetStatus(3); // Activity
                if (Flash_Write(write_ptr, payload, len) != 0) {
                     printf("Write Error at %X\r\n", (unsigned int)write_ptr);
                     LED_SetStatus(2); // Error
                } else {
                    write_ptr += len;
                    SendAck(OTA_CMD_DATA);
                }
            }
        }
        else if (cmd == OTA_CMD_END) {
            printf("OTA End. Copying Bootloader...\r\n");

            // 1. Copy Bootloader to Target Bank Sector 0/1
            if (Flash_CopyBootloader() != 0) {
                 printf("Bootloader Copy Failed\r\n");
                 // Don't toggle if copy failed!
            } else {
                 printf("Bootloader Copied. Toggling Bank...\r\n");
                 // 2. Toggle Bank
                 Flash_ToggleBank();

                 // 3. Reset
                 HAL_NVIC_SystemReset();
            }
        }
    }
}
