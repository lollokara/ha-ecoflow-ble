#ifndef EcoflowESP32_h
#define EcoflowESP32_h

#include "Arduino.h"
#include "NimBLEDevice.h"
#include "EcoflowData.h"
#include <queue>

class EcoflowESP32;

/**
 * @brief Callback handler for BLE scan operations
 */
class EcoflowScanCallbacks : public NimBLEScanCallbacks {
public:
    EcoflowScanCallbacks(EcoflowESP32* pEcoflowESP32);
    void onDiscovered(const NimBLEAdvertisedDevice* advertisedDevice) override;
    
private:
    EcoflowESP32* _pEcoflowESP32;
};

/**
 * @file EcoflowESP32.h
 * @brief Ecoflow BLE communication library for ESP32
 * 
 * Provides robust BLE connection management and protocol handling for Ecoflow devices,
 * particularly the Delta 3. Key features:
 * - Automatic device discovery and connection
 * - Notification-based data reception
 * - Keep-alive mechanism to maintain connection
 * - Command queuing for reliable communication
 */
class EcoflowESP32 : public NimBLEClientCallbacks {
    friend class EcoflowScanCallbacks;

public:
    EcoflowESP32();
    ~EcoflowESP32();
    
    /**
     * @brief Initialize BLE system
     * @return true if initialization successful
     */
    bool begin();
    
    /**
     * @brief Scan for nearby Ecoflow devices
     * @param scanTime Time to scan in seconds (default 5)
     * @return true if device found
     */
    bool scan(uint32_t scanTime = 5);
    
    /**
     * @brief Connect to discovered Ecoflow device
     * Performs the full connection sequence:
     * 1. Validates device exists
     * 2. Creates BLE client
     * 3. Waits for service discovery (with retries)
     * 4. Subscribes to notifications
     * 5. Starts keep-alive task
     * @return true if connection successful and ready
     */
    bool connectToServer();
    
    /**
     * @brief Disconnect from device and cleanup
     */
    void disconnect();
    
    // Control commands
    bool setAC(bool on);
    bool setDC(bool on);
    bool setUSB(bool on);
    
    // Data getters
    int getBatteryLevel();
    int getInputPower();
    int getOutputPower();
    bool isAcOn();
    bool isDcOn();
    bool isUsbOn();
    bool isConnected();
    
    // Low-level operations
    bool sendCommand(const uint8_t* command, size_t size);
    bool requestData();
    
    // Utility
    void setAdvertisedDevice(NimBLEAdvertisedDevice* device);
    void update();

private:
    // NimBLEClientCallbacks overrides
    void onConnect(NimBLEClient* pclient) override;
    void onDisconnect(NimBLEClient* pclient, int reason) override;
    
    // Notification callback
    static void notifyCallback(NimBLERemoteCharacteristic* pBLERemoteCharacteristic,
                              uint8_t* pData, size_t length, bool isNotify);
    
    // Data parsing
    void parse(uint8_t* pData, size_t length);
    
    // Internal helpers
    bool _resolveCharacteristics();
    void _keepAliveTask();
    static void _keepAliveTaskStatic(void* pvParameters);
    
    // Member variables
    NimBLEClient* pClient;
    NimBLERemoteCharacteristic* pWriteChr;
    NimBLERemoteCharacteristic* pReadChr;
    NimBLEAdvertisedDevice* m_pAdvertisedDevice;
    
    EcoflowData _data;
    
    bool _connected;
    bool _authenticated;
    bool _running;
    bool _subscribedToNotifications;
    
    static EcoflowESP32* _instance;
    EcoflowScanCallbacks* _scanCallbacks;
    TaskHandle_t _keepAliveTaskHandle;
    
    // Connection timing
    unsigned long _lastDataTime;
    
    static const unsigned long DATA_REQUEST_INTERVAL = 5000;
    static const unsigned long CONNECTION_TIMEOUT = 30000;
};

#endif
