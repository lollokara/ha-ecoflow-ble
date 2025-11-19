/*
 * EcoflowESP32.h - Header file (Updated - getInstance moved to .cpp)
 */

#ifndef EcoflowESP32_h
#define EcoflowESP32_h

#include <NimBLEDevice.h>
#include "EcoflowData.h"
#include "EcoflowProtocol.h"

// Forward declaration
struct EcoflowData;

class EcoflowESP32 {
public:
    EcoflowESP32();
    ~EcoflowESP32();

    // Lifecycle
    bool begin();
    bool scan(uint32_t scanTime = 5);
    bool connectToServer();
    void disconnect();
    void update();

    // Connection status
    bool isConnected();

    // Getters
    int getBatteryLevel();
    int getInputPower();
    int getOutputPower();
    bool isAcOn();
    bool isDcOn();
    bool isUsbOn();

    // Commands
    bool requestData();
    bool setAC(bool on);
    bool setDC(bool on);
    bool setUSB(bool on);
    bool sendCommand(const uint8_t* command, size_t size);

    // Callbacks (for connection/disconnection only)
    void onConnect(NimBLEClient* pclient);
    void onDisconnect(NimBLEClient* pclient);
    
    // Notification handler (called from lambda callback)
    void onNotify(NimBLERemoteCharacteristic* pRemoteCharacteristic,
                  uint8_t* pData,
                  size_t length,
                  bool isNotify);

    // Singleton - defined in .cpp
    static EcoflowESP32* getInstance();

private:
    // BLE objects
    NimBLEClient* pClient;
    NimBLERemoteCharacteristic* pWriteChr;
    NimBLERemoteCharacteristic* pReadChr;
    NimBLEAdvertisedDevice* m_pAdvertisedDevice;

    // State
    EcoflowData _data;
    bool _connected;
    bool _authenticated;
    bool _running;
    bool _subscribedToNotifications;
    TaskHandle_t _keepAliveTaskHandle;
    uint32_t _lastDataTime;

    // Static instance
    static EcoflowESP32* _instance;

    // Internal methods
    bool _resolveCharacteristics();
    void parse(uint8_t* pData, size_t length);
    void setAdvertisedDevice(NimBLEAdvertisedDevice* device);
};

#endif
