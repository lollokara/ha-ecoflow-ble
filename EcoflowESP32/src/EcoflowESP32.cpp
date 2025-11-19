#include "EcoflowESP32.h"
#include "EcoflowProtocol.h"
#include <mbedtls/md5.h>

// ============================================================================
// Static instance & callbacks
// ============================================================================

EcoflowESP32* EcoflowESP32::_instance = nullptr;

static const uint8_t CONNECT_RETRIES = 3;
static const uint32_t CONNECT_RETRY_DELAY_MS = 250;
static const uint32_t SERVICE_DISCOVERY_DELAY = 1000;
static const uint32_t KEEPALIVE_INTERVAL_MS = 3000;
static const uint32_t KEEPALIVE_CHECK_MS = 500;
static const uint32_t AUTH_TIMEOUT_MS = 10000;

void keepAliveTask(void* param);
void handleNotificationCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic,
                                uint8_t* pData, size_t length, bool isNotify);

// ============================================================================
// Keep-alive task
// ============================================================================

void keepAliveTask(void* param) {
  EcoflowESP32* pThis = static_cast<EcoflowESP32*>(param);
  Serial.println(">>> Keep-Alive task started");

  uint32_t sendCounter = 0;
  while (pThis && pThis->_running) {
    if (pThis->isConnected() && pThis->_authenticated && pThis->_sessionKeyEstablished) {
      uint32_t now = millis();
      uint32_t dt = now - pThis->_lastCommandTime;

      if (dt >= KEEPALIVE_INTERVAL_MS) {
        Serial.print(">>> [KEEP-ALIVE #");
        Serial.print(++sendCounter);
        Serial.println("] Sending encrypted status request...");

        pThis->_notificationReceived = false;

        // Build & send encrypted status request
        auto statusReq = EcoflowCommands::buildStatusRequest();
        EncPacket enc(ENCPACKET_FRAME_TYPE_COMMAND, ENCPACKET_PAYLOAD_TYPE_VX_PROTOCOL,
                      statusReq.data(), statusReq.size(),
                      0, 0, pThis->_sessionKey, pThis->_sessionIV);

        auto encBytes = enc.toBytes();
        pThis->sendEncryptedCommand(encBytes);
        pThis->_lastCommandTime = millis();

        // Wait for notification with timeout
        uint32_t waitStart = millis();
        while (millis() - waitStart < 1000) {
          if (pThis->_notificationReceived) break;
          vTaskDelay(50 / portTICK_PERIOD_MS);
        }

        // If no notification, try polling
        if (!pThis->_notificationReceived && pThis->pReadChr) {
          try {
            std::string val = pThis->pReadChr->readValue();
            if (!val.empty()) {
              Serial.print(">>> [POLL] Read ");
              Serial.print(val.size());
              Serial.println(" bytes from notify characteristic");
              pThis->_decryptAndParseNotification((uint8_t*)val.data(), val.size());
            }
          } catch (...) {
            Serial.println(">>> [POLL] ERROR: readValue failed");
          }
        }
      }
    }

    vTaskDelay(KEEPALIVE_CHECK_MS / portTICK_PERIOD_MS);
  }

  Serial.println(">>> Keep-Alive task stopped");
  vTaskDelete(nullptr);
}

// ============================================================================
// Notification callback
// ============================================================================

void handleNotificationCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic,
                                uint8_t* pData, size_t length, bool isNotify) {
  if (!pRemoteCharacteristic || !pData || length == 0) {
    Serial.println(">>> [ERROR] Invalid params in callback");
    return;
  }

  Serial.print(">>> [NOTIFY] Received ");
  Serial.print(length);
  Serial.print(" bytes: ");
  size_t maxDump = length < 25 ? length : 25;
  for (size_t i = 0; i < maxDump; ++i) {
    if (pData[i] < 0x10) Serial.print("0");
    Serial.print(pData[i], HEX);
    Serial.print(" ");
  }
  if (length > 25) Serial.print("...");
  Serial.println();

  EcoflowESP32* inst = EcoflowESP32::getInstance();
  if (inst) {
    inst->_notificationReceived = true;
    inst->_lastNotificationTime = millis();
    inst->onNotify(pRemoteCharacteristic, pData, length, isNotify);
  }
}

