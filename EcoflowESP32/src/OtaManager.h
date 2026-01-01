#ifndef OTA_MANAGER_H
#define OTA_MANAGER_H

#include <Arduino.h>
#include <FS.h>

enum OtaState {
    OTA_IDLE,
    OTA_BUFFERING,
    OTA_ERASING,
    OTA_FLASHING,
    OTA_COMPLETE,
    OTA_ERROR
};

class OtaManager {
public:
    static void startUpdateSTM32(const char* filepath, size_t size);
    static bool updateESP32(Stream& firmware, size_t size);

    static OtaState getState();
    static int getProgress();
    static String getError();
    static void cleanup();

private:
    static void otaTask(void* param);
    static OtaState _state;
    static int _progress;
    static String _error;
    static String _fwPath;
    static size_t _fwSize;
};

#endif // OTA_MANAGER_H
