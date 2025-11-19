#ifndef EcoflowESP32_h
#define EcoflowESP32_h

#include <Arduino.h>
#include <NimBLEDevice.h>
#include "EcoflowData.h"
#include "EcoflowProtocol.h" // For ToDevice/FromDevice

class EcoflowESP32 {
public:
    EcoflowESP32();
    ~EcoflowESP32();

    bool begin();
    bool scan(uint32_t scanTime = 10);
    bool connectToServer();
    void disconnect();
    void update();

    // --- Data Access ---
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
    uint32_t getLastDataTime();

    // --- Commands ---
    bool requestData();
    bool sendHeartbeat();
    bool setAC(bool on);
    bool setDC(bool on);
    bool setUSB(bool on);

    // --- Callbacks ---
    void onConnect(NimBLEClient* pclient);
    void onDisconnect(NimBLEClient* pclient);
    void onNotify(NimBLERemoteCharacteristic* pRemoteCharacteristic,
                  uint8_t* pData, size_t length, bool isNotify);

    // --- Configuration ---
    void setCredentials(const std::string& userId, const std::string& deviceSn);

    // Singleton accessor used by global callbacks
    static EcoflowESP32* getInstance() { return _instance; }

    // Public for task / callback access
    uint32_t _lastDataTime          = 0;
    uint32_t _lastCommandTime       = 0;
    bool     _running               = false;

    void parse(uint8_t* pData, size_t length);
    void updateData(const char* key, int32_t value);

private:
    // --- Internal Helpers ---
    bool _resolveCharacteristics();
    void _startKeepAliveTask();
    void _stopKeepAliveTask();
    bool _authenticate();
    bool _sendCommand(const ToDevice& message); // New send command helper

    // --- BLE Members ---
    NimBLEClient*              pClient     = nullptr;
    NimBLERemoteCharacteristic* pWriteChr  = nullptr;
    NimBLERemoteCharacteristic* pReadChr   = nullptr;
    NimBLEAdvertisedDevice*    m_pAdvertisedDevice = nullptr;

    // --- State ---
    bool _connected               = false;
    bool _authenticated           = false;
    bool _isAuthenticating        = false;
    bool _subscribedToNotifications = false;
    uint32_t _sequence              = 1;
    TaskHandle_t _keepAliveTaskHandle = nullptr;

    // --- Data ---
    EcoflowData _data;

    // --- Credentials ---
    std::string _userId;
    std::string _deviceSn;

    // Singleton
    static EcoflowESP32* _instance;

    friend void keepAliveTask(void* param);
};

#endif // EcoflowESP32_h