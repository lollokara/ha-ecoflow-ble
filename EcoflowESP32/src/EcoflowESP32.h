#ifndef ECOFLOW_ESP32_H
#define ECOFLOW_ESP32_H

#include <NimBLEDevice.h>
#include "EcoflowData.h"
#include "EcoflowCrypto.h"
#include <vector>
#include <string>

// Forward declaration
class Packet;

enum class ConnectionState {
    NOT_CONNECTED,
    CONNECTING,
    CONNECTED,
    PUBLIC_KEY_EXCHANGE,
    SESSION_KEY_REQUESTED,
    AUTH_STATUS_REQUESTED,
    AUTHENTICATING,
    AUTHENTICATED,
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
    int getBatteryVoltage();
    int getACVoltage();
    int getACFrequency();
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

    TaskHandle_t _keepAliveTaskHandle = nullptr;
    uint32_t _lastCommandTime = 0;
    bool _running = false;

private:
    static void keepAliveTask(void* param);
    static void notifyCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
    void _handlePacket(Packet* pkt);

    bool _sendCommand(const std::vector<uint8_t>& command);
    
    // Authentication flow
    void _startAuthentication();
    void _handleAuthPacket(Packet* pkt);

    static EcoflowESP32* _instance;
    ConnectionState _state = ConnectionState::NOT_CONNECTED;

    std::string _userId;
    std::string _deviceSn;
    std::string _ble_address;

    NimBLEClient* _pClient = nullptr;
    NimBLERemoteCharacteristic* _pWriteChr = nullptr;
    NimBLERemoteCharacteristic* _pReadChr = nullptr;
    EcoflowClientCallback* _clientCallback;

    EcoflowCrypto _crypto;

    EcoflowData _data;
};

#endif // ECOFLOW_ESP32_H