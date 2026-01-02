#ifndef OTAMANAGER_H
#define OTAMANAGER_H

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>
#include "Stm32Serial.h"

// Forward Declaration
class Stm32Serial;

enum OtaState {
    OTA_IDLE,
    OTA_STARTING,
    OTA_SENDING,
    OTA_VERIFYING,
    OTA_DONE,
    OTA_ERROR
};

class OtaManager {
public:
    static OtaManager& getInstance();

    void begin();
    void loop(); // Legacy, handled by task now

    // STM32 Update Methods
    bool startStm32Update(const String& path);
    OtaState getStm32State() { return _stm32State; }
    int getProgress() { return _progress; }
    String getError() { return _errorMsg; }

    // Callback from Serial when data received
    void handleUartData(uint8_t* data, size_t len);

private:
    OtaManager();
    OtaManager(const OtaManager&) = delete;
    OtaManager& operator=(const OtaManager&) = delete;

    // STM32 State
    OtaState _stm32State = OTA_IDLE;
    File _stm32File;
    size_t _totalSize = 0;
    size_t _sentBytes = 0;
    int _progress = 0;
    String _errorMsg;
    unsigned long _lastActivity = 0;
    int _retryCount = 0;

    void processStm32Update();
    void sendChunk();
    uint8_t calculateCRC8(const uint8_t *data, size_t len);

    // Task wrapper
    void task();
};

#endif
