#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "WebAssets.h"
#include <LittleFS.h>
#include <FS.h>

class OtaManager {
public:
    static OtaManager& getInstance() {
        static OtaManager instance;
        return instance;
    }

    void begin();
    void update();
    void handleSTM32Upload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
    void handleESP32Upload(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);

    // Get current progress for UI
    float getSTM32Progress();
    String getSTM32Status();

private:
    OtaManager();

    // STM32 Update State Machine
    enum STM32State {
        IDLE,
        STARTING,
        SENDING,
        VERIFYING,
        DONE,
        ERROR
    };

    STM32State _state;
    File _updateFile;
    size_t _totalSize;
    size_t _sentSize;
    uint32_t _lastChunkTime;
    String _statusMsg;
    uint32_t _expectedCrc;

    void sendNextChunk();
    uint32_t calculateCRC32(File& file);
};

#endif // OTA_MANAGER_H