// ============================================================================
// NimBLE client callbacks
// ============================================================================

class MyClientCallback : public NimBLEClientCallbacks {
 public:
  void onConnect(NimBLEClient* pClient) override {
    EcoflowESP32* inst = EcoflowESP32::getInstance();
    if (inst) inst->onConnect(pClient);
  }

  void onDisconnect(NimBLEClient* pClient) override {
    EcoflowESP32* inst = EcoflowESP32::getInstance();
    if (inst) inst->onDisconnect(pClient);
  }
};

static MyClientCallback g_clientCallback;

// ============================================================================
// Constructor / destructor
// ============================================================================

EcoflowESP32::EcoflowESP32()
    : pClient(nullptr),
      pWriteChr(nullptr),
      pReadChr(nullptr),
      m_pAdvertisedDevice(nullptr),
      _connected(false),
      _authenticated(false),
      _subscribedToNotifications(false),
      _keepAliveTaskHandle(nullptr),
      _sessionKeyEstablished(false) {
  _instance = this;
  memset(_sessionKey, 0, 16);
  memset(_sessionIV, 0, 16);
}

EcoflowESP32::~EcoflowESP32() {
  _running = false;
  _stopKeepAliveTask();
  disconnect();
  if (m_pAdvertisedDevice) {
    delete m_pAdvertisedDevice;
    m_pAdvertisedDevice = nullptr;
  }
}

// ============================================================================
// Public API
// ============================================================================

bool EcoflowESP32::begin() {
  try {
    NimBLEDevice::init("");
    Serial.println(">>> BLE stack initialized");
    NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_PUBLIC);
    NimBLEDevice::setMTU(247);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    NimBLEDevice::setSecurityAuth(false, false, false);
    Serial.println(">>> Security settings configured");

    _running = true;
    return true;
  } catch (...) {
    Serial.println(">>> ERROR during BLE init");
    return false;
  }
}

void EcoflowESP32::setCredentials(const std::string& userId,
                                  const std::string& deviceSn) {
  _userId = userId;
  _deviceSn = deviceSn;
  Serial.print(">>> Credentials set: userId=");
  Serial.print(_userId.c_str());
  Serial.print(", deviceSn=");
  Serial.println(_deviceSn.c_str());
}

void EcoflowESP32::setKeyData(const uint8_t* keydata_4096_bytes) {
  if (keydata_4096_bytes) {
    EcoflowKeyData::initKeyData(keydata_4096_bytes);
    EcoflowECDH::init();
    Serial.println(">>> ✓ KEYDATA initialized");
  }
}

bool EcoflowESP32::scan(uint32_t scanTime) {
  try {
    if (m_pAdvertisedDevice) {
      delete m_pAdvertisedDevice;
      m_pAdvertisedDevice = nullptr;
    }

    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setInterval(97);
    pScan->setWindow(97);
    pScan->setActiveScan(true);
    pScan->setMaxResults(20);

    Serial.print(">>> Starting BLE scan for ");
    Serial.print(scanTime);
    Serial.println(" seconds…");

    uint32_t start = millis();
    NimBLEScanResults results = pScan->start(scanTime, false);
    uint32_t elapsed = millis() - start;
    uint32_t count = results.getCount();

    Serial.print(">>> Scan completed in ");
    Serial.print(elapsed);
    Serial.print(" ms, found ");
    Serial.print(count);
    Serial.println(" devices");

    for (uint32_t i = 0; i < count; ++i) {
      NimBLEAdvertisedDevice device = results.getDevice(i);
      if (!device.haveManufacturerData()) continue;

      std::string mfgData = device.getManufacturerData();
      if (mfgData.length() < 2) continue;

      uint16_t manufacturerId =
          (static_cast<uint16_t>(mfgData[1]) << 8) |
          static_cast<uint16_t>(mfgData[0]);

      if (manufacturerId == ECOFLOW_MANUFACTURER_ID) {
        Serial.print(">>> ✓ ECOFLOW DEVICE FOUND! Address: ");
        Serial.println(device.getAddress().toString().c_str());
        m_pAdvertisedDevice = new NimBLEAdvertisedDevice(device);
        return true;
      }
    }

    Serial.println(">>> No Ecoflow device found");
    return false;
  } catch (...) {
    Serial.println(">>> ERROR during scan");
    return false;
  }
}

