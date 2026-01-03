#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <FS.h>
#include <LittleFS.h>

class OtaManager {
public:
    static OtaManager& getInstance() {
        static OtaManager instance;
        return instance;
    }

    void begin();

    // Trigger the update process using a file stored in LittleFS
    void startStm32Update(const String& filename);

    bool isUpdating() const { return _isUpdating; }

    // Main state machine update
    void update();

    // Process incoming OTA-related responses from STM32
    void handlePacket(uint8_t cmd, uint8_t* payload, uint8_t len);

private:
    OtaManager();

    bool _isUpdating;
    String _filename;
    File _file;
    size_t _fileSize;
    size_t _sentBytes;

    enum State {
        IDLE,
        STARTING,
        SENDING,
        ENDING,
        APPLYING,
        DONE,
        ERROR
    };
    State _state;

    uint32_t _lastPacketTime;
    int _retryCount;
    uint32_t _globalCRC;

    void sendChunk();
    uint32_t calculateFileCRC();
};

#endif
