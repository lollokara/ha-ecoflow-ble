#ifndef ECOFLOW_ESP32_H
#define ECOFLOW_ESP32_H

#include <NimBLEDevice.h>
#include "EcoflowData.h"
#include "EcoflowCrypto.h"
#include <vector>
#include <string>
#include "freertos/task.h"
#include "freertos/queue.h"
#include "pd335_sys.pb.h"

#define MAX_CONNECT_ATTEMPTS 5

// Forward declaration
class Packet;

enum class ConnectionState {
    NOT_CONNECTED,
    SCANNING,
    CREATED,
    ESTABLISHING_CONNECTION,
    CONNECTED,
    SERVICE_DISCOVERY,
    SUBSCRIBING_NOTIFICATIONS,
    PUBLIC_KEY_EXCHANGE,
    PUBLIC_KEY_RECEIVED,
    REQUESTING_SESSION_KEY,
    SESSION_KEY_RECEIVED,
    REQUESTING_AUTH_STATUS,
    AUTH_STATUS_RECEIVED,
    AUTHENTICATING,
    AUTHENTICATED,

    ERROR_TIMEOUT,
    ERROR_NOT_FOUND,
    ERROR_BLE,
    ERROR_PACKET_PARSE,
    ERROR_SEND_REQUEST,
    ERROR_UNKNOWN,
    ERROR_AUTH_FAILED,
    ERROR_TOO_MANY_ERRORS,

    RECONNECTING,
    ERROR_MAX_RECONNECT_ATTEMPTS_REACHED,

    DISCONNECTING,
    DISCONNECTED
};

class EcoflowClientCallback : public NimBLEClientCallbacks {
public:
    EcoflowClientCallback(class EcoflowESP32* instance);
    void onConnect(NimBLEClient* pClient) override;
    void onDisconnect(NimBLEClient* pClient) override;
private:
    class EcoflowESP32* _instance;
};

class EcoflowESP32 {
public:
    EcoflowESP32();
    ~EcoflowESP32();

    bool begin(const std::string& userId, const std::string& deviceSn, const std::string& ble_address);
    void update();

    int getBatteryLevel();
    int getInputPower();
    int getOutputPower();
    float getBatteryVoltage();
    int getACVoltage();
    int getACFrequency();
    float getSolarInputPower();
    float getAcOutputPower();
    float getDcOutputPower();
    float getCellTemperature();
    bool isAcOn();
    bool isDcOn();
    bool isUsbOn();

    bool isConnected();
    bool isAuthenticated();

    bool requestData();
    bool setAC(bool on);
    bool setDC(bool on);
    bool setUSB(bool on);

    void onConnect(NimBLEClient* pclient);
    void onDisconnect(NimBLEClient* pclient);

    uint32_t _lastKeepAliveTime = 0;
    uint8_t _connectionRetries = 0;
    uint32_t _lastConnectionAttempt = 0;
    uint32_t _lastScanTime = 0;
    uint32_t _lastAuthActivity = 0;

private:
    static void notifyCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
    void _handlePacket(Packet* pkt);

    bool _sendCommand(const std::vector<uint8_t>& command);
    
    // Authentication flow
    void _startAuthentication();
    void _handleAuthPacket(Packet* pkt);
    void _sendConfigPacket(const pd335_sys_ConfigWrite& config);

    static EcoflowESP32* _instance;
    ConnectionState _state = ConnectionState::NOT_CONNECTED;
    ConnectionState _lastState = ConnectionState::NOT_CONNECTED;

    std::string _userId;
    std::string _deviceSn;
    std::string _ble_address;

    NimBLEClient* _pClient = nullptr;
    NimBLERemoteCharacteristic* _pWriteChr = nullptr;
    NimBLERemoteCharacteristic* _pReadChr = nullptr;
    EcoflowClientCallback* _clientCallback;
    NimBLEScan* _pScan = nullptr;
    NimBLEAdvertisedDevice* _pAdvertisedDevice = nullptr;
    void _startScan();
    class AdvertisedDeviceCallbacks : public NimBLEAdvertisedDeviceCallbacks {
        public:
            AdvertisedDeviceCallbacks(EcoflowESP32* instance);
            void onResult(NimBLEAdvertisedDevice* advertisedDevice) override;
        private:
            EcoflowESP32* _instance;
    };

    EcoflowCrypto _crypto;

    EcoflowData _data;

    static void ble_task_entry(void* pvParameters);
    TaskHandle_t _ble_task_handle = nullptr;
    QueueHandle_t _ble_queue = nullptr;

    struct BleNotification {
        uint8_t* data;
        size_t length;
    };
};

#endif // ECOFLOW_ESP32_H