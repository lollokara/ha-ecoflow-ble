#include "OtaManager.h"
#include "Stm32Serial.h"
#include "ecoflow_protocol.h"
#include <esp_log.h>

static const char* TAG = "OtaManager";

static uint32_t calculate_crc32(const uint8_t *data, size_t len, uint32_t current_crc) {
    for (size_t i = 0; i < len; i++) {
        uint32_t byte = data[i];
        current_crc = current_crc ^ byte;
        for (int j = 0; j < 8; j++) {
            if (current_crc & 1)
                current_crc = (current_crc >> 1) ^ 0xEDB88320;
            else
                current_crc = current_crc >> 1;
        }
    }
    return current_crc;
}

OtaManager::OtaManager() : _isUpdating(false), _state(IDLE) {}

void OtaManager::begin() {
    LittleFS.begin(true);
}

void OtaManager::startStm32Update(const String& filename) {
    if (_isUpdating) return;

    _filename = filename;
    _file = LittleFS.open(filename, "r");
    if (!_file) {
        ESP_LOGE(TAG, "Failed to open firmware file: %s", filename.c_str());
        return;
    }

    _fileSize = _file.size();
    _globalCRC = calculateFileCRC();
    ESP_LOGI(TAG, "Starting OTA. Size: %d, CRC32: %08X", _fileSize, _globalCRC);

    _sentBytes = 0;
    _retryCount = 0;
    _state = STARTING;
    _isUpdating = true;
    _lastPacketTime = 0; // Trigger immediate send
}

void OtaManager::update() {
    if (!_isUpdating) return;

    uint32_t now = millis();

    if (_state == STARTING) {
        if (now - _lastPacketTime > 1000) {
            _lastPacketTime = now;
            _retryCount++;
            if (_retryCount > 60) { // 60 seconds to allow STM32 erase
                 ESP_LOGE(TAG, "OTA Start Timeout");
                 _isUpdating = false;
                 _file.close();
                 return;
            }
            ESP_LOGI(TAG, "Sending OTA Start (Attempt %d)", _retryCount);

            // Payload: Size (4 bytes)
            uint8_t payload[4];
            payload[0] = (_fileSize >> 0) & 0xFF;
            payload[1] = (_fileSize >> 8) & 0xFF;
            payload[2] = (_fileSize >> 16) & 0xFF;
            payload[3] = (_fileSize >> 24) & 0xFF;

            uint8_t packet[32];
            packet[0] = START_BYTE;
            packet[1] = CMD_OTA_START;
            packet[2] = 4; // Length
            memcpy(&packet[3], payload, 4);
            packet[7] = calculate_crc8(&packet[1], 6); // CRC of CMD+LEN+PAYLOAD

            Stm32Serial::getInstance().sendRaw(packet, 8);
        }
    }
    else if (_state == SENDING) {
        if (now - _lastPacketTime > 2000) {
            ESP_LOGW(TAG, "Chunk Timeout. Retrying...");
            sendChunk();
            _lastPacketTime = now; // Reset timer to prevent flood
        }
    }
    else if (_state == ENDING) {
        if (now - _lastPacketTime > 1000) {
            _lastPacketTime = now;
            ESP_LOGI(TAG, "Sending OTA End with CRC %08X", _globalCRC);

            // Payload: CRC32 (4 bytes)
            uint8_t payload[4];
            payload[0] = (_globalCRC >> 0) & 0xFF;
            payload[1] = (_globalCRC >> 8) & 0xFF;
            payload[2] = (_globalCRC >> 16) & 0xFF;
            payload[3] = (_globalCRC >> 24) & 0xFF;

            uint8_t packet[16];
            packet[0] = START_BYTE;
            packet[1] = CMD_OTA_END;
            packet[2] = 4;
            memcpy(&packet[3], payload, 4);
            packet[7] = calculate_crc8(&packet[1], 6);

            Stm32Serial::getInstance().sendRaw(packet, 8);
        }
    }
    else if (_state == APPLYING) {
         if (now - _lastPacketTime > 1000) {
             ESP_LOGI(TAG, "Sending OTA Apply");
             uint8_t packet[8];
            packet[0] = START_BYTE;
            packet[1] = CMD_OTA_APPLY;
            packet[2] = 0;
            packet[3] = calculate_crc8(&packet[1], 2);
            Stm32Serial::getInstance().sendRaw(packet, 4);

            _isUpdating = false;
            _file.close();
            ESP_LOGI(TAG, "OTA Sequence Finished.");
         }
    }
}

void OtaManager::sendChunk() {
    if (!_file) return;

    _file.seek(_sentBytes);
    uint8_t buf[240];
    int n = _file.read(buf, sizeof(buf));

    if (n > 0) {
        uint8_t header[3];
        header[0] = START_BYTE;
        header[1] = CMD_OTA_CHUNK;
        header[2] = n;

        Stm32Serial::getInstance().sendRaw(header, 3);
        Stm32Serial::getInstance().sendRaw(buf, n);

        uint8_t crcBuf[245];
        crcBuf[0] = CMD_OTA_CHUNK;
        crcBuf[1] = n;
        memcpy(&crcBuf[2], buf, n);
        uint8_t crc = calculate_crc8(crcBuf, n + 2);

        Stm32Serial::getInstance().sendRaw(&crc, 1);

        _lastPacketTime = millis();
    }
}

uint32_t OtaManager::calculateFileCRC() {
    if (!_file) return 0;
    size_t cur = _file.position();
    _file.seek(0);
    uint32_t crc = 0xFFFFFFFF;
    uint8_t buf[256];
    while (_file.available()) {
        int n = _file.read(buf, sizeof(buf));
        crc = calculate_crc32(buf, n, crc);
    }
    _file.seek(cur);
    return crc ^ 0xFFFFFFFF;
}

void OtaManager::handlePacket(uint8_t cmd, uint8_t* payload, uint8_t len) {
    if (!_isUpdating) return;

    if (cmd == CMD_OTA_ACK) {
        _retryCount = 0;
        if (_state == STARTING) {
            ESP_LOGI(TAG, "OTA Start ACK. Sending Data...");
            _state = SENDING;
            sendChunk();
        } else if (_state == SENDING) {
            int chunkSize = 240;
            if (_fileSize - _sentBytes < 240) chunkSize = _fileSize - _sentBytes;

            _sentBytes += chunkSize;
            // Log every 10%
            if ((_sentBytes * 100 / _fileSize) > ((_sentBytes - chunkSize) * 100 / _fileSize)) {
                ESP_LOGI(TAG, "Progress: %d%% (%d/%d)", (int)(_sentBytes * 100 / _fileSize), _sentBytes, _fileSize);
            }

            if (_sentBytes >= _fileSize) {
                _state = ENDING;
                _lastPacketTime = 0;
            } else {
                sendChunk();
            }
        } else if (_state == ENDING) {
            ESP_LOGI(TAG, "OTA End ACK. Applying...");
            _state = APPLYING;
            _lastPacketTime = 0;
        }
    } else if (cmd == CMD_OTA_NACK) {
        ESP_LOGE(TAG, "OTA NACK received in state %d", _state);
        if (_state == SENDING) {
             sendChunk();
        }
    }
}
