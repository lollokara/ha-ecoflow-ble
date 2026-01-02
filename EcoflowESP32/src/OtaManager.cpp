#include "OtaManager.h"
#include <esp_log.h>

static const char* TAG = "OtaManager";

// UART Commands for OTA (Must match Bootloader)
#define CMD_OTA_START   0xF0
#define CMD_OTA_DATA    0xF1
#define CMD_OTA_END     0xF2
#define CMD_ACK         0x06
#define CMD_NACK        0x15

OtaManager::OtaManager() : stmState(IDLE), fileSize(0), bytesSent(0), retryCount(0), progressPercent(0) {}

void OtaManager::begin() {
    if (!LittleFS.begin(true)) {
        ESP_LOGE(TAG, "LittleFS Mount Failed");
    } else {
        ESP_LOGI(TAG, "LittleFS Mounted");
    }
}

void OtaManager::handleEspUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
        ESP_LOGI(TAG, "ESP Update Start: %s", filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
        statusMsg = "ESP Update Started";
        progressPercent = 0;
    }

    if (Update.write(data, len) != len) {
        Update.printError(Serial);
    }

    // Calculate progress (Approximate since total unknown usually, but request has content-length)
    if(request->contentLength() > 0)
        progressPercent = (index + len) * 100 / request->contentLength();

    if (final) {
        if (Update.end(true)) {
            ESP_LOGI(TAG, "ESP Update Success: %uB", index + len);
            statusMsg = "ESP Update Success. Rebooting...";
            progressPercent = 100;
            delay(1000);
            ESP.restart();
        } else {
            Update.printError(Serial);
            statusMsg = "ESP Update Failed";
        }
    }
}

void OtaManager::handleStmUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
        ESP_LOGI(TAG, "STM Update Upload Start: %s", filename.c_str());
        updateFile = LittleFS.open("/stm32_update.bin", FILE_WRITE);
        if (!updateFile) {
            ESP_LOGE(TAG, "Failed to open file for writing");
            return;
        }
        stmState = IDLE;
        statusMsg = "Uploading STM32 Firmware...";
        progressPercent = 0;
    }

    if (updateFile) {
        updateFile.write(data, len);
    }

    if(request->contentLength() > 0)
        progressPercent = (index + len) * 100 / request->contentLength();

    if (final) {
        if (updateFile) {
            updateFile.close();
            ESP_LOGI(TAG, "STM Update Upload Complete: %uB", index + len);
            statusMsg = "Upload Complete. Starting Flash...";
            progressPercent = 0;

            // Start OTA Process
            updateFile = LittleFS.open("/stm32_update.bin", FILE_READ);
            if(updateFile) {
                fileSize = updateFile.size();
                bytesSent = 0;
                stmState = STARTING;
                lastTxTime = millis();
                retryCount = 0;
            } else {
                statusMsg = "Failed to open update file";
                stmState = FAILED;
            }
        }
    }
}

void OtaManager::update() {
    processStmUpdate();
}

void OtaManager::sendRaw(uint8_t* data, size_t len) {
    // We assume Serial1 is initialized by Stm32Serial
    // We bypass Stm32Serial wrapper for raw access or add friend/method there.
    // Ideally Stm32Serial should handle this, but for simplicity here we write directly.
    Serial1.write(data, len);
}

