#ifndef ECOFLOW_ESP32_H
#define ECOFLOW_ESP32_H

#include <NimBLEDevice.h>
#include "EcoflowData.h"
#include <vector>
#include <string>

// Forward declaration
class Packet;

enum class ConnectionState {
    NOT_CONNECTED,
    CONNECTING,
    CONNECTED,
    PUBLIC_KEY_EXCHANGE,
    PUBLIC_KEY_SENT,
    SESSION_KEY_REQUESTED,
    AUTH_STATUS_REQUESTED,
    AUTHENTICATING,
    AUTHENTICATED,
    DISCONNECTED
};

class MyClientCallback : public NimBLEClientCallbacks {
public:
    void onConnect(NimBLEClient* pClient) override;
    void onDisconnect(NimBLEClient* pClient) override;
};

class EcoflowESP32 {
public:
    static MyClientCallback g_clientCallback;
    EcoflowESP32();
    ~EcoflowESP32();

    bool begin();
    void setCredentials(const std::string& userId, const std::string& deviceSn);
    bool scan(uint32_t scanTime);
    bool connectToServer();
    void disconnect();
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
    bool isConnecting();
    bool isAuthenticated();

    bool requestData();
    bool setAC(bool on);
    bool setDC(bool on);
    bool setUSB(bool on);

    void onConnect(NimBLEClient* pclient);
    void onDisconnect(NimBLEClient* pclient);
    void onNotify(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);

    static EcoflowESP32* getInstance() { return _instance; }

    bool _running = false;
    bool _sessionKeyEstablished = false;
    uint32_t _lastCommandTime = 0;

private:
    void _setState(ConnectionState newState);
    bool _resolveCharacteristics();
    void _startKeepAliveTask();
    void _stopKeepAliveTask();
    bool _sendCommand(const std::vector<uint8_t>& packet);
    
    // Authentication flow
    bool _startAuthentication();
    void _sendPublicKey();
    void _requestSessionKey();
    void _requestAuthStatus();
    void _sendAuthCredentials();

    // Packet handlers
    void _handlePacket(Packet* pkt);
    void _handlePublicKeyExchange(const std::vector<uint8_t>& payload);
    void _handleSessionKeyResponse(const std::vector<uint8_t>& payload);
    void _handleAuthStatusResponse(const std::vector<uint8_t>& payload);
    void _handleAuthResponse(const std::vector<uint8_t>& payload);
    void _handleDataNotification(Packet* pkt);

    static EcoflowESP32* _instance;
    ConnectionState _state = ConnectionState::NOT_CONNECTED;

    std::string _userId;
    std::string _deviceSn;

    NimBLEAdvertisedDevice* m_pAdvertisedDevice = nullptr;
    NimBLEClient* pClient = nullptr;
    NimBLERemoteCharacteristic* pWriteChr = nullptr;
    NimBLERemoteCharacteristic* pReadChr = nullptr;

    TaskHandle_t _keepAliveTaskHandle = nullptr;
    bool _notificationReceived = false;

    uint8_t _private_key[21];
    uint8_t _shared_key[32]; // Increased to 32 for SHA256, although MD5 uses less
    uint8_t _sessionKey[16];
    uint8_t _sessionIV[16];
    uint8_t _iv[16];

    EcoflowData _data;
};

#endif // ECOFLOW_ESP32_H