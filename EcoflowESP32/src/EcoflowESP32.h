#ifndef EcoflowESP32_h
#define EcoflowESP32_h

#include "Arduino.h"
#include "NimBLEDevice.h"
#include "EcoflowData.h"
#include <string>

/**
 * @file EcoflowESP32.h
 * @brief Ecoflow BLE communication library for ESP32
 *
 * Compatible with multiple NimBLE-Arduino versions
 */

class EcoflowESP32 : public NimBLEClientCallbacks {

public:
    EcoflowESP32();
    ~EcoflowESP32();

    bool begin();
    bool scan(uint32_t scanTime = 5);
    bool connectToServer();
    void disconnect();

    bool sendCommand(const uint8_t* command, size_t size);
    bool requestData();

    bool setAC(bool on);
    bool setDC(bool on);
    bool setUSB(bool on);

    int getBatteryLevel();
    int getInputPower();
    int getOutputPower();

    bool isAcOn();
    bool isDcOn();
    bool isUsbOn();
    bool isConnected();

    void update();
    void parse(uint8_t* pData, size_t length);
    void setAdvertisedDevice(NimBLEAdvertisedDevice* device);
    
    // NEW: Service discovery diagnostic
    void dumpAllServices();

protected:
    void onConnect(NimBLEClient* pclient) override;
    void onDisconnect(NimBLEClient* pclient) override;

private:
    NimBLEClient* pClient;
    NimBLERemoteCharacteristic* pWriteChr;
    NimBLERemoteCharacteristic* pReadChr;
    NimBLEAdvertisedDevice* m_pAdvertisedDevice;

    bool _connected;
    bool _authenticated;
    bool _running;
    bool _subscribedToNotifications;

    TaskHandle_t _keepAliveTaskHandle;
    unsigned long _lastDataTime;
    EcoflowData _data;

    static EcoflowESP32* _instance;

    bool _resolveCharacteristics();
    void _keepAliveTask();
    static void _keepAliveTaskStatic(void* pvParameters);
};

#endif // EcoflowESP32_h