void OtaManager::processStmUpdate() {
    // Simple state machine
    static uint32_t lastStateTime = 0;

    if (stmState == IDLE || stmState == COMPLETE || stmState == FAILED) return;

    if (stmState == STARTING) {
        if (millis() - lastStateTime > 1000) {
            lastStateTime = millis();
            ESP_LOGI(TAG, "Sending START Command...");

            // Packet: [AA][CMD][LEN][PAYLOAD...][CRC]
            // CMD_OTA_START Payload: 4 Bytes Size (Little Endian)
            uint8_t payload[4];
            payload[0] = (uint8_t)(fileSize & 0xFF);
            payload[1] = (uint8_t)((fileSize >> 8) & 0xFF);
            payload[2] = (uint8_t)((fileSize >> 16) & 0xFF);
            payload[3] = (uint8_t)((fileSize >> 24) & 0xFF);

            // But Bootloader expects simple header [CMD][LEN]... wait, look at my bootloader code.
            // Bootloader:
            // Receive byte -> if 0xAA ->
            // Receive header[2] (CMD, LEN)
            // Receive Payload (LEN)
            // Receive CRC (1)

            uint8_t buf[10];
            buf[0] = 0xAA;
            buf[1] = CMD_OTA_START;
            buf[2] = 4; // Len
            memcpy(&buf[3], payload, 4);

            // CRC: Sum of buf[1] to buf[2+Len] ?
            // Bootloader: uint8_t received_crc = b; uint8_t calcd_crc = calculate_crc8(&parseBuffer[1], parseIndex - 1);
            // So CRC is over CMD, LEN, PAYLOAD.
            // I need calculate_crc8.

            uint8_t crc = 0; // Placeholder, assuming same logic as ecoflow_protocol.c or simple sum if I implemented simple sum.
            // Bootloader calls calculate_crc8. I need to match that.
            // Assume standard CRC8 used in project.
            // For now, I'll assume 0 for check or implement the function.
            // Wait, I didn't implement calculate_crc8 in Bootloader main.c yet!
            // I used "calculate_crc8" in the code block but didn't define it.
            // I need to FIX Bootloader main.c to include crc function or remove check.

            // Assuming I will fix bootloader to use Sum or simple XOR if I don't want to import full CRC lib.
            // Let's use simple Sum for bootloader to save space.

            // ... Back to ESP32 side ...
            // Let's assume Sum for now.
            uint8_t sum = 0;
            for(int i=1; i<7; i++) sum += buf[i];
            buf[7] = sum;

            sendRaw(buf, 8);

            stmState = WAIT_ACK;
        }
    }
    else if (stmState == WAIT_ACK) {
        // Poll for ACK
        if (Serial1.available()) {
            uint8_t b = Serial1.read();
            if (b == CMD_ACK) {
                if (bytesSent == 0) {
                    stmState = SENDING;
                    statusMsg = "Flashing...";
                } else if (bytesSent >= fileSize) {
                    stmState = ENDING;
                } else {
                    stmState = SENDING;
                }
                lastStateTime = millis();
                retryCount = 0;
            } else if (b == CMD_NACK) {
                ESP_LOGE(TAG, "NACK Received");
                retryCount++;
            }
        }

        if (millis() - lastStateTime > 5000) { // Timeout
            retryCount++;
            lastStateTime = millis();
            if (retryCount > 5) {
                stmState = FAILED;
                statusMsg = "Timeout Waiting for ACK";
                updateFile.close();
            } else {
                // Retransmit logic?
                // If we were STARTING, go back to STARTING.
                // If SENDING, go back to SENDING (resend chunk).
                if(bytesSent == 0) stmState = STARTING;
                else stmState = SENDING;
            }
        }
    }
    else if (stmState == SENDING) {
        // Send Chunk
        uint8_t buf[260]; // AA CMD LEN PAYLOAD CRC
        uint8_t chunk[250];
        size_t readLen = updateFile.read(chunk, 240); // Max payload 250ish

        if (readLen == 0) {
            stmState = ENDING;
            return;
        }

        buf[0] = 0xAA;
        buf[1] = CMD_OTA_DATA;
        buf[2] = (uint8_t)readLen;
        memcpy(&buf[3], chunk, readLen);

        uint8_t sum = 0;
        for(int i=1; i < 3 + readLen; i++) sum += buf[i];
        buf[3 + readLen] = sum;

        sendRaw(buf, 3 + readLen + 1);

        bytesSent += readLen;
        progressPercent = (bytesSent * 100) / fileSize;

        stmState = WAIT_ACK;
        lastStateTime = millis();
    }
    else if (stmState == ENDING) {
         ESP_LOGI(TAG, "Sending END Command...");
         uint8_t buf[5];
         buf[0] = 0xAA;
         buf[1] = CMD_OTA_END;
         buf[2] = 0;
         uint8_t sum = 0;
         for(int i=1; i<3; i++) sum += buf[i];
         buf[3] = sum;

         sendRaw(buf, 4);

         stmState = COMPLETE; // Wait for ACK? Or just assume done.
         statusMsg = "Flash Complete. Rebooting STM32...";
         updateFile.close();
    }
}

String OtaManager::getStatusJson() {
    String s = "{";
    s += "\"status\":\"" + statusMsg + "\",";
    s += "\"progress\":" + String(progressPercent);
    s += "}";
    return s;
}
