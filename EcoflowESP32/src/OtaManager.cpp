#include "OtaManager.h"
#include "ecoflow_protocol.h"

// Define constants from protocol if not available
#ifndef CMD_OTA_START
#define CMD_OTA_START       0xF0
#define CMD_OTA_CHUNK       0xF1
#define CMD_OTA_END         0xF2
#define CMD_OTA_ACK         0xAA
#define CMD_OTA_NACK        0x55
#endif

extern Stm32Serial stm32Serial;

OtaManager& OtaManager::getInstance() {
    static OtaManager instance;
    return instance;
}

OtaManager::OtaManager() {}

void OtaManager::begin() {
    LittleFS.begin(true);
    // Create background task for OTA management
    xTaskCreatePinnedToCore(
        [](void* param) {
            static_cast<OtaManager*>(param)->task();
        },
        "OtaTask",
        4096,
        this,
        1, // Priority
        NULL,
        1 // Core 1 (same as loop)
    );
}

void OtaManager::task() {
    while(1) {
        processStm32Update();
        vTaskDelay(pdMS_TO_TICKS(10)); // 10ms loop
    }
}

// Ensure loop() is kept for API compatibility but implementation is in task()
void OtaManager::loop() {
    // No-op if task is running
}

bool OtaManager::startStm32Update(const String& path) {
    if (_stm32State != OTA_IDLE && _stm32State != OTA_ERROR && _stm32State != OTA_DONE) {
        return false;
    }

    if (!LittleFS.exists(path)) {
        _errorMsg = "File not found";
        return false;
    }

    _stm32File = LittleFS.open(path, "r");
    if (!_stm32File) {
        _errorMsg = "Failed to open file";
        return false;
    }

    _totalSize = _stm32File.size();
    _sentBytes = 0;
    _progress = 0;
    _retryCount = 0;
    _stm32State = OTA_STARTING;
    _lastActivity = millis();
    _errorMsg = "";

    Serial.println("OTA: Starting STM32 Update...");
    return true;
}

void OtaManager::processStm32Update() {
    unsigned long now = millis();

    switch (_stm32State) {
        case OTA_STARTING:
            // Send Handshake every 200ms
            if (now - _lastActivity > 200) {
                _lastActivity = now;

                // [CMD][SIZE...]
                // The Bootloader implementation handles:
                // if (HAL_UART_Receive(&huart6, rx_buf, 5, 500) == HAL_OK)
                // if (rx_buf[0] == CMD_OTA_START)
                // So it expects: [CMD] [S1] [S2] [S3] [S4].

                uint8_t payload[4];
                payload[0] = (_totalSize >> 24) & 0xFF;
                payload[1] = (_totalSize >> 16) & 0xFF;
                payload[2] = (_totalSize >> 8) & 0xFF;
                payload[3] = (_totalSize) & 0xFF;

                uint8_t packet[5];
                packet[0] = CMD_OTA_START;
                memcpy(&packet[1], payload, 4);

                Stm32Serial::getInstance().sendRaw(packet, 5);

                _retryCount++;
                if (_retryCount > 300) { // 60 seconds
                    _stm32State = OTA_ERROR;
                    _errorMsg = "Handshake Timeout";
                    _stm32File.close();
                }
            }
            break;

        case OTA_SENDING:
            // Handled by ACK trigger, but check timeout
            if (now - _lastActivity > 5000) {
                _stm32State = OTA_ERROR;
                _errorMsg = "Write Timeout";
                _stm32File.close();
            }
            break;

        case OTA_VERIFYING:
             if (now - _lastActivity > 60000) { // Increased to 60s for slow flash
                _stm32State = OTA_ERROR;
                _errorMsg = "Verify Timeout";
                _stm32File.close();
            }
            break;

        default:
            break;
    }
}

void OtaManager::sendChunk() {
    if (!_stm32File) return;

    if (_sentBytes >= _totalSize) {
        // Send End
        uint8_t packet[1];
        packet[0] = CMD_OTA_END;
        Stm32Serial::getInstance().sendRaw(packet, 1);
        _stm32State = OTA_VERIFYING;
        _lastActivity = millis();
        return;
    }

    // Read Chunk
    uint8_t buffer[240]; // Reduced from 256 to allow header overhead in buffer if needed, and safer UART
    size_t toRead = 240;
    if (_totalSize - _sentBytes < 240) {
        toRead = _totalSize - _sentBytes;
    }

    _stm32File.read(buffer, toRead);

    // Packet: [CMD 0xF1] [LEN_H] [LEN_L] [DATA...] [CRC]
    // Bootloader expects: [CMD] [LEN_H] [LEN_L] [DATA...] [CRC]
    uint8_t header[3];
    header[0] = CMD_OTA_CHUNK;
    header[1] = (toRead >> 8) & 0xFF;
    header[2] = toRead & 0xFF;

    uint8_t crc = calculateCRC8(buffer, toRead);

    // Send Header
    Stm32Serial::getInstance().sendRaw(header, 3);
    // Send Data
    Stm32Serial::getInstance().sendRaw(buffer, toRead);
    // Send CRC
    Stm32Serial::getInstance().sendRaw(&crc, 1);

    _sentBytes += toRead;
    _progress = (_sentBytes * 100) / _totalSize;
    _lastActivity = millis();

    // Log every 10%
    static int lastLog = -1;
    if (_progress / 10 != lastLog) {
        Serial.printf("OTA: %d%%\n", _progress);
        lastLog = _progress / 10;
    }
}

void OtaManager::handleUartData(uint8_t* data, size_t len) {
    // Check for ACK
    // Bootloader sends single byte ACK (0xAA)
    if (len > 0) {
        uint8_t cmd = data[0];
        if (cmd == CMD_OTA_ACK) {
            if (_stm32State == OTA_STARTING) {
                Serial.println("\nOTA: Handshake ACK! Sending data...");
                _stm32State = OTA_SENDING;
                _sentBytes = 0;
                sendChunk(); // Start sending
            } else if (_stm32State == OTA_SENDING) {
                // Next Chunk
                sendChunk();
            } else if (_stm32State == OTA_VERIFYING) {
                Serial.println("OTA: Success!");
                _stm32State = OTA_DONE;
                _stm32File.close();
            }
        } else if (cmd == CMD_OTA_NACK) {
            if (_stm32State == OTA_SENDING) {
                _stm32State = OTA_ERROR;
                _errorMsg = "Received NACK during write";
                _stm32File.close();
            }
        }
    }
}

uint8_t OtaManager::calculateCRC8(const uint8_t *data, size_t len) {
    uint8_t crc = 0x00;
    while (len--) {
        crc ^= *data++;
        for (uint8_t i = 0; i < 8; i++) {
            if (crc & 0x80) crc = (crc << 1) ^ 0x31;
            else crc <<= 1;
        }
    }
    return crc;
}
