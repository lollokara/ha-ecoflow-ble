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
        Serial.println("OtaManager: LittleFS Mount Failed");
        Serial0.println("OtaManager: LittleFS Mount Failed");
    } else {
        Serial.println("OtaManager: LittleFS Mounted");
        Serial0.println("OtaManager: LittleFS Mounted");
    }
}

void OtaManager::handleEspUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
        Serial.printf("OtaManager: ESP Update Start: %s\n", filename.c_str());
        Serial0.printf("OtaManager: ESP Update Start: %s\n", filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
            Update.printError(Serial0);
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
            Serial.printf("OtaManager: ESP Update Success: %uB\n", index + len);
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
        Serial.printf("OtaManager: STM Update Upload Start: %s\n", filename.c_str());
        Serial0.printf("OtaManager: STM Update Upload Start: %s\n", filename.c_str());
        updateFile = LittleFS.open("/stm32_update.bin", FILE_WRITE);
        if (!updateFile) {
            Serial.println("OtaManager: Failed to open file for writing");
            Serial0.println("OtaManager: Failed to open file for writing");
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
            Serial.printf("OtaManager: STM Update Upload Complete: %uB\n", index + len);
            Serial0.printf("OtaManager: STM Update Upload Complete: %uB\n", index + len);
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
                Serial.printf("OtaManager: State -> STARTING. File Size: %d\n", fileSize);
                Serial0.printf("OtaManager: State -> STARTING. File Size: %d\n", fileSize);
            } else {
                statusMsg = "Failed to open update file";
                stmState = FAILED;
                Serial.println("OtaManager: Failed to reopen update file");
                Serial0.println("OtaManager: Failed to reopen update file");
            }
        }
    }
}

void OtaManager::update() {
    processStmUpdate();
}

void OtaManager::sendRaw(uint8_t* data, size_t len) {
    Serial1.write(data, len);
}

void OtaManager::processStmUpdate() {
    // Simple state machine
    static uint32_t lastStateTime = 0;

    if (stmState == IDLE || stmState == COMPLETE || stmState == FAILED) return;

    if (stmState == STARTING) {
        if (millis() - lastStateTime > 1000) {
            lastStateTime = millis();
            Serial.println("OtaManager: Sending START Command...");
            Serial0.println("OtaManager: Sending START Command...");

            // Packet: [AA][CMD][LEN][PAYLOAD...][CRC]
            // CMD_OTA_START Payload: 4 Bytes Size (Little Endian)
            uint8_t payload[4];
            payload[0] = (uint8_t)(fileSize & 0xFF);
            payload[1] = (uint8_t)((fileSize >> 8) & 0xFF);
            payload[2] = (uint8_t)((fileSize >> 16) & 0xFF);
            payload[3] = (uint8_t)((fileSize >> 24) & 0xFF);

            uint8_t buf[10];
            buf[0] = 0xAA;
            buf[1] = CMD_OTA_START;
            buf[2] = 4; // Len
            memcpy(&buf[3], payload, 4);

            // Calculate Checksum (Sum of CMD + LEN + PAYLOAD) - Must match Bootloader!
            uint8_t sum = buf[1] + buf[2];
            for(int i=0; i<4; i++) sum += payload[i];
            buf[7] = sum;

            sendRaw(buf, 8);

            stmState = WAIT_ACK;
            Serial.println("OtaManager: State -> WAIT_ACK");
        }
    }
    else if (stmState == WAIT_ACK) {
        // Poll for ACK
        while (Serial1.available()) {
            uint8_t b = Serial1.read();
            Serial.printf("RX: %02X ", b);
            Serial0.printf("RX: %02X ", b);
            if (b == CMD_ACK) {
                Serial.println("\nOtaManager: ACK Received!");
                Serial0.println("\nOtaManager: ACK Received!");
                if (bytesSent == 0) {
                    stmState = SENDING;
                    statusMsg = "Flashing...";
                    Serial.println("OtaManager: State -> SENDING");
                    Serial0.println("OtaManager: State -> SENDING");
                } else if (bytesSent >= fileSize) {
                    stmState = ENDING;
                    Serial.println("OtaManager: State -> ENDING");
                    Serial0.println("OtaManager: State -> ENDING");
                } else {
                    stmState = SENDING;
                }
                lastStateTime = millis();
                retryCount = 0;
                return; // State changed, exit loop
            } else if (b == CMD_NACK) {
                Serial.println("\nOtaManager: NACK Received");
                Serial0.println("\nOtaManager: NACK Received");
                retryCount++;
            }
        }

        if (millis() - lastStateTime > 15000) { // Timeout extended to 15s
            Serial.println("\nOtaManager: Timeout waiting for ACK");
            Serial0.println("\nOtaManager: Timeout waiting for ACK");
            retryCount++;
            lastStateTime = millis();
            if (retryCount > 5) {
                stmState = FAILED;
                statusMsg = "Timeout Waiting for ACK";
                Serial.println("OtaManager: State -> FAILED (Too many timeouts)");
                Serial0.println("OtaManager: State -> FAILED (Too many timeouts)");
                updateFile.close();
            } else {
                Serial.println("OtaManager: Retrying...");
                Serial0.println("OtaManager: Retrying...");
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
            Serial.println("OtaManager: EOF. State -> ENDING");
            return;
        }

        buf[0] = 0xAA;
        buf[1] = CMD_OTA_DATA;
        buf[2] = (uint8_t)readLen;
        memcpy(&buf[3], chunk, readLen);

        // Checksum
        uint8_t sum = buf[1] + buf[2];
        for(int i=0; i < readLen; i++) sum += chunk[i];
        buf[3 + readLen] = sum;

        sendRaw(buf, 3 + readLen + 1);

        bytesSent += readLen;
        progressPercent = (bytesSent * 100) / fileSize;

        stmState = WAIT_ACK;
        lastStateTime = millis();
        // Serial.printf("Sent Chunk: %d bytes\n", readLen);
    }
    else if (stmState == ENDING) {
         Serial.println("OtaManager: Sending END Command...");
         uint8_t buf[5];
         buf[0] = 0xAA;
         buf[1] = CMD_OTA_END;
         buf[2] = 0;

         uint8_t sum = buf[1] + buf[2];
         buf[3] = sum;

         sendRaw(buf, 4);

         stmState = COMPLETE; // Wait for ACK? Or just assume done.
         statusMsg = "Flash Complete. Rebooting STM32...";
         Serial.println("OtaManager: State -> COMPLETE");
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
