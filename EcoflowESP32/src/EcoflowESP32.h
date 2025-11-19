#ifndef EcoflowESP32_h
#define EcoflowESP32_h

#include <NimBLEDevice.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

// Forward declaration
struct EcoflowData {
    uint8_t batteryLevel = 0;
    uint16_t inputPower = 0;
    uint16_t outputPower = 0;
    uint16_t batteryVoltage = 0;
    uint16_t acVoltage = 0;
    uint16_t acFrequency = 0;
    bool acOn = false;
    bool usbOn = false;
    bool dcOn = false;
};

class EcoflowESP32 {
public:
    // Constructor & lifecycle
    EcoflowESP32();
    ~EcoflowESP32();
    
    bool begin();
    bool scan(uint32_t scanTime = 10);
    bool connectToServer();
    void disconnect();
    void update();
    
    // Data access
    int getBatteryLevel();
    int getInputPower();
    int getOutputPower();
    int getBatteryVoltage();
    int getACVoltage();
    int getACFrequency();
    bool isAcOn();
    bool isDcOn();
    bool isUsbOn();
    bool isConnected();
    
    // Commands
    bool requestData();
    bool setAC(bool on);
    bool setDC(bool on);
    bool setUSB(bool on);
    
    // Internal helpers
    void parse(uint8_t* pData, size_t length);
    bool sendCommand(const uint8_t* command, size_t size);
    
    // Callbacks
    void onConnect(NimBLEClient* pclient);
    void onDisconnect(NimBLEClient* pclient);
    void onNotify(NimBLERemoteCharacteristic* pRemoteCharacteristic,
                  uint8_t* pData, size_t length, bool isNotify);
    
    // Singleton
    static EcoflowESP32* getInstance() { return _instance; }
    
    // Public for keep-alive task and notification callback access
    uint32_t _lastDataTime = 0;
    uint32_t _lastCommandTime = 0;
    bool _running = false;
    bool _notificationReceived = false;          // ← NEW: Track notification arrival
    uint32_t _lastNotificationTime = 0;          // ← NEW: When last notification arrived

private:
    // Connection management
    bool _resolveCharacteristics();
    void _startKeepAliveTask();
    void _stopKeepAliveTask();
    
    // BLE members
    NimBLEClient* pClient = nullptr;
    NimBLERemoteCharacteristic* pWriteChr = nullptr;
    NimBLERemoteCharacteristic* pReadChr = nullptr;
    NimBLEAdvertisedDevice* m_pAdvertisedDevice = nullptr;
    
    // State
    bool _connected = false;
    bool _authenticated = false;
    bool _subscribedToNotifications = false;
    TaskHandle_t _keepAliveTaskHandle = nullptr;
    
    // Data storage
    EcoflowData _data;
    
    // Singleton
    static EcoflowESP32* _instance;
    
    // Friends for task access
    friend void keepAliveTask(void* param);
};

#endif // EcoflowESP32_h
