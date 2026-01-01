#include "stm32f4xx_hal.h"
#include "ecoflow_protocol.h"
#include <string.h>
#include <stdio.h>

// External IWDG Handle to refresh watchdog during erase
extern IWDG_HandleTypeDef hiwdg;
extern UART_HandleTypeDef huart6;

// Bank Definition
#define FLASH_BANK1_START 0x08000000
#define FLASH_BANK2_START 0x08100000
#define FLASH_BANK_SIZE   0x00100000 // 1MB

// State Machine
static uint8_t ota_active = 0;
static uint32_t ota_total_size = 0;
static uint32_t ota_received_size = 0;
static uint32_t ota_target_addr = 0;

// Helper to send ACK
static void OTA_SendAck(uint8_t success) {
    uint8_t buffer[4];
    // Manual pack to avoid dependency issues if not linked
    buffer[0] = PROTOCOL_START_BYTE;
    buffer[1] = success ? PROTOCOL_CMD_OTA_ACK : PROTOCOL_CMD_OTA_NACK;
    buffer[2] = 0;
    // Simple CRC: cmd is byte 1.
    // If calculate_crc8 is available:
    extern uint8_t calculate_crc8(const uint8_t *data, uint8_t len);
    buffer[3] = calculate_crc8(&buffer[1], 2);

    HAL_UART_Transmit(&huart6, buffer, 4, 100);
}

void OTA_Start(uint32_t size) {
    printf("OTA: Start request size=%lu\n", size);
    if(size > FLASH_BANK_SIZE) {
        printf("OTA: Size too large!\n");
        OTA_SendAck(0);
        return;
    }

    ota_active = 1;
    ota_total_size = size;
    ota_received_size = 0;

    // Determine Bank
    uint32_t pc = (uint32_t)OTA_Start;
    uint32_t start_sector = 0;

    if (pc < 0x08100000) {
        // Bank 1 -> Target Bank 2
        ota_target_addr = FLASH_BANK2_START;
        start_sector = FLASH_SECTOR_12;
        printf("OTA: Target Bank 2 (0x%08lX)\n", ota_target_addr);
    } else {
        // Bank 2 -> Target Bank 1
        ota_target_addr = FLASH_BANK1_START;
        start_sector = FLASH_SECTOR_0;
        printf("OTA: Target Bank 1 (0x%08lX)\n", ota_target_addr);
    }

    HAL_FLASH_Unlock();

    // Erase 12 Sectors one by one to refresh IWDG
    FLASH_EraseInitTypeDef EraseInitStruct;
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    EraseInitStruct.NbSectors = 1;

    uint32_t SectorError = 0;

    for(int i=0; i<12; i++) {
        EraseInitStruct.Sector = start_sector + i;
        printf("OTA: Erasing Sector %lu...\n", EraseInitStruct.Sector);

        // Refresh IWDG before blocking call
        HAL_IWDG_Refresh(&hiwdg);

        if(HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
            printf("OTA: Erase Failed! Sector: %lu\n", SectorError);
            ota_active = 0;
            HAL_FLASH_Lock();
            OTA_SendAck(0);
            return;
        }

        HAL_IWDG_Refresh(&hiwdg);
    }

    printf("OTA: Erase Complete. Sending ACK.\n");
    OTA_SendAck(1); // ACK Success
}

void OTA_WriteChunk(uint8_t* payload, uint32_t len) {
    if (!ota_active) return;
    if (len < 4) return;

    uint32_t offset = payload[0] | (payload[1] << 8) | (payload[2] << 16) | (payload[3] << 24);
    uint8_t* data = &payload[4];
    uint32_t data_len = len - 4;

    if (offset >= FLASH_BANK_SIZE) return;

    for (uint32_t i = 0; i < data_len; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, ota_target_addr + offset + i, data[i]) != HAL_OK) {
             printf("OTA: Write Error at offset %lu\n", offset + i);
             ota_active = 0;
             HAL_FLASH_Lock();
             return;
        }
    }

    if ((offset + data_len) > ota_received_size) {
        ota_received_size = offset + data_len;
    }
}

void OTA_End() {
    if (!ota_active) return;

    printf("OTA: Transfer Complete. Verifying...\n");
    HAL_FLASH_Lock();
    ota_active = 0;

    if (ota_received_size != ota_total_size) {
        printf("OTA: Size mismatch! Rx=%lu, Exp=%lu\n", ota_received_size, ota_total_size);
        OTA_SendAck(0);
        return;
    }

    printf("OTA: Swapping Banks...\n");
    HAL_FLASH_Unlock();
    HAL_FLASH_OB_Unlock();

    uint32_t optcr = FLASH->OPTCR;
    if (optcr & (1 << 23)) { // BFB2
        optcr &= ~(1 << 23);
    } else {
        optcr |= (1 << 23);
    }

    FLASH->OPTCR = optcr;
    HAL_FLASH_OB_Launch();
    NVIC_SystemReset();
}

void OTA_ProcessCommand(uint8_t cmd, uint8_t* payload, uint32_t len) {
    if (cmd == PROTOCOL_CMD_OTA_START) {
        if(len >= 4) {
            uint32_t size = *(uint32_t*)payload;
            OTA_Start(size);
        }
    }
    else if (cmd == PROTOCOL_CMD_OTA_CHUNK) {
        OTA_WriteChunk(payload, len);
    }
    else if (cmd == PROTOCOL_CMD_OTA_END) {
        OTA_End();
    }
}
