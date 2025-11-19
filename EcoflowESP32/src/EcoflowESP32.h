#ifndef EcoflowESP32_h
#define EcoflowESP32_h

#include <NimBLEDevice.h>
#include <string>
#include "EcoflowData.h"

// Forward declaration
class EcoflowESP32;
namespace EcoflowECDH {
    void init();
    void generate_public_key(uint8_t* buf, size_t* len);
    void compute_shared_secret(const uint8_t* peer_pub_key, size_t peer_len, uint8_t* shared_secret);
    void generateSessionKey(const uint8_t* sRand, const uint8_t* seed, uint8_t* sessionKey, uint8_t* iv);
}

// Keep-alive task function
void keepAliveTask(void* param);

// BLE notification callback
void handleNotificationCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);

// BLE client callbacks
class MyClientCallback : public NimBLEClientCallbacks {
public:
    void onConnect(NimBLEClient* pClient) override;
    void onDisconnect(NimBLEClient* pClient) override;
};

// Enum for the connection and authentication state machine
enum class ConnectionState {
    NOT_CONNECTED,
    CONNECTING,
    CONNECTED,
    WAITING_FOR_SERVICES,
    SERVICES_RESOLVED,
    SUBSCRIBING_TO_NOTIFICATIONS,
    SUBSCRIBED,
    PUBLIC_KEY_EXCHANGE,
    PUBLIC_KEY_SENT,
    PUBLIC_KEY_RECEIVED,
    SESSION_KEY_REQUESTED,
    SESSION_KEY_RECEIVED,
    AUTH_STATUS_REQUESTED,
    AUTH_STATUS_RECEIVED,
    AUTHENTICATING,
    AUTHENTICATED,
    DISCONNECTED
};

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

    // Commands
    bool requestData();
    bool setAC(bool on);
    bool setDC(bool on);
    bool setUSB(bool on);

    // Credentials
    void setCredentials(const std::string& userId, const std::string& deviceSn);
    void setKeyData(const uint8_t* keydata_4096_bytes);

    // Singleton accessor
    static EcoflowESP32* getInstance() { return _instance; }

    // Public members for callbacks/tasks
    void onConnect(NimBLEClient* pclient);
    void onDisconnect(NimBLEClient* pclient);
    void onNotify(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);

    NimBLERemoteCharacteristic* pReadChr = nullptr;
    bool _running = false;
    uint32_t _lastCommandTime = 0;
    bool _notificationReceived = false;
    uint8_t _sessionKey[16];
    uint8_t _sessionIV[16];
    bool _sessionKeyEstablished = false;

private:
    bool _resolveCharacteristics();
    void _startKeepAliveTask();
    void _stopKeepAliveTask();
    bool _startAuthentication();

    void _setState(ConnectionState newState);
    const char* _stateToString(ConnectionState state);

    // Authentication sequence handlers
    void _handlePublicKeyExchange(const uint8_t* pData, size_t length);
    void _handleSessionKeyResponse(const uint8_t* pData, size_t length);
    void _handleAuthStatusResponse(const uint8_t* pData, size_t length);
    void _handleAuthResponse(const uint8_t* pData, size_t length);
    void _handleDataNotification(const uint8_t* pData, size_t length);

    // Packet sending helpers
    bool _sendEncryptedCommand(const std::vector<uint8_t>& packet);
    void _sendPublicKey();
    void _requestSessionKey();
    void _requestAuthStatus();
    void _sendAuthCredentials();

    // BLE members
    NimBLEClient* pClient = nullptr;
    NimBLERemoteCharacteristic* pWriteChr = nullptr;
    NimBLEAdvertisedDevice* m_pAdvertisedDevice = nullptr;
    static MyClientCallback g_clientCallback;

    // State
    ConnectionState _state = ConnectionState::NOT_CONNECTED;
    bool _subscribedToNotifications = false;
    TaskHandle_t _keepAliveTaskHandle = nullptr;

    // Crypto state
    uint8_t _private_key[21]; // SECP160r1 private key is 20 bytes, plus one for mbedtls weirdness
    uint8_t _shared_key[16];
    uint8_t _iv[16];

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