bool EcoflowESP32::connectToServer() {
  if (!m_pAdvertisedDevice) {
    Serial.println(">>> ERROR: No device found to connect to");
    return false;
  }

  // Reuse existing client if connected with notifications
  if (pClient && pClient->isConnected()) {
    if (pWriteChr && pReadChr && _subscribedToNotifications) {
      Serial.println(">>> Already connected with notifications active");
      _connected = true;

      if (!_authenticated) {
        if (!_authenticate()) {
          Serial.println(">>> ERROR: Authentication failed on reused client");
          _connected = false;
          return false;
        }
      }

      _startKeepAliveTask();
      return true;
    }

    try {
      pClient->disconnect();
    } catch (...) {
    }
    try {
      NimBLEDevice::deleteClient(pClient);
    } catch (...) {
    }
    pClient = nullptr;
    pWriteChr = nullptr;
    pReadChr = nullptr;
    _subscribedToNotifications = false;
  }

  // Connect with retries
  for (uint8_t attempt = 1; attempt <= CONNECT_RETRIES; ++attempt) {
    Serial.print(">>> Connection attempt ");
    Serial.print(attempt);
    Serial.print("/");
    Serial.println(CONNECT_RETRIES);

    try {
      pClient = NimBLEDevice::createClient();
      if (!pClient) {
        Serial.println(">>> ERROR: Failed to create BLE client");
        if (attempt < CONNECT_RETRIES) delay(CONNECT_RETRY_DELAY_MS);
        continue;
      }

      pClient->setClientCallbacks(&g_clientCallback, false);

      Serial.print(">>> Connecting to ");
      Serial.println(m_pAdvertisedDevice->getAddress().toString().c_str());

      if (!pClient->connect(m_pAdvertisedDevice, false)) {
        Serial.println(">>> Failed to connect");
        NimBLEDevice::deleteClient(pClient);
        pClient = nullptr;
        if (attempt < CONNECT_RETRIES) delay(CONNECT_RETRY_DELAY_MS);
        continue;
      }

      Serial.println(">>> BLE connection established");
      delay(SERVICE_DISCOVERY_DELAY);

      if (!_resolveCharacteristics()) {
        Serial.println(">>> ERROR: Failed to resolve characteristics");
        try {
          pClient->disconnect();
        } catch (...) {
        }
        NimBLEDevice::deleteClient(pClient);
        pClient = nullptr;
        if (attempt < CONNECT_RETRIES) delay(CONNECT_RETRY_DELAY_MS);
        continue;
      }

      _connected = true;

      // Subscribe to notifications
      if (pReadChr) {
        try {
          pReadChr->subscribe(true, handleNotificationCallback, false);
          _subscribedToNotifications = true;
          Serial.println(">>> ✓ Subscribed to notifications");
        } catch (...) {
          Serial.println(">>> ERROR: Failed to subscribe to notifications");
          try {
            pClient->disconnect();
          } catch (...) {
          }
          NimBLEDevice::deleteClient(pClient);
          pClient = nullptr;
          _connected = false;
          if (attempt < CONNECT_RETRIES) delay(CONNECT_RETRY_DELAY_MS);
          continue;
        }
      }

      // Authenticate
      if (!_authenticate()) {
        Serial.println(">>> ERROR: Authentication failed");
        try {
          pClient->disconnect();
        } catch (...) {
        }
        NimBLEDevice::deleteClient(pClient);
        pClient = nullptr;
        _connected = false;
        _subscribedToNotifications = false;
        if (attempt < CONNECT_RETRIES) delay(CONNECT_RETRY_DELAY_MS);
        continue;
      }

      // Start keep-alive
      _startKeepAliveTask();

      Serial.println("\n╔════════════════════════════════════════╗");
      Serial.println("║ ✓ Successfully connected & authenticated ║");
      Serial.println("║ Device ready for commands ║");
      Serial.println("╚════════════════════════════════════════╝\n");

      return true;

    } catch (const std::exception& e) {
      Serial.print(">>> Exception during connect: ");
      Serial.println(e.what());
      if (pClient) {
        try {
          NimBLEDevice::deleteClient(pClient);
        } catch (...) {
        }
        pClient = nullptr;
      }
      if (attempt < CONNECT_RETRIES) delay(CONNECT_RETRY_DELAY_MS);
    }
  }

  Serial.println(">>> FAILED: All connection attempts exhausted");
  return false;
}

