#ifndef OTAMANAGER_H
#define OTAMANAGER_H

#include <Arduino.h>
#include <LittleFS.h>

class Stm32Serial;

class OtaManager {
public:
    enum OtaState {
        OTA_IDLE,
        OTA_STARTING,
        OTA_SENDING,
        OTA_ENDING,
        OTA_WAIT_APPLY,
        OTA_ERROR
    };

    OtaManager(Stm32Serial* stm32);
    void begin();
    void handle();
    void startStm32Update(const char* filepath);
    void processStoredUpdate();

    void onAck(uint8_t cmd);
    void onNack(uint8_t cmd);

private:
    Stm32Serial* _stm32;
    OtaState _state;
    File _file;
    uint32_t _totalSize;
    uint32_t _bytesSent;
    uint32_t _lastChunkTime;
    uint8_t _retryCount;
    void sendChunk();
};

#endif
