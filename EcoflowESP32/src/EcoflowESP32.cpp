#include "EcoflowESP32.h"
#include "EcoflowProtocol.h"
#include "uECC.h"

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

  EcoflowESP32* inst = EcoflowESP32::getInstance();
  if (inst) {
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
      _sessionKeyEstablished(false),
      _authState(IDLE) {
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

  if (pClient && pClient->isConnected()) {
    return true;
  }

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
  Serial.println(">>> [AUTH] Starting authentication sequence...");
  _authState = WAITING_FOR_DEVICE_PUBLIC_KEY;

  EcoflowECDH::generateKeys();
  const uint8_t* public_key = EcoflowECDH::getPublicKey();

  std::vector<uint8_t> payload;
  payload.push_back(0x01);
  payload.push_back(0x00);
  payload.insert(payload.end(), public_key, public_key + ef_uECC_BYTES * 2);

  EncPacket enc(ENCPACKET_FRAME_TYPE_COMMAND, ENCPACKET_PAYLOAD_TYPE_VX_PROTOCOL,
                payload.data(), payload.size());

  auto encBytes = enc.toBytes();
  sendEncryptedCommand(encBytes);

  uint32_t authStart = millis();
  while (millis() - authStart < AUTH_TIMEOUT_MS) {
    if (_authenticated) {
      Serial.println("\n╔════════════════════════════════════════╗");
      Serial.println("║ ✓ Successfully connected & authenticated ║");
      Serial.println("║ Device ready for commands ║");
      Serial.println("╚════════════════════════════════════════╝\n");
      _startKeepAliveTask();
      return true;
    }
    delay(100);
  }
  return false;
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
void EcoflowESP32::onNotify(NimBLERemoteCharacteristic* pRemoteCharacteristic,
                            uint8_t* pData, size_t length, bool isNotify) {
    switch(_authState) {
        case WAITING_FOR_DEVICE_PUBLIC_KEY:
            initBleSessionKeyHandler(pRemoteCharacteristic, pData, length, isNotify);
            break;
        case WAITING_FOR_SRAND_SEED:
            getKeyInfoReqHandler(pRemoteCharacteristic, pData, length, isNotify);
            break;
        case WAITING_FOR_AUTH_RESPONSE:
            authHandler(pRemoteCharacteristic, pData, length, isNotify);
            break;
        default:
            _decryptAndParseNotification(pData, length);
    }
}


void EcoflowESP32::initBleSessionKeyHandler(NimBLERemoteCharacteristic* pRemoteCharacteristic,
                                            uint8_t* pData, size_t length, bool isNotify) {
    EncPacket* enc_packet = EncPacket::fromBytes(pData, length);
    if (enc_packet) {
        const auto& payload = enc_packet->getPayload();
        uint8_t device_public_key[ef_uECC_BYTES * 2];
        memcpy(device_public_key, payload.data() + 3, ef_uECC_BYTES * 2);
        EcoflowECDH::computeSharedSecret(device_public_key);

        std::vector<uint8_t> req_payload;
        req_payload.push_back(0x02);
        EncPacket req_enc(ENCPACKET_FRAME_TYPE_COMMAND, ENCPACKET_PAYLOAD_TYPE_VX_PROTOCOL,
                          req_payload.data(), req_payload.size());
        auto encBytes = req_enc.toBytes();
        sendEncryptedCommand(encBytes);
        _authState = WAITING_FOR_SRAND_SEED;
        delete enc_packet;
    }
}

void EcoflowESP32::getKeyInfoReqHandler(NimBLERemoteCharacteristic* pRemoteCharacteristic,
                                       uint8_t* pData, size_t length, bool isNotify) {
    EncPacket* enc_packet = EncPacket::fromBytes(pData, length);
    if (enc_packet) {
        const auto& encrypted_payload = enc_packet->getPayload();

        // The IV for this step is derived from the shared key itself
        uint8_t temp_iv[16];
        mbedtls_md5_context md5_ctx;
        mbedtls_md5_init(&md5_ctx);
        mbedtls_md5_starts_ret(&md5_ctx);
        mbedtls_md5_update(&md5_ctx, EcoflowECDH::getSharedKey().data(), EcoflowECDH::getSharedKey().size());
        mbedtls_md5_finish(&md5_ctx, temp_iv);
        mbedtls_md5_free(&md5_ctx);

        auto decrypted_payload = EncPacket::decryptPayload(encrypted_payload.data() + 1, encrypted_payload.size() - 1, EcoflowECDH::getSharedKey().data(), temp_iv);

        uint8_t sRand[16];
        uint8_t seed[2];
        memcpy(sRand, decrypted_payload.data(), 16);
        memcpy(seed, decrypted_payload.data() + 16, 2);

        EcoflowECDH::generateSessionKey(sRand, seed, _sessionKey, _sessionIV);
        _sessionKeyEstablished = true;

        auto authCmd = EcoflowCommands::buildAutoAuthentication(_userId, _deviceSn);
        EncPacket auth_enc(ENCPACKET_FRAME_TYPE_COMMAND, ENCPACKET_PAYLOAD_TYPE_VX_PROTOCOL,
                           authCmd.data(), authCmd.size(), 0, 0, _sessionKey, _sessionIV);
        auto encBytes = auth_enc.toBytes();
        sendEncryptedCommand(encBytes);
        _authState = WAITING_FOR_AUTH_RESPONSE;
        delete enc_packet;
    }
}

void EcoflowESP32::authHandler(NimBLERemoteCharacteristic* pRemoteCharacteristic,
                               uint8_t* pData, size_t length, bool isNotify) {
    EncPacket* enc_packet = EncPacket::fromBytes(pData, length, _sessionKey, _sessionIV);
    if (enc_packet) {
        Packet* packet = Packet::fromBytes(enc_packet->getPayload().data(), enc_packet->getPayload().size());
        if (packet) {
            if (packet->getPayload()[0] == 0x00) {
                _authenticated = true;
                _authState = IDLE;
            }
            delete packet;
        }
        delete enc_packet;
    }
}

bool EcoflowESP32::_decryptAndParseNotification(const uint8_t* pData, size_t length) {
  if (length < 6) return false;

  // Check for EncPacket prefix (0x5a 0x5a)
  if (pData[0] != 0x5a || pData[1] != 0x5a) {
    return true;
  }

  // Decrypt EncPacket
  EncPacket* pEncPacket = EncPacket::fromBytes(pData, length, _sessionKey, _sessionIV);
  if (!pEncPacket) {
    Serial.println(">>> [PARSE] ERROR: Failed to decrypt EncPacket");
    return false;
  }

  const auto& decrypted = pEncPacket->getPayload();

  // Parse inner Packet (0xaa)
  if (decrypted.size() > 0 && decrypted[0] == 0xaa) {
    Packet* pPkt = Packet::fromBytes(decrypted.data(), decrypted.size());
    if (pPkt) {
      if (pPkt->getCmdId() == PACKET_CMD_ID_REQUEST_STATUS) {
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

        }
      }
      delete pPkt;
    }
  }
  delete pEncPacket;
  return true;
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
