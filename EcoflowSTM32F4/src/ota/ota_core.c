#include "ota_core.h"
#include "stm32f4xx_hal.h"
#include "uart_task.h"
#include <string.h>

// Current State
static bool _ota_active = false;
static uint32_t _total_size = 0;
static uint32_t _written_size = 0;
static uint32_t _target_crc = 0;
static uint32_t _current_crc = 0xFFFFFFFF;

// Bank Info
static uint32_t _target_addr = 0;
static uint32_t _target_config_addr = 0;

#define CMD_OTA_START 0xF0
#define CMD_OTA_DATA  0xF1
#define CMD_OTA_END   0xF2
#define CMD_OTA_ACK   0xF3
#define CMD_OTA_NACK  0xF4

static void SendAck(uint8_t cmd_id);
static void SendNack(uint8_t cmd_id);

void OTA_Init() {
    _ota_active = false;
}

bool OTA_IsActive() {
    return _ota_active;
}

uint8_t OTA_GetProgress() {
    if (_total_size == 0) return 0;
    return (_written_size * 100) / _total_size;
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        uint8_t b = data[i];
        for (int j = 0; j < 8; j++) {
            uint32_t bit = ((b >> (7 - j)) & 1) ^ ((crc >> 31) & 1);
            crc <<= 1;
            if (bit) crc ^= 0x04C11DB7;
        }
    }
    return crc;
}


// Helper to calculate CRC8 (Generic 1-Wire/Maxim)
// Same as used in protocol
static uint8_t calc_crc8(uint8_t *data, int len) {
    uint8_t crc = 0;
    for (int i = 0; i < len; i++) {
        uint8_t inbyte = data[i];
        for (uint8_t j = 0; j < 8; j++) {
            uint8_t mix = (crc ^ inbyte) & 0x01;
            crc >>= 1;
            if (mix) crc ^= 0x8C;
            inbyte >>= 1;
        }
    }
    return crc;
}