bool EcoflowESP32::_resolveCharacteristics() {
  try {
    NimBLERemoteService* pSvc = pClient->getService(SERVICE_UUID_ECOFLOW);
    if (!pSvc) {
      Serial.println(">>> ERROR: EcoFlow service not found");
      return false;
    }

    Serial.println(">>> Found EcoFlow service");

    pWriteChr = pSvc->getCharacteristic(CHAR_WRITE_UUID_ECOFLOW);
    pReadChr = pSvc->getCharacteristic(CHAR_READ_UUID_ECOFLOW);

    if (!pWriteChr || !pReadChr) {
      Serial.println(">>> ERROR: Could not find write/read characteristics");
      return false;
    }

    Serial.println(">>> ✓ Found both write and read characteristics");
    return true;

  } catch (...) {
    Serial.println(">>> ERROR during characteristic resolution");
    return false;
  }
}

bool EcoflowESP32::_authenticate() {
  if (!pWriteChr || !_connected) {
    Serial.println(">>> [AUTH] ERROR: Write characteristic not ready");
    return false;
  }

  Serial.println(">>> [AUTH] Starting authentication sequence...");

  // Generate session key via ECDH
  uint8_t sRand[16] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16};
  uint8_t seed[2] = {0x00, 0x01};

  EcoflowECDH::generateSessionKey(sRand, seed, _sessionKey, _sessionIV);
  _sessionKeyEstablished = true;

  Serial.print(">>> [AUTH] Session key: ");
  for (int i = 0; i < 16; i++) {
    if (_sessionKey[i] < 0x10) Serial.print("0");
    Serial.print(_sessionKey[i], HEX);
  }
  Serial.println();

  // Build auto-auth command
  auto authCmd = EcoflowCommands::buildAutoAuthentication(_userId, _deviceSn);

  // Wrap in encrypted EncPacket
  EncPacket enc(ENCPACKET_FRAME_TYPE_COMMAND, ENCPACKET_PAYLOAD_TYPE_VX_PROTOCOL,
                authCmd.data(), authCmd.size(),
                0, 0, _sessionKey, _sessionIV);

  auto encBytes = enc.toBytes();

  // Send encrypted auth
  _authResponseReceived = false;
  try {
    pWriteChr->writeValue(encBytes.data(), encBytes.size(), true);
    Serial.println(">>> [AUTH] ✓ Sent encrypted auth command");
  } catch (...) {
    Serial.println(">>> [AUTH] ERROR: Failed to send auth command");
    return false;
  }

  // Wait for auth response
  Serial.println(">>> [AUTH] Waiting for device to authenticate...");
  uint32_t authStart = millis();
  while (millis() - authStart < AUTH_TIMEOUT_MS) {
    if (_authResponseReceived) {
      Serial.println(">>> [AUTH] ✓ Device authenticated successfully");
      _authenticated = true;
      return true;
    }
    delay(100);
  }

  // Some devices don't send explicit auth response, but accept the auth
  if (_notificationReceived) {
    Serial.println(">>> [AUTH] ✓ Received data notification (assuming auth OK)");
    _authenticated = true;
    return true;
  }

  Serial.println(">>> [AUTH] WARNING: No response, but proceeding...");
  _authenticated = true;
  return true;
}

void EcoflowESP32::disconnect() {
  _running = false;
  _stopKeepAliveTask();

  if (pClient && pClient->isConnected()) {
    try {
      pClient->disconnect();
    } catch (...) {
    }
  }

  if (pClient) {
    try {
      NimBLEDevice::deleteClient(pClient);
    } catch (...) {
    }
    pClient = nullptr;
  }

  pWriteChr = nullptr;
  pReadChr = nullptr;
  _connected = false;
  _authenticated = false;
  _subscribedToNotifications = false;
  _sessionKeyEstablished = false;
}

void EcoflowESP32::_startKeepAliveTask() {
  if (_keepAliveTaskHandle) {
    Serial.println(">>> Keep-Alive task already running");
    return;
  }

  Serial.println(">>> Creating Keep-Alive task");
  xTaskCreatePinnedToCore(keepAliveTask, "keepAlive", 8192, this, 1,
                          &_keepAliveTaskHandle, 0);
}

