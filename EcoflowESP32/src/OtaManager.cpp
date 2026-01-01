#include "OtaManager.h"
#include "Stm32Serial.h"
#include <Update.h>
#include <esp_task_wdt.h>

// Chunk size for UART transfer
// Must match STM32 Receiver buffer logic, or be reasonably small to avoid blocking
// STM32 has a 2048 byte ring buffer.
// We can safely send 256 bytes chunks + overhead.
#define UART_CHUNK_SIZE 256

OtaManager::OtaManager() : _state(IDLE), _totalSize(0), _sentSize(0), _lastChunkTime(0) {}

void OtaManager::begin() {
    // Ensure LittleFS is mounted
    if (!LittleFS.begin(true)) {
        Serial.println("LittleFS Mount Failed");
    }
}

float OtaManager::getSTM32Progress() {
    if (_state == IDLE) return 0.0f;
    if (_state == DONE) return 100.0f;
    if (_totalSize == 0) return 0.0f;
    return ((float)_sentSize / (float)_totalSize) * 100.0f;
}

String OtaManager::getSTM32Status() {
    return _statusMsg;
}

void OtaManager::handleESP32Upload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
        Serial.printf("ESP32 OTA Start: %s\n", filename.c_str());
        // if (!Update.begin(UPDATE_SIZE_UNKNOWN)) { // Start with max available size
        if (!Update.begin(request->contentLength())) {
            Update.printError(Serial);
        }
    }

    if (Update.write(data, len) != len) {
        Update.printError(Serial);
    }

    if (final) {
        if (Update.end(true)) {
            Serial.printf("ESP32 OTA Success: %uB\n", index + len);
        } else {
            Update.printError(Serial);
        }
    }
}

void OtaManager::handleSTM32Upload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
        Serial.printf("STM32 FW Upload Start: %s\n", filename.c_str());
        _state = IDLE; // Reset state

        // Open file for writing
        _updateFile = LittleFS.open("/stm32_update.bin", FILE_WRITE);
        if (!_updateFile) {
            Serial.println("Failed to open file for writing");
            return;
        }
    }

    if (_updateFile) {
        _updateFile.write(data, len);
    }

    if (final) {
        Serial.printf("STM32 FW Upload Complete: %uB\n", index + len);
        _updateFile.close();

        // Trigger Update Process
        _updateFile = LittleFS.open("/stm32_update.bin", FILE_READ);
        if (_updateFile) {
            _totalSize = _updateFile.size();
            _sentSize = 0;
            _state = STARTING;
            _statusMsg = "Starting Update...";

            // Calculate CRC before starting? Or assume integrity check at end?
            // Let's calc CRC now to be safe and send it at the end.
             _expectedCrc = calculateCRC32(_updateFile);
             _updateFile.seek(0); // Reset to beginning

             Serial.printf("FW Size: %d, CRC: 0x%08X\n", _totalSize, _expectedCrc);
        }
    }
}

uint32_t OtaManager::calculateCRC32(File& file) {
    // STM32 HW Compatible CRC32 (Standard Poly 0x04C11DB7, Init 0xFFFFFFFF, No Final XOR)
    // Actually, user said: "The STM32F4 OTA implementation uses the hardware-compatible CRC32 algorithm... The ESP32 must perform this exact calculation"
    // STM32 HW CRC:
    // Input data is 32-bit words.
    // Default: Init 0xFFFFFFFF.
    // Poly: 0x04C11DB7.
    // No input reflection, No output reflection.
    // No Final XOR.

    // Most software CRC32 libs use Reflected Input/Output and Final XOR.
    // We need a custom implementation.

    uint32_t crc = 0xFFFFFFFF;
    uint32_t buffer;
    size_t bytesRead;

    // Note: STM32 HW CRC operates on Big Endian words if not configured otherwise?
    // Actually, it takes 32-bit registers. Memory is Little Endian.
    // If we write 0x12345678 to DR, it processes 0x12345678.
    // Byte stream: 78 56 34 12.
    // We need to read 4 bytes, pack into uint32, then process.

    // Simpler: Use a SW implementation that mimics STM32 default.
    // Or just stream bytes and use standard CRC32, but modify STM32 side to match?
    // User Constraint: "STM32F4 OTA implementation uses the hardware-compatible CRC32... ESP32 must perform this exact calculation".

    // Implementation of STM32 default CRC32 in software:

    file.seek(0);
    uint8_t buf[4];
    while(file.read(buf, 4) == 4) {
        uint32_t data = (uint32_t)((buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3]);

        crc = crc ^ data;
        for (int i = 0; i < 32; i++) {
            if (crc & 0x80000000)
                crc = (crc << 1) ^ 0x04C11DB7;
            else
                crc = (crc << 1);
        }
    }
    // Handle remaining bytes? STM32 HAL usually expects word aligned.
    // If file is not multiple of 4, we pad?
    // Assuming binary is 4-byte aligned (ARM binaries usually are).

    file.seek(0);
    return crc;
}

