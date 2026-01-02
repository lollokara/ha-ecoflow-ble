#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include "LittleFS.h"
#include <Update.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

class OtaManager {
public:
    static OtaManager& getInstance() {
        static OtaManager instance;
        return instance;
    }

    void begin();
    void update(); // Main loop

    // ESP32 OTA
    void handleEspUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final);

    // STM32 OTA
    void handleStmUpdate(AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final);

    // Get Status JSON
    String getStatusJson();

private:
    OtaManager();

    // STM32 Update State Machine
    enum StmOtaState {
        IDLE,
        STARTING,
        SENDING,
        WAIT_ACK,
        ENDING,
        COMPLETE,
        FAILED
    };

    StmOtaState stmState;
    File updateFile;
    size_t fileSize;
    size_t bytesSent;
    uint32_t lastTxTime;
    uint32_t lastChunkTime;
    uint32_t retryCount;
    String statusMsg;
    int progressPercent;

    void processStmUpdate();
    void sendChunk();

    // Helper to send raw UART
    void sendRaw(uint8_t* data, size_t len);
};

#endif