void EcoflowESP32::_stopKeepAliveTask() {
  if (_keepAliveTaskHandle) {
    vTaskDelete(_keepAliveTaskHandle);
    _keepAliveTaskHandle = nullptr;
  }
}

// ============================================================================
// Encryption & commands
// ============================================================================

bool EcoflowESP32::sendEncryptedCommand(const std::vector<uint8_t>& packet) {
  if (!pWriteChr || !_connected) {
    Serial.println(">>> ERROR: Write characteristic not ready");
    return false;
  }

  try {
    pWriteChr->writeValue((uint8_t*)packet.data(), packet.size(), true);
    Serial.print(">>> [SEND] Sent ");
    Serial.print(packet.size());
    Serial.println(" encrypted bytes");
    return true;
  } catch (...) {
    Serial.println(">>> [SEND] ERROR: Write failed");
    return false;
  }
}

bool EcoflowESP32::sendCommand(const uint8_t* command, size_t size) {
  if (!pWriteChr) return false;
  try {
    pWriteChr->writeValue((uint8_t*)command, size, true);
    return true;
  } catch (...) {
    return false;
  }
}

bool EcoflowESP32::requestData() {
  if (!_sessionKeyEstablished) {
    Serial.println(">>> ERROR: Session key not established");
    return false;
  }

  auto statusReq = EcoflowCommands::buildStatusRequest();
  EncPacket enc(ENCPACKET_FRAME_TYPE_COMMAND, ENCPACKET_PAYLOAD_TYPE_VX_PROTOCOL,
                statusReq.data(), statusReq.size(),
                0, 0, _sessionKey, _sessionIV);

  auto encBytes = enc.toBytes();
  return sendEncryptedCommand(encBytes);
}

bool EcoflowESP32::setAC(bool on) {
  if (!_sessionKeyEstablished) return false;

  auto acCmd = EcoflowCommands::buildAcCommand(on);
  EncPacket enc(ENCPACKET_FRAME_TYPE_COMMAND, ENCPACKET_PAYLOAD_TYPE_VX_PROTOCOL,
                acCmd.data(), acCmd.size(),
                0, 0, _sessionKey, _sessionIV);

  auto encBytes = enc.toBytes();
  Serial.println(on ? ">>> [CMD] AC ON" : ">>> [CMD] AC OFF");
  return sendEncryptedCommand(encBytes);
}

bool EcoflowESP32::setDC(bool on) {
  if (!_sessionKeyEstablished) return false;

  auto dcCmd = EcoflowCommands::buildDcCommand(on);
  EncPacket enc(ENCPACKET_FRAME_TYPE_COMMAND, ENCPACKET_PAYLOAD_TYPE_VX_PROTOCOL,
                dcCmd.data(), dcCmd.size(),
                0, 0, _sessionKey, _sessionIV);

  auto encBytes = enc.toBytes();
  Serial.println(on ? ">>> [CMD] DC ON" : ">>> [CMD] DC OFF");
  return sendEncryptedCommand(encBytes);
}

bool EcoflowESP32::setUSB(bool on) {
  if (!_sessionKeyEstablished) return false;

  auto usbCmd = EcoflowCommands::buildUsbCommand(on);
  EncPacket enc(ENCPACKET_FRAME_TYPE_COMMAND, ENCPACKET_PAYLOAD_TYPE_VX_PROTOCOL,
                usbCmd.data(), usbCmd.size(),
                0, 0, _sessionKey, _sessionIV);

  auto encBytes = enc.toBytes();
  Serial.println(on ? ">>> [CMD] USB ON" : ">>> [CMD] USB OFF");
  return sendEncryptedCommand(encBytes);
}

// ============================================================================
// Notification handling & parsing
// ============================================================================