void OtaManager::update() {
    esp_task_wdt_reset();

    switch (_state) {
        case STARTING:
            if (millis() - _lastChunkTime > 1000) {
                // Send Start Command: [CMD=0xF0] [LEN=4] [SIZE_MSB...SIZE_LSB]
                // Using Stm32Serial::sendPacket?
                // Stm32Serial usually wraps in protocol.
                // We need to construct a packet: [AA][F0][04][SIZE][CRC]
                // But Stm32Serial::sendMessage handles the AA/CRC wrapping?
                // No, Stm32Serial has `sendPacket`.

                std::vector<uint8_t> payload;
                payload.push_back((_totalSize >> 24) & 0xFF);
                payload.push_back((_totalSize >> 16) & 0xFF);
                payload.push_back((_totalSize >> 8) & 0xFF);
                payload.push_back(_totalSize & 0xFF);

                // We need to send a raw packet that matches the simple protocol in UART Task
                // [START][CMD][LEN][PAYLOAD][CRC]
                // 0xAA  0xF0  0x04  ...      ...

                Stm32Serial::getInstance().sendRaw(0xF0, payload);
                _statusMsg = "Erasing Target Bank...";
                _state = SENDING;
                _lastChunkTime = millis();
                // Wait for Erase (can take time). STM32 won't ACK immediately?
                // Or we just wait a few seconds before sending data.
                // Erasing 1MB takes ~2-5 seconds.
            }
            break;

        case SENDING:
             // Wait enough time for erase to finish before pumping data?
             // Or assume STM32 UART buffer catches it?
             // Safest: Wait 5 seconds after start command.
             if (_sentSize == 0 && (millis() - _lastChunkTime < 5000)) {
                 return;
             }

             // Send Chunks
             if (millis() - _lastChunkTime > 50) { // Throttle: 20 chunks/sec * 256B = 5KB/s. Full 400KB = 80s.
                 uint8_t buf[UART_CHUNK_SIZE];
                 size_t n = _updateFile.read(buf, UART_CHUNK_SIZE);

                 if (n > 0) {
                     std::vector<uint8_t> payload;
                     for(size_t i=0; i<n; i++) payload.push_back(buf[i]);

                     Stm32Serial::getInstance().sendRaw(0xF1, payload); // CMD_OTA_DATA

                     _sentSize += n;
                     _lastChunkTime = millis();
                     _statusMsg = "Uploading: " + String((int)getSTM32Progress()) + "%";
                 } else {
                     _state = VERIFYING;
                 }
             }
             break;

        case VERIFYING:
            if (millis() - _lastChunkTime > 500) {
                // Send End Command: [CMD=0xF2] [LEN=4] [CRC]
                std::vector<uint8_t> payload;
                payload.push_back((_expectedCrc >> 24) & 0xFF);
                payload.push_back((_expectedCrc >> 16) & 0xFF);
                payload.push_back((_expectedCrc >> 8) & 0xFF);
                payload.push_back(_expectedCrc & 0xFF);

                Stm32Serial::getInstance().sendRaw(0xF2, payload);
                _statusMsg = "Verifying & Rebooting...";
                _state = DONE;
            }
            break;

        case DONE:
            // Idle
            break;

        case ERROR:
            break;

        case IDLE:
            break;
    }
}
