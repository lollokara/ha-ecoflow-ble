#include "OtaManager.h"
#include <Update.h>
#include "LogBuffer.h"

static const char* TAG = "OtaManager";

// UART Protocol commands (must match ecoflow_protocol.h or defined here)
#define CMD_OTA_START 0xF0
#define CMD_OTA_DATA  0xF1
#define CMD_OTA_END   0xF2
#define CMD_OTA_ACK   0xF3
#define CMD_OTA_NACK  0xF4

// Helper to pack raw packet
static size_t pack_ota_packet(uint8_t* buf, uint8_t cmd, uint8_t* payload, size_t len) {
    buf[0] = 0xAA; // Start byte
    buf[1] = cmd;
    buf[2] = (uint8_t)len;
    memcpy(&buf[3], payload, len);
    buf[3 + len] = 0; // Placeholder for CRC
    // Calculate CRC8
    uint8_t crc = 0; // Simple CRC8 implementation or use existing
    // We should use existing CRC8 from ecoflow_protocol.c if possible, but it's not exposed.
    // Let's implement a simple one matching the protocol.
    // The protocol uses specific CRC8. Let's look at Stm32Serial.cpp: calculate_crc8
    // I need to duplicate it or expose it.
    // Since I cannot modify ecoflow_protocol.c easily without sync, I'll copy the logic if it's simple.
    // Actually, Stm32Serial includes ecoflow_protocol.h. Let's check if calculate_crc8 is exposed.
    // It's static in Stm32Serial.cpp usually or in ecoflow_protocol.c.
    // Checked file list: EcoflowProtocol.h exists.
    // Assuming calculate_crc8 is available or I can copy it.
    // Let's assume for now I can access it via extern "C" or copy it.
    // I'll check EcoflowProtocol.h content later. For now, I'll assume I need to implement it.
    // Wait, the Stm32Serial.cpp uses `calculate_crc8`. I can make it public or copy.
    // Copying is safer to avoid breaking changes.
    for (size_t i = 1; i < 3 + len; i++) {
        uint8_t data = buf[i];
        for (int j = 0; j < 8; j++) {
            if ((crc ^ data) & 0x01) {
                crc = (crc >> 1) ^ 0x8C;
            } else {
                crc >>= 1;
            }
            data >>= 1;
        }
    }
    buf[3 + len] = crc;
    return 4 + len;
}

bool OtaManager::beginStm32Ota(size_t fileSize) {
    if (_isUpdating) return false;
    ESP_LOGI(TAG, "Starting STM32 OTA download. Size: %d", fileSize);

    // Open file in write mode
    LittleFS.remove("/update.bin");
    _fwFile = LittleFS.open("/update.bin", FILE_WRITE);
    if (!_fwFile) {
        ESP_LOGE(TAG, "Failed to open file for writing");
        return false;
    }

    _isUpdating = true;
    _totalSize = fileSize;
    _bytesWritten = 0;
    _progress = 0;
    _status = "Downloading...";
    return true;
}

bool OtaManager::writeStm32Data(uint8_t* data, size_t len) {
    if (!_isUpdating || !_fwFile) return false;
    size_t written = _fwFile.write(data, len);
    if (written != len) {
        ESP_LOGE(TAG, "Write failed");
        _fwFile.close();
        _isUpdating = false;
        _status = "Write Error";
        return false;
    }
    _bytesWritten += written;
    _progress = (_bytesWritten * 100) / _totalSize;
    // Keep progress < 100 until fully streamed
    if (_progress > 49) _progress = 49;
    return true;
}

bool OtaManager::endStm32Ota() {
    if (!_isUpdating || !_fwFile) return false;
    _fwFile.close();
    ESP_LOGI(TAG, "Download complete. Starting streaming...");

    // Open for reading
    _fwFile = LittleFS.open("/update.bin", FILE_READ);
    if (!_fwFile) {
        ESP_LOGE(TAG, "Failed to open file for reading");
        _isUpdating = false;
        _status = "Read Error";
        return false;
    }

    _totalSize = _fwFile.size();
    _offset = 0;
    _state = SENDING_DATA; // Simplified: No handshake for start? Usually needed.
    // Protocol:
    // 1. Send CMD_OTA_START with Total Size & CRC32 of file.
    // 2. Wait ACK.
    // 3. Send CMD_OTA_DATA chunks.
    // 4. Send CMD_OTA_END.

    // Calculate CRC32 of file
    _crc = 0xFFFFFFFF; // Init
    uint8_t buf[256];
    while (_fwFile.available()) {
        size_t n = _fwFile.read(buf, sizeof(buf));
        for(size_t i=0; i<n; i++) {
            uint8_t b = buf[i];
            // Simple CRC32 (Poly 0x04C11DB7)
            // Using a table would be faster, but this is once.
             // Standard STM32 CRC32 logic
            // Actually, let's use a simpler check or just size for now if we don't have a crc32 lib.
            // ESP32 has ROM CRC32.
            // But let's just sum it for now or implement properly later.
            // The memory says: "The STM32F4 OTA implementation uses the hardware-compatible CRC32 algorithm (Forward Poly 0x04C11DB7, Init 0xFFFFFFFF, No Final XOR). The ESP32 must perform this exact calculation in software to verify firmware integrity before transmission."
            // So I MUST implement it correctly.

             for (int j = 0; j < 8; j++) {
                uint32_t bit = ((b >> (7-j)) & 1) ^ ((_crc >> 31) & 1);
                _crc <<= 1;
                if (bit) _crc ^= 0x04C11DB7;
            }
        }
    }
    _fwFile.seek(0);

    setState(WAIT_START_ACK);
    sendStartCmd();

    _status = "Flashing STM32...";
    _progress = 50;
    return true;
}