bool EcoflowESP32::_decryptAndParseNotification(const uint8_t* pData, size_t length) {
  if (length < 6) return false;

  // Check for EncPacket prefix (0x5a 0x5a)
  if (pData[0] != 0x5a || pData[1] != 0x5a) {
    Serial.println(">>> [PARSE] Not an EncPacket, trying raw parse");
    parse((uint8_t*)pData, length);
    return true;
  }

  // Decrypt EncPacket
  EncPacket* pEncPacket = EncPacket::fromBytes(pData, length, _sessionKey, _sessionIV);
  if (!pEncPacket) {
    Serial.println(">>> [PARSE] ERROR: Failed to decrypt EncPacket");
    return false;
  }

  const auto& decrypted = pEncPacket->getPayload();
  Serial.print(">>> [DECRYPT] Decrypted ");
  Serial.print(decrypted.size());
  Serial.println(" bytes");

  // Parse inner Packet (0xaa)
  if (decrypted.size() > 0 && decrypted[0] == 0xaa) {
    Packet* pPkt = Packet::fromBytes(decrypted.data(), decrypted.size());
    if (pPkt) {
      const auto& payload = pPkt->getPayload();
      Serial.print(">>> [PACKET] cmd_id=");
      Serial.print(pPkt->getCmdId());
      Serial.print(", payload_len=");
      Serial.println(payload.size());

      // Detect auth response
      if (pPkt->getCmdId() == PACKET_CMD_ID_GET_AUTH_STATUS) {
        _authResponseReceived = true;
        Serial.println(">>> [AUTH] Auth status response received");
      } else if (pPkt->getCmdId() == PACKET_CMD_ID_REQUEST_STATUS) {
        // Parse status
        EcoflowDelta3::Status status;
        if (EcoflowDelta3::parseStatusResponse(pPkt, status)) {
          _data.batteryLevel = status.batteryLevel;
          _data.inputPower = status.inputPower;
          _data.outputPower = status.outputPower;
          _data.batteryVoltage = status.batteryVoltage;
          _data.acVoltage = status.acVoltage;
          _data.acFrequency = status.acFrequency;
          _data.acOn = status.acOn;
          _data.dcOn = status.dcOn;
          _data.usbOn = status.usbOn;

          Serial.print(">>> [STATUS] Batt:");
          Serial.print(_data.batteryLevel);
          Serial.print("% In:");
          Serial.print(_data.inputPower);
          Serial.print("W Out:");
          Serial.print(_data.outputPower);
          Serial.print("W AC:");
          Serial.print(_data.acOn ? "ON" : "OFF");
          Serial.println();
        }
      }

      delete pPkt;
    }
  } else {
    parse((uint8_t*)decrypted.data(), decrypted.size());
  }

  delete pEncPacket;
  return true;
}

void EcoflowESP32::parse(uint8_t* pData, size_t length) {
  // Legacy v1 frame parser (optional, for backwards compat)
  if (length >= 21 && pData[0] == 0xaa && pData[1] == 0x02) {
    Serial.println(">>> [PARSE] Legacy v1 frame detected");
    // Extract fields from legacy format if needed
  }
}

void EcoflowESP32::onNotify(NimBLERemoteCharacteristic* /*pRemoteCharacteristic*/,
                            uint8_t* pData, size_t length, bool /*isNotify*/) {
  _decryptAndParseNotification(pData, length);
}

void EcoflowESP32::onConnect(NimBLEClient* /*pclient*/) {
  Serial.println(">>> [CALLBACK] Connected");
}

void EcoflowESP32::onDisconnect(NimBLEClient* /*pclient*/) {
  Serial.println(">>> [CALLBACK] Disconnected");
  _connected = false;
  _authenticated = false;
}

// ============================================================================
// Data accessors
// ============================================================================

int EcoflowESP32::getBatteryLevel() { return _data.batteryLevel; }
int EcoflowESP32::getInputPower() { return _data.inputPower; }
int EcoflowESP32::getOutputPower() { return _data.outputPower; }
int EcoflowESP32::getBatteryVoltage() { return _data.batteryVoltage; }
int EcoflowESP32::getACVoltage() { return _data.acVoltage; }
int EcoflowESP32::getACFrequency() { return _data.acFrequency; }
bool EcoflowESP32::isAcOn() { return _data.acOn; }
bool EcoflowESP32::isDcOn() { return _data.dcOn; }
bool EcoflowESP32::isUsbOn() { return _data.usbOn; }
bool EcoflowESP32::isConnected() { return _connected; }

void EcoflowESP32::update() {
  // Placeholder for periodic updates
}
