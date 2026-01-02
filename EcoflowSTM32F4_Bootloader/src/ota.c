#include "main.h"
#include <string.h>
#include <stdio.h>

// Commands
#define CMD_OTA_START 0xF0
#define CMD_OTA_CHUNK 0xF1
#define CMD_OTA_END   0xF2
#define CMD_ACK       0x21

// Buffer size
#define OTA_BUF_SIZE 300

extern UART_HandleTypeDef huart6;

// Externs
extern uint32_t Flash_GetActiveBank(void);
extern void Flash_Unlock(void);
extern void Flash_Lock(void);
extern uint8_t Flash_EraseSector(uint32_t sector);
extern uint8_t Flash_Write(uint32_t addr, uint8_t *data, uint32_t len);
extern void Flash_ToggleBank(void);
extern void Flash_CopyBootloader(uint32_t src_bank_start, uint32_t dst_bank_start);
extern uint32_t GetPhysicalSector(uint32_t Address);

// CRC8 Helper (Polynomial 0x31)
static uint8_t CalcCrc8(uint8_t *data, int len) {
    uint8_t crc = 0;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) crc = (crc << 1) ^ 0x31;
            else crc <<= 1;
        }
    }
    return crc;
}

static void SendAck(void) {
    uint8_t ack = CMD_ACK;
    HAL_UART_Transmit(&huart6, &ack, 1, 100);
}

void ProcessOTA(void) {
    // 0. Determine Targets
    // We ALWAYS write to 0x0810xxxx (which maps to Inactive Bank).
    // The GetPhysicalSector function handles the BFB2 logic.

    // However, if we are in Bank 2 (Active), 0x0810xxxx maps to Bank 1 (Inactive).
    // If we are in Bank 1 (Active), 0x0810xxxx maps to Bank 2 (Inactive).
    // So the TARGET ADDRESS is constant: 0x08100000.

    const uint32_t TARGET_BANK_START = 0x08100000;
    const uint32_t ACTIVE_BANK_START = 0x08000000;

    uint32_t target_app_addr = TARGET_BANK_START + 0x8000; // Offset 32KB
    uint32_t current_offset = 0;
    uint8_t buf[OTA_BUF_SIZE];

    printf("[OTA] Active Bank: %d\n", (int)Flash_GetActiveBank());

    // Drain
    while(HAL_UART_Receive(&huart6, buf, 1, 0) == HAL_OK);

    printf("[OTA] Waiting for Start...\n");

    // 1. Handshake
    while (1) {
        if (HAL_UART_Receive(&huart6, buf, 1, 1000) == HAL_OK) {
            if (buf[0] == CMD_OTA_START) {
                printf("[OTA] Start Command Received!\n");
                SendAck();
                break;
            }
        } else {
            HAL_GPIO_TogglePin(LED_BLUE_PORT, LED_BLUE_PIN);
        }
    }

    HAL_GPIO_WritePin(LED_BLUE_PORT, LED_BLUE_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_ORANGE_PORT, LED_ORANGE_PIN, GPIO_PIN_SET);

    // 2. Prepare Target (Unlock & Copy Bootloader)
    Flash_Unlock();
    Flash_CopyBootloader(ACTIVE_BANK_START, TARGET_BANK_START);

    printf("[OTA] Ready for Chunks.\n");

    // 3. Receive Loop
    uint32_t last_phys_sector = 0xFFFFFFFF;
    uint32_t total_bytes = 0;

    while (1) {
        // [CMD] [LEN] [PAYLOAD...] [CRC]
        if (HAL_UART_Receive(&huart6, buf, 2, 5000) != HAL_OK) {
            printf("[OTA] Timeout!\n");
            // Wait loop or Break? If broken pipe, better to reset.
            continue;
        }

        uint8_t cmd = buf[0];
        uint8_t len = buf[1];

        if (cmd == CMD_OTA_END) {
             printf("[OTA] End. Total: %d bytes\n", (int)total_bytes);
             SendAck();
             break;
        }

        // Read Payload + CRC
        if (HAL_UART_Receive(&huart6, &buf[2], len + 1, 1000) != HAL_OK) {
            printf("[OTA] Read Error\n");
            continue;
        }

        // Verify CRC (of Payload)
        // Assumption: ESP sends CRC of payload only.
        if (CalcCrc8(&buf[2], len) != buf[2+len]) {
             printf("[OTA] CRC Fail\n");
             continue;
        }

        uint32_t write_addr = target_app_addr + current_offset;

        // Erase if needed
        // Check Physical Sector of Start and End of this chunk
        uint32_t start_phys = GetPhysicalSector(write_addr);
        uint32_t end_phys = GetPhysicalSector(write_addr + len - 1);

        for (uint32_t s = start_phys; s <= end_phys; s++) {
            if (s != last_phys_sector) {
                printf("[OTA] Erasing Phys Sector %d\n", (int)s);
                HAL_GPIO_TogglePin(LED_ORANGE_PORT, LED_ORANGE_PIN);
                if (Flash_EraseSector(s) != HAL_OK) {
                    printf("[OTA] Erase Fail!\n");
                    Error_Handler();
                }
                last_phys_sector = s;
            }
        }

        HAL_GPIO_TogglePin(LED_BLUE_PORT, LED_BLUE_PIN);
        if (Flash_Write(write_addr, &buf[2], len) != HAL_OK) {
             printf("[OTA] Write Fail at %X\n", (unsigned int)write_addr);
             Error_Handler();
        }

        current_offset += len;
        total_bytes += len;
        SendAck();
    }

    Flash_Lock();

    // 4. Finalize
    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_SET);
    printf("[OTA] Success. Toggling Bank in 1s...\n");
    HAL_Delay(1000);

    Flash_ToggleBank();

    printf("[OTA] Resetting...\n");
    HAL_NVIC_SystemReset();
}
