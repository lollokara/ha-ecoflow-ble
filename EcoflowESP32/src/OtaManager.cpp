#include "OtaManager.h"
#include "Stm32Serial.h"
#include <Update.h>
#include <esp_log.h>

static const char* TAG = "OtaManager";

// STM32 UART Protocol Constants
#define CMD_OTA_START   0xF0
#define CMD_OTA_DATA    0xF1
#define CMD_OTA_END     0xF2

OtaState OtaManager::_state = OTA_IDLE;
int OtaManager::_progress = 0;
String OtaManager::_error = "";
uint8_t* OtaManager::_fwBuffer = NULL;
size_t OtaManager::_fwSize = 0;

// Helper to calculate STM32 Hardware Compatible CRC32
static uint32_t stm32_crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFF;
    size_t words = len / 4;
    for (size_t i = 0; i < words; i++) {
        uint32_t val = 0;
        val |= data[i * 4 + 0] << 0;
        val |= data[i * 4 + 1] << 8;
        val |= data[i * 4 + 2] << 16;
        val |= data[i * 4 + 3] << 24;
        crc ^= val;
        for (int j = 0; j < 32; j++) {
            if (crc & 0x80000000) crc = (crc << 1) ^ 0x04C11DB7;
            else crc <<= 1;
        }
    }
    return crc;
}

OtaState OtaManager::getState() { return _state; }
int OtaManager::getProgress() { return _progress; }
String OtaManager::getError() { return _error; }

void OtaManager::cleanup() {
    if (_fwBuffer) {
        free(_fwBuffer);
        _fwBuffer = NULL;
    }
    _fwSize = 0;
    _state = OTA_IDLE;
}

void OtaManager::startUpdateSTM32(uint8_t* buffer, size_t size) {
    if (_state != OTA_IDLE && _state != OTA_COMPLETE && _state != OTA_ERROR) return;

    _fwBuffer = buffer;
    _fwSize = size;
    _state = OTA_BUFFERING;
    _progress = 0;
    _error = "";

    xTaskCreate(otaTask, "OtaTask", 4096, NULL, 1, NULL);
}

void OtaManager::otaTask(void* param) {
    ESP_LOGI(TAG, "Starting STM32 Update Task. Size: %d", _fwSize);

    uint32_t checksum = stm32_crc32(_fwBuffer, _fwSize);
    ESP_LOGI(TAG, "Checksum: 0x%08X", checksum);

    // Helper to send packet
    auto sendPacket = [](uint8_t cmd, uint8_t* data, size_t len) {
        Serial1.write(0xAA);
        Serial1.write(cmd);
        Serial1.write((uint8_t)len);
        if (len > 0 && data) Serial1.write(data, len);

        // CRC8
        uint8_t c = 0;
        c ^= cmd;
        for(int j=0; j<8; j++) if(c & 0x80) c = (c << 1) ^ 0x07; else c <<= 1;
        c ^= (uint8_t)len;
        for(int j=0; j<8; j++) if(c & 0x80) c = (c << 1) ^ 0x07; else c <<= 1;
        for(size_t i=0; i<len; i++) {
            c ^= data[i];
            for(int j=0; j<8; j++) if(c & 0x80) c = (c << 1) ^ 0x07; else c <<= 1;
        }
        Serial1.write(c);
        vTaskDelay(pdMS_TO_TICKS(20)); // Gentle throttle
    };

    // 1. Send Start
    _state = OTA_ERASING;
    uint8_t payload[8];
    memcpy(payload, &_fwSize, 4);
    memcpy(payload + 4, &checksum, 4);
    sendPacket(CMD_OTA_START, payload, 8);

    // 2. Wait for Erase (60s for safety - Full Bank Erase on F4 can take 20s+)
    ESP_LOGI(TAG, "Waiting 60s for erase...");
    for(int i=0; i<600; i++) {
        _progress = i / 60; // 0-10% progress during erase
        vTaskDelay(pdMS_TO_TICKS(100));
    }

    // 3. Send Data
    _state = OTA_FLASHING;
    size_t offset = 0;
    size_t total = _fwSize;

    while (offset < total) {
        size_t chunk = 128;
        if (offset + chunk > total) chunk = total - offset;

        sendPacket(CMD_OTA_DATA, _fwBuffer + offset, chunk);
        offset += chunk;

        _progress = 10 + (int)((offset * 90) / total); // 10-100%

        if (offset % 2048 == 0) vTaskDelay(pdMS_TO_TICKS(10)); // Yield
    }

    // 4. End
    sendPacket(CMD_OTA_END, NULL, 0);
    _state = OTA_COMPLETE;
    _progress = 100;
    ESP_LOGI(TAG, "STM32 Update Complete");

    cleanup();
    vTaskDelete(NULL);
}

bool OtaManager::updateESP32(Stream& firmware, size_t size) {
    if (!Update.begin(size, U_FLASH)) {
        ESP_LOGE(TAG, "Update Begin Failed");
        return false;
    }

    size_t written = Update.writeStream(firmware);
    if (written != size) {
        ESP_LOGE(TAG, "Write mismatch");
        return false;
    }

    if (!Update.end()) {
        ESP_LOGE(TAG, "Update End Failed");
        return false;
    }

    return true;
}