bool OTA_ProcessPacket(uint8_t* buffer, uint16_t len) {
    uint8_t cmd = buffer[1];

    if (cmd == CMD_OTA_START) {
        if (len < 12) return false;

        memcpy(&_total_size, &buffer[3], 4);
        memcpy(&_target_crc, &buffer[7], 4);

        // Detect Active Bank via Option Bytes
        FLASH_OBProgramInitTypeDef OBInit;
        HAL_FLASHEx_OBGetConfig(&OBInit);
        bool is_bank2_active = (OBInit.USERConfig & FLASH_OPTCR_BFB2);

        // Target is the INACTIVE bank.
        // Due to hardware remapping, the INACTIVE bank is always mapped to 0x0810xxxx.
        // So _target_addr is always APP_B_ADDR (0x08108000).
        _target_addr = APP_B_ADDR;
        _target_config_addr = CONFIG_B_ADDR;

        // Determine PHYSICAL sectors to erase.
        // If BFB2=0 (Active=Bank1), Inactive=Bank2 (Sectors 12-23).
        // If BFB2=1 (Active=Bank2), Inactive=Bank1 (Sectors 0-11).

        uint32_t bootSector, configSector, startAppSector, endAppSector;
        uint32_t bootAddrSrc = BOOTLOADER_A_ADDR; // Always copy from active bootloader at 0x08000000
        uint32_t bootAddrDst = BOOTLOADER_B_ADDR; // Always write to inactive bootloader at 0x08100000

        if (is_bank2_active) {
            // Running from Bank 2. Target is Bank 1 (Physically Sectors 0-11)
            // But wait, if remapping is active:
            // 0x0800xxxx accesses Bank 2.
            // 0x0810xxxx accesses Bank 1.
            // So if we erase Sector 0 via HAL, does HAL erase Physical Sector 0 (now at 0x0810xxxx) or Logical Sector 0 (0x0800xxxx)?
            // HAL_FLASHEx_Erase uses FLASH_CR SNB bits.
            // On F469, SNB bits select physical sectors?
            // "When the dual bank boot is enabled... addressing the Flash memory... 0x08000000 is aliased to Bank 2".
            // "Sector Erase... The sector to erase is selected using the SNB bits".
            // SNB 0-11 select sectors in Bank 1. SNB 12-23 select sectors in Bank 2.
            // The Mapping:
            // If BFB2=1: Bank 2 is at 0x0800. Bank 1 is at 0x0810.
            // We want to erase the INACTIVE bank (Bank 1, at 0x0810).
            // This corresponds to SECTORS 0-11 physically?
            // Yes, Bank 1 is Sectors 0-11.
            // So if we pass FLASH_SECTOR_0 to HAL, it erases Bank 1.
            // Since Bank 1 is mapped to 0x0810xxxx (inactive), this is SAFE.
            // If we pass FLASH_SECTOR_12, it erases Bank 2 (Active). THIS IS BAD.

            bootSector = FLASH_SECTOR_0;
            configSector = FLASH_SECTOR_1;
            startAppSector = FLASH_SECTOR_2;
            endAppSector = FLASH_SECTOR_11;
        } else {
            // Running from Bank 1. Target is Bank 2 (Physically Sectors 12-23).
            bootSector = FLASH_SECTOR_12;
            configSector = FLASH_SECTOR_13;
            startAppSector = FLASH_SECTOR_14;
            endAppSector = FLASH_SECTOR_23;
        }

        HAL_FLASH_Unlock();

        // 1. Erase Bootloader Sector
        FLASH_EraseInitTypeDef EraseInitStruct;
        EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
        EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
        EraseInitStruct.Sector = bootSector;
        EraseInitStruct.NbSectors = 1;
        uint32_t SectorError = 0;

        extern IWDG_HandleTypeDef hiwdg;
        __HAL_IWDG_RELOAD_COUNTER(&hiwdg);

        if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
             HAL_FLASH_Lock(); SendNack(cmd); return true;
        }

        // 2. Copy Bootloader (Always from 0x08000000 to 0x08100000 logic space)
        for (uint32_t i = 0; i < 0x4000; i+=4) { // 16KB
            uint32_t data = *(__IO uint32_t*)(bootAddrSrc + i);
            if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, bootAddrDst + i, data) != HAL_OK) {
                 HAL_FLASH_Lock(); SendNack(cmd); return true;
            }
        }

        // 3. Erase Config Sector
        EraseInitStruct.Sector = configSector;
        __HAL_IWDG_RELOAD_COUNTER(&hiwdg);
        if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
             HAL_FLASH_Lock(); SendNack(cmd); return true;
        }

        // 4. Erase App Sectors
        EraseInitStruct.NbSectors = 1;
        for (uint32_t i = startAppSector; i <= endAppSector; i++) {
             __HAL_IWDG_RELOAD_COUNTER(&hiwdg);
             EraseInitStruct.Sector = i;
             if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
                 HAL_FLASH_Lock(); SendNack(cmd); return true;
             }
        }

        HAL_FLASH_Lock();

        _written_size = 0;
        _current_crc = 0xFFFFFFFF;
        _ota_active = true;

        SendAck(cmd);
        return true;
    }
    else if (cmd == CMD_OTA_DATA) {
        if (!_ota_active) { SendNack(cmd); return true; }

        uint32_t offset;
        memcpy(&offset, &buffer[3], 4);
        uint8_t* data = &buffer[7];
        uint8_t payload_len = buffer[2];
        if (payload_len < 4) return true;
        uint16_t chunk_len = payload_len - 4;

        HAL_FLASH_Unlock();
        for (int i=0; i < chunk_len; i+=4) {
             uint32_t word = 0;
             memcpy(&word, &data[i], (chunk_len - i < 4) ? (chunk_len - i) : 4);
             if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, _target_addr + offset + i, word) != HAL_OK) {
                 HAL_FLASH_Lock(); SendNack(cmd); return true;
             }
        }
        HAL_FLASH_Lock();

        _current_crc = crc32_update(_current_crc, data, chunk_len);
        _written_size += chunk_len;

        SendAck(cmd);
        return true;
    }
    else if (cmd == CMD_OTA_END) {
        if (!_ota_active) { SendNack(cmd); return true; }

        uint32_t received_crc;
        memcpy(&received_crc, &buffer[3], 4);

        if (_current_crc == received_crc) {
             if (_current_crc != _target_crc) { SendNack(cmd); return true; }

             BootConfig cfg;
             cfg.magic = CONFIG_MAGIC;
             cfg.update_pending = 1;
             // We are always writing to B, so if current is A (BFB2=0), target is 1.
             // If current is B (BFB2=1), target is 0?
             // No, BFB2 toggle swaps.
             // If we write to 0x0810xxxx, we are writing to the 'other' bank.
             // To boot from it, we just need to toggle BFB2.
             // The bootloader logic: "cfg.active_bank = (cfg.active_bank == 0) ? 1 : 0;"
             // So we just need to set update_pending.
             // We should set active_bank to WHAT IT IS NOW, so bootloader knows to swap.
             // Or bootloader checks update_pending and just swaps?
             // My bootloader logic: "cfg.active_bank = (cfg.active_bank == 0) ? 1 : 0; ToggleBank();"
             // So we need to store CURRENT active bank.

             FLASH_OBProgramInitTypeDef OBInit;
             HAL_FLASHEx_OBGetConfig(&OBInit);
             bool is_bank2_active = (OBInit.USERConfig & FLASH_OPTCR_BFB2);
             cfg.active_bank = is_bank2_active ? 1 : 0;

             cfg.image_size = _written_size;
             cfg.image_crc = _current_crc;

             HAL_FLASH_Unlock();
             // Config sector already erased in START
             uint32_t addr = _target_config_addr;
             uint32_t* p = (uint32_t*)&cfg;
             for (int i = 0; i < sizeof(BootConfig)/4; i++) {
                 HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i*4, p[i]);
             }
             HAL_FLASH_Lock();

             SendAck(cmd);
             HAL_Delay(100);
             HAL_NVIC_SystemReset();
        } else {
             SendNack(cmd);
        }
        return true;
    }

    return false;
}

static void SendAck(uint8_t cmd_id) {
    uint8_t packet[5];
    packet[0] = 0xAA;
    packet[1] = CMD_OTA_ACK;
    packet[2] = 1;
    packet[3] = cmd_id;
    packet[4] = calc_crc8(&packet[1], 3);
    extern UART_HandleTypeDef huart6;
    HAL_UART_Transmit(&huart6, packet, 5, 100);
}

static void SendNack(uint8_t cmd_id) {
    uint8_t packet[5];
    packet[0] = 0xAA;
    packet[1] = CMD_OTA_NACK;
    packet[2] = 1;
    packet[3] = cmd_id;
    packet[4] = calc_crc8(&packet[1], 3);
    extern UART_HandleTypeDef huart6;
    HAL_UART_Transmit(&huart6, packet, 5, 100);
}
