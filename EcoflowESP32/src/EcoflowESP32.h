#ifndef EcoflowESP32_h
#define EcoflowESP32_h

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "EcoflowData.h"

class EcoflowESP32 {
public:
    EcoflowESP32();
    ~EcoflowESP32();

    bool begin();
    bool scan(uint32_t scanTime = 10);
    bool connectToServer();
    void disconnect();
    void update();

    // Data access
    int  getBatteryLevel();
    int  getInputPower();
    int  getOutputPower();
    int  getBatteryVoltage();
    int  getACVoltage();
    int  getACFrequency();
    bool isAcOn();
    bool isDcOn();
    bool isUsbOn();
    bool isConnected();

    // Commands
    bool requestData();
    bool setAC(bool on);
    bool setDC(bool on);
    bool setUSB(bool on);
    bool sendCommand(const uint8_t* command, size_t size);

    // Callbacks
    void onConnect(NimBLEClient* pclient);
    void onDisconnect(NimBLEClient* pclient);
    void onNotify(NimBLERemoteCharacteristic* pRemoteCharacteristic,
                  uint8_t* pData, size_t length, bool isNotify);

    // Credentials
    void setCredentials(const std::string& userId, const std::string& deviceSn);

    // Singleton accessor used by global callbacks
    static EcoflowESP32* getInstance() { return _instance; }

    // Public for task / callback access
    uint32_t _lastDataTime          = 0;
    uint32_t _lastCommandTime       = 0;
    bool     _running               = false;
    bool     _notificationReceived  = false;
    uint32_t _lastNotificationTime  = 0;

    // Public parser (so you can test it if needed)
    void parse(uint8_t* pData, size_t length);

private:
    // Internal helpers
    bool _resolveCharacteristics();
    void _startKeepAliveTask();
    void _stopKeepAliveTask();
    bool _authenticate();

    // BLE members
    NimBLEClient*              pClient     = nullptr;
    NimBLERemoteCharacteristic* pWriteChr  = nullptr;
    NimBLERemoteCharacteristic* pReadChr   = nullptr;
    NimBLEAdvertisedDevice*    m_pAdvertisedDevice = nullptr;

    // State
    bool _connected               = false;
    bool _authenticated           = false;
    bool _subscribedToNotifications = false;
    TaskHandle_t _keepAliveTaskHandle = nullptr;

    // Data
    EcoflowData _data;

    // Credentials
    std::string _userId;
    std::string _deviceSn;

    // Singleton
    static EcoflowESP32* _instance;

    friend void keepAliveTask(void* param);
};

#endif // EcoflowESP32_h
