#ifndef EcoflowESP32_h
#define EcoflowESP32_h

#include <NimBLEDevice.h>
#include <string>
#include "EcoflowData.h"
#include "EcoflowProtocol.h"

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

  // Commands
  bool requestData();
  bool setAC(bool on);
  bool setDC(bool on);
  bool setUSB(bool on);
  bool sendCommand(const uint8_t* command, size_t size);
  bool sendEncryptedCommand(const std::vector<uint8_t>& packet);

  // Callbacks
  void onConnect(NimBLEClient* pclient);
  void onDisconnect(NimBLEClient* pclient);
  void onNotify(NimBLERemoteCharacteristic* pRemoteCharacteristic,
                uint8_t* pData, size_t length, bool isNotify);

  // Credentials
  void setCredentials(const std::string& userId, const std::string& deviceSn);

  // KEYDATA injection (4096 bytes from ha-ef-ble keydata.py)
  void setKeyData(const uint8_t* keydata_4096_bytes);

  // Singleton accessor used by global callbacks
  static EcoflowESP32* getInstance() { return _instance; }

  // Public for task / callback access
  uint32_t _lastDataTime = 0;
  uint32_t _lastCommandTime = 0;
  bool _running = false;
  bool _notificationReceived = false;
  uint32_t _lastNotificationTime = 0;
  bool _authResponseReceived = false;
  bool _authenticated = false;
  bool _sessionKeyEstablished = false;

  // Session crypto state
  uint8_t _sessionKey[16];
  uint8_t _sessionIV[16];

  // Public parser
  void parse(uint8_t* pData, size_t length);

  NimBLERemoteCharacteristic* pReadChr = nullptr;

 private:
  // Internal helpers
  bool _resolveCharacteristics();
  void _startKeepAliveTask();
  void _stopKeepAliveTask();
  bool _authenticate();
  void _buildAuthFrame(uint8_t* buffer, size_t* length);
  bool _decryptAndParseNotification(const uint8_t* pData, size_t length);

  // BLE members
  NimBLEClient* pClient = nullptr;
  NimBLERemoteCharacteristic* pWriteChr = nullptr;
  NimBLEAdvertisedDevice* m_pAdvertisedDevice = nullptr;

  // State
  bool _connected = false;
  bool _subscribedToNotifications = false;
  TaskHandle_t _keepAliveTaskHandle = nullptr;

  // Data
  EcoflowData _data;

  // Credentials
  std::string _userId;
  std::string _deviceSn;

  // Auth state
  enum AuthState {
    IDLE,
    WAITING_FOR_DEVICE_PUBLIC_KEY,
    WAITING_FOR_SRAND_SEED,
    WAITING_FOR_AUTH_RESPONSE
  };
  volatile AuthState _authState;

  // Auth flow handlers
  void initBleSessionKeyHandler(NimBLERemoteCharacteristic* pRemoteCharacteristic,
                                uint8_t* pData, size_t length, bool isNotify);
  void getKeyInfoReqHandler(NimBLERemoteCharacteristic* pRemoteCharacteristic,
                            uint8_t* pData, size_t length, bool isNotify);
  void authHandler(NimBLERemoteCharacteristic* pRemoteCharacteristic,
                   uint8_t* pData, size_t length, bool isNotify);

  // Singleton
  static EcoflowESP32* _instance;

  friend void keepAliveTask(void* param);
};

#endif  // EcoflowESP32_h
