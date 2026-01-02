#include "OtaManager.h"
#include <Arduino.h>
#include <Update.h>
#include <LittleFS.h>
#include "Stm32Serial.h"

// Define STM32 UART Protocol Constants
#define CMD_OTA_START 0xA0
#define CMD_OTA_CHUNK 0xA1
#define CMD_OTA_END   0xA2
#define CMD_OTA_APPLY 0xA3

OtaManager::OtaManager(Stm32Serial* stm32) : _stm32(stm32), _state(OTA_IDLE) {}

void OtaManager::begin() {
    // LittleFS must be started by main setup
}

void OtaManager::handle() {
    if (_state == OTA_IDLE) return;

    if (_state == OTA_STARTING) {
        if (millis() - _lastChunkTime > 1000) {
            if (_retryCount < 60) { // Try for 60 seconds (STM32 Reboot + Erase time)
                log_i("Sending OTA START...");
                _stm32->sendOtaStart(_totalSize);
                _lastChunkTime = millis();
                _retryCount++;
            } else {
                log_e("OTA Start Timeout");
                _state = OTA_ERROR;
            }
        }
    }
    else if (_state == OTA_SENDING) {
        // Send next chunk if ACK received (handled in onAck)
        // Or handle timeout
        if (millis() - _lastChunkTime > 2000) { // 2s timeout per chunk
             if (_retryCount < 3) {
                 log_w("Chunk Timeout at %d, retrying...", _bytesSent);
                 sendChunk();
                 _retryCount++;
                 _lastChunkTime = millis();
             } else {
                 log_e("OTA Chunk Retry limit reached");
                 _state = OTA_ERROR;
             }
        }
    }
    else if (_state == OTA_WAIT_APPLY) {
         if (millis() - _lastChunkTime > 5000) {
              log_e("OTA Apply Timeout");
              _state = OTA_ERROR;
         }
    }
}

void OtaManager::startStm32Update(const char* filepath) {
    if (!LittleFS.exists(filepath)) {
        log_e("File not found: %s", filepath);
        return;
    }
    _file = LittleFS.open(filepath, "r");
    _totalSize = _file.size();
    _bytesSent = 0;
    _state = OTA_STARTING;
    _retryCount = 0;
    _lastChunkTime = 0;
    log_i("Starting STM32 OTA. File size: %d bytes", _totalSize);
}

void OtaManager::sendChunk() {
    if (!_file) return;

    _file.seek(_bytesSent);
    uint8_t buf[240]; // 256 byte payload max in protocol
    int len = _file.read(buf, sizeof(buf));

    if (len > 0) {
        _stm32->sendOtaChunk(_bytesSent, buf, len);
        // Log every 10%
        if ((_bytesSent % (_totalSize / 10)) < 240) {
             log_i("OTA Progress: %d%% (%d/%d)", (_bytesSent * 100) / _totalSize, _bytesSent, _totalSize);
        }
    }
}

void OtaManager::onAck(uint8_t cmd) {
    if (_state == OTA_STARTING && cmd == CMD_OTA_START) {
        log_i("OTA START ACK Received. Sending Chunks...");
        _state = OTA_SENDING;
        _bytesSent = 0;
        _retryCount = 0;
        sendChunk();
        _lastChunkTime = millis();
    }
    else if (_state == OTA_SENDING && cmd == CMD_OTA_CHUNK) {
        _bytesSent += 240; // Approx logic, assuming full chunks
        if (_bytesSent >= _totalSize) {
             log_i("All Chunks Sent. Sending OTA END...");
             _stm32->sendOtaEnd();
             _state = OTA_ENDING;
        } else {
             _retryCount = 0;
             sendChunk();
             _lastChunkTime = millis();
        }
    }
    else if (_state == OTA_ENDING && cmd == CMD_OTA_END) {
        log_i("OTA END ACK Received. Sending APPLY...");
        _stm32->sendOtaApply();
        _state = OTA_WAIT_APPLY;
        _lastChunkTime = millis();
    }
    else if (_state == OTA_WAIT_APPLY && cmd == CMD_OTA_APPLY) {
        log_i("OTA Complete! STM32 should be rebooting.");
        _state = OTA_IDLE;
        _file.close();
    }
}

void OtaManager::onNack(uint8_t cmd) {
    log_e("OTA NACK Received for CMD: 0x%02X", cmd);
    if (_state == OTA_STARTING) {
        // Maybe still erasing? Keep trying if retries allow
    } else {
        _state = OTA_ERROR;
        _file.close();
    }
}

void OtaManager::processStoredUpdate() {
    // Check if there is a pending update file on LittleFS
    if (LittleFS.exists("/firmware.bin")) {
         // This is for ESP32 Self Update usually
         // ...
    }
}