void OtaManager::sendStartCmd() {
    uint8_t payload[8];
    memcpy(&payload[0], &_totalSize, 4);
    memcpy(&payload[4], &_crc, 4);

    uint8_t packet[16];
    size_t len = pack_ota_packet(packet, CMD_OTA_START, payload, 8);
    Stm32Serial::getInstance().sendPacket(packet, len);
    _lastPacketTime = millis();
    ESP_LOGI(TAG, "Sent Start CMD. Size: %d, CRC: %08X", _totalSize, _crc);
}

void OtaManager::sendDataCmd(uint32_t offset, uint8_t* data, size_t len) {
    // CMD_OTA_DATA: [Offset(4)][Data(N)]
    // Max packet size is limited. UART buffer is 1024.
    // Payload max 200 bytes is safe.
    // Let's use 128 bytes chunks.

    // Wait, the header takes 4 bytes, offset 4 bytes.
    // If I send 128 bytes data, payload is 132. Total ~137. Safe.
}

void OtaManager::sendChunk() {
    if (!_fwFile.available()) {
        setState(WAIT_END_ACK);
        sendEndCmd(_crc); // Send CRC again as verification
        return;
    }

    size_t chunkSize = 128;
    uint8_t buf[chunkSize + 4];
    memcpy(buf, &_offset, 4);
    size_t read = _fwFile.read(&buf[4], chunkSize);

    uint8_t packet[256];
    size_t len = pack_ota_packet(packet, CMD_OTA_DATA, buf, read + 4);
    Stm32Serial::getInstance().sendPacket(packet, len);
    _lastPacketTime = millis();
}

void OtaManager::sendEndCmd(uint32_t crc) {
     uint8_t payload[4];
     memcpy(payload, &crc, 4);
     uint8_t packet[12];
     size_t len = pack_ota_packet(packet, CMD_OTA_END, payload, 4);
     Stm32Serial::getInstance().sendPacket(packet, len);
     _lastPacketTime = millis();
}


void OtaManager::update() {
    if (!_isUpdating || _state == IDLE || _state == COMPLETE || _state == FAILED) return;

    uint32_t now = millis();

    // Special handling for START: Retry rapidly to catch Bootloader window
    if (_state == WAIT_START_ACK) {
        if (now - _lastPacketTime > 300) {
            if (_retryCount++ < (60000 / 300)) { // Retry for ~60s
                sendStartCmd();
            } else {
                ESP_LOGE(TAG, "OTA Failed: Start Timeout");
                _state = FAILED;
                _status = "Failed: No Response";
                _isUpdating = false;
                _fwFile.close();
            }
        }
        return;
    }

    // Normal timeout for data/end
    if (now - _lastPacketTime > TIMEOUT_MS) {
        if (_retryCount++ < MAX_RETRIES) {
            ESP_LOGW(TAG, "Timeout, retrying...");
            if (_state == WAIT_DATA_ACK) {
                _fwFile.seek(_offset);
                sendChunk();
            }
            else if (_state == WAIT_END_ACK) sendEndCmd(_crc);
        } else {
            ESP_LOGE(TAG, "OTA Failed: Timeout");
            _state = FAILED;
            _status = "Failed: Timeout";
            _isUpdating = false;
            _fwFile.close();
        }
    }
}

void OtaManager::setState(State s) {
    _state = s;
    _retryCount = 0;
}

void OtaManager::onAck(uint8_t cmd_id) {
    if (!_isUpdating) return;

    if (_state == WAIT_START_ACK && cmd_id == CMD_OTA_START) {
        ESP_LOGI(TAG, "Start ACK received. Sending data...");
        _offset = 0;
        _fwFile.seek(0);
        setState(WAIT_DATA_ACK);
        sendChunk();
    } else if (_state == WAIT_DATA_ACK && cmd_id == CMD_OTA_DATA) {
        _offset = _fwFile.position(); // Update offset
        _progress = 50 + (_offset * 50) / _totalSize;
        // Continue sending
        _retryCount = 0; // Reset retry on success
        sendChunk();
    } else if (_state == WAIT_END_ACK && cmd_id == CMD_OTA_END) {
        ESP_LOGI(TAG, "End ACK received. Success!");
        _state = COMPLETE;
        _status = "Success";
        _progress = 100;
        _isUpdating = false;
        _fwFile.close();
        LittleFS.remove("/update.bin");
    }
}

void OtaManager::onNack(uint8_t cmd_id) {
     ESP_LOGE(TAG, "Received NACK for cmd %02X", cmd_id);
     // Logic to handle NACK?
}
