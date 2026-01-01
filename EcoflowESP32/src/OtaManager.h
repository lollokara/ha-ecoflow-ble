#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <LittleFS.h>
#include <Update.h>
#include "Stm32Serial.h"

class OtaManager {
public:
    static OtaManager& getInstance() {
        static OtaManager instance;
        return instance;
    }

    // Prepare for STM32 OTA
    bool beginStm32Ota(size_t fileSize);

    // Process a chunk of STM32 firmware data from Web Upload
    bool writeStm32Data(uint8_t* data, size_t len);

    // Finalize STM32 OTA upload and start streaming
    bool endStm32Ota();

    // Check if OTA is in progress
    bool isUpdating() const { return _isUpdating; }

    // Get progress (0-100)
    int getProgress() const { return _progress; }

    // Get status message
    String getStatus() const { return _status; }

    // Main update loop
    void update();

    // Notify of ACK received from STM32
    void onAck(uint8_t cmd_id);
    void onNack(uint8_t cmd_id);

private:
    OtaManager() {}

    bool _isUpdating = false;
    size_t _totalSize = 0;
    File _fwFile;
    String _status = "Idle";
    int _progress = 0;

    // Streaming state
    enum State {
        IDLE,
        WAIT_START_ACK,
        SENDING_DATA,
        WAIT_DATA_ACK,
        SEND_END,
        WAIT_END_ACK,
        COMPLETE,
        FAILED
    };
    State _state = IDLE;
    size_t _bytesSent = 0;
    uint32_t _crc = 0;
    uint32_t _lastPacketTime = 0;
    int _retryCount = 0;
    const int MAX_RETRIES = 3;
    const int TIMEOUT_MS = 60000;
    const size_t CHUNK_SIZE = 1024; // Chunk size for UART
    uint8_t _buffer[1024]; // Buffer for reading file
    uint32_t _offset = 0;

    void setState(State s);
    void sendChunk();

    // UART Protocol Helpers (implemented in Stm32Serial or here if Stm32Serial exposes raw send)
    // We will assume Stm32Serial needs a sendRawPacket method.
};

#endif
