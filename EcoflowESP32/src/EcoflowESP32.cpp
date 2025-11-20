#include "EcoflowESP32.h"
#include "EcoflowProtocol.h"
#include "mbedtls/md5.h"
#include "mbedtls/aes.h"
#include "EcoflowECDH.h"
#include "EcoflowDataParser.h"

// ============================================================================
// Static instance & callbacks
// ============================================================================

EcoflowESP32* EcoflowESP32::_instance = nullptr;
MyClientCallback EcoflowESP32::g_clientCallback;

static const uint8_t CONNECT_RETRIES = 3;
static const uint32_t CONNECT_RETRY_DELAY_MS = 250;
static const uint32_t SERVICE_DISCOVERY_DELAY = 1000;
static const uint32_t KEEPALIVE_INTERVAL_MS = 3000;
static const uint32_t KEEPALIVE_CHECK_MS = 500;
static const uint32_t AUTH_TIMEOUT_MS = 10000;

void keepAliveTask(void* param) {
    EcoflowESP32* pThis = static_cast<EcoflowESP32*>(param);
    Serial.println(">>> Keep-Alive task started");

    uint32_t sendCounter = 0;
    while (pThis && pThis->_running) {
        if (pThis->isAuthenticated() && pThis->_sessionKeyEstablished) {
            uint32_t now = millis();
            uint32_t dt = now - pThis->_lastCommandTime;

            if (dt >= KEEPALIVE_INTERVAL_MS) {
                Serial.print(">>> [KEEP-ALIVE #");
                Serial.print(++sendCounter);
                Serial.println("] Sending encrypted status request...");
                pThis->requestData();
            }
        }
        vTaskDelay(KEEPALIVE_CHECK_MS / portTICK_PERIOD_MS);
    }

    Serial.println(">>> Keep-Alive task stopped");
    vTaskDelete(nullptr);
}


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
        inst->onNotify(pRemoteCharacteristic, pData, length, isNotify);
    }
}

void MyClientCallback::onConnect(NimBLEClient* pClient) {
    EcoflowESP32* inst = EcoflowESP32::getInstance();
    if (inst) inst->onConnect(pClient);
}

void MyClientCallback::onDisconnect(NimBLEClient* pClient) {
    EcoflowESP32* inst = EcoflowESP32::getInstance();
    if (inst) inst->onDisconnect(pClient);
}

// ============================================================================
// Constructor / destructor
// ============================================================================

EcoflowESP32::EcoflowESP32() {
    _instance = this;
    memset(_sessionKey, 0, 16);
    memset(_sessionIV, 0, 16);
    memset(_private_key, 0, sizeof(_private_key));
    memset(_shared_key, 0, sizeof(_shared_key));
    memset(_iv, 0, sizeof(_iv));
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
        NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
        NimBLEDevice::setSecurityAuth(false, false, false);
        NimBLEDevice::setMTU(247);
        Serial.println(">>> Security settings configured");
        _running = true;
        return true;
    } catch (...) {
        Serial.println(">>> ERROR during BLE init");
        return false;
    }
}

void EcoflowESP32::setCredentials(const std::string& userId, const std::string& deviceSn) {
    _userId = userId;
    _deviceSn = deviceSn;
}

bool EcoflowESP32::scan(uint32_t scanTime) {
    if (m_pAdvertisedDevice) {
        delete m_pAdvertisedDevice;
        m_pAdvertisedDevice = nullptr;
    }

    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setActiveScan(true);
    pScan->setInterval(100);
    pScan->setWindow(99);

    NimBLEScanResults results = pScan->start(scanTime, false);

    for (int i = 0; i < results.getCount(); i++) {
        NimBLEAdvertisedDevice device = results.getDevice(i);
        if (device.haveManufacturerData() && device.getManufacturerData().length() > 2) {
            uint16_t manufacturerId = (device.getManufacturerData()[1] << 8) | device.getManufacturerData()[0];
            if (manufacturerId == 0xB5B5) {
                m_pAdvertisedDevice = new NimBLEAdvertisedDevice(device);
                return true;
            }
        }
    }
    return false;
}

bool EcoflowESP32::connectToServer() {
    if (!m_pAdvertisedDevice) {
        return false;
    }

    if (pClient) {
        pClient->disconnect();
        NimBLEDevice::deleteClient(pClient);
        pClient = nullptr;
    }

    pClient = NimBLEDevice::createClient();
    pClient->setClientCallbacks(&g_clientCallback, false);

    _setState(ConnectionState::CONNECTING);
    if (pClient->connect(m_pAdvertisedDevice)) {
        return true;
    }
    return false;
}

void EcoflowESP32::disconnect() {
    _running = false;
    _stopKeepAliveTask();

    if (pClient && pClient->isConnected()) {
        pClient->disconnect();
    }

    if (pClient) {
        NimBLEDevice::deleteClient(pClient);
        pClient = nullptr;
    }

    pWriteChr = nullptr;
    pReadChr = nullptr;
    _setState(ConnectionState::DISCONNECTED);
}

void EcoflowESP32::update() {
    // Placeholder for periodic updates
}

int EcoflowESP32::getBatteryLevel() { return _data.batteryLevel; }
int EcoflowESP32::getInputPower() { return _data.inputPower; }
int EcoflowESP32::getOutputPower() { return _data.outputPower; }
int EcoflowESP32::getBatteryVoltage() { return _data.batteryVoltage; }
int EcoflowESP32::getACVoltage() { return _data.acVoltage; }
int EcoflowESP32::getACFrequency() { return _data.acFrequency; }
bool EcoflowESP32::isAcOn() { return _data.acOn; }
bool EcoflowESP32::isDcOn() { return _data.dcOn; }
bool EcoflowESP32::isUsbOn() { return _data.usbOn; }


bool EcoflowESP32::isConnected() {
    return _state >= ConnectionState::CONNECTED && _state < ConnectionState::DISCONNECTED;
}

bool EcoflowESP32::isConnecting() {
    return _state == ConnectionState::CONNECTING;
}

bool EcoflowESP32::isAuthenticated() {
    return _state == ConnectionState::AUTHENTICATED;
}

bool EcoflowESP32::requestData() {
    if (!isAuthenticated()) return false;
    auto cmd = EcoflowCommands::buildStatusRequest(_sessionKey, _sessionIV);
    return _sendCommand(cmd);
}

bool EcoflowESP32::setAC(bool on) {
    if (!isAuthenticated()) return false;
    auto cmd = EcoflowCommands::buildAcCommand(on, _sessionKey, _sessionIV);
    return _sendCommand(cmd);
}

bool EcoflowESP32::setDC(bool on) {
    if (!isAuthenticated()) return false;
    auto cmd = EcoflowCommands::buildDcCommand(on, _sessionKey, _sessionIV);
    return _sendCommand(cmd);
}

bool EcoflowESP32::setUSB(bool on) {
    if (!isAuthenticated()) return false;
    auto cmd = EcoflowCommands::buildUsbCommand(on, _sessionKey, _sessionIV);
    return _sendCommand(cmd);
}

// ============================================================================
// State Machine and Callbacks
// ============================================================================

void EcoflowESP32::_setState(ConnectionState newState) {
    _state = newState;
}

void EcoflowESP32::onConnect(NimBLEClient* pclient) {
    _setState(ConnectionState::CONNECTED);
    if(_resolveCharacteristics()) {
        _startAuthentication();
    }
}

void EcoflowESP32::onDisconnect(NimBLEClient* pclient) {
    _setState(ConnectionState::DISCONNECTED);
    _sessionKeyEstablished = false;
    _stopKeepAliveTask();
}

void EcoflowESP32::onNotify(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    _notificationReceived = true;

    // During auth, packets are not encrypted with session key
    bool is_auth_step = _state < ConnectionState::AUTHENTICATED;
    const uint8_t* key = is_auth_step ? nullptr : _sessionKey;
    const uint8_t* iv = is_auth_step ? nullptr : _sessionIV;

    EncPacket* encPkt = EncPacket::fromBytes(pData, length, key, iv);
    if (!encPkt) {
        if (is_auth_step) {
            // for auth, we can receive simple packets, let's try to parse them
            auto payload = EncPacket::parseSimple(pData, length);
            if(payload.empty()){
                Serial.println(">>> [ERROR] Received invalid simple packet");
                return;
            }
            Packet* pkt = Packet::fromBytes(payload.data(), payload.size());
            if(!pkt){
                Serial.println(">>> [ERROR] Failed to parse inner packet from simple packet");
                return;
            }
            _handlePacket(pkt);
            delete pkt;
        } else {
            Serial.println(">>> [ERROR] Received invalid encrypted packet");
        }
        return;
    }

    Packet* pkt = Packet::fromBytes(encPkt->getPayload().data(), encPkt->getPayload().size());
    if(!pkt){
        Serial.println(">>> [ERROR] Failed to parse inner packet");
        delete encPkt;
        return;
    }
    _handlePacket(pkt);
    delete encPkt;
    delete pkt;
}


void EcoflowESP32::_handlePacket(Packet* pkt) {
    switch (_state) {
        case ConnectionState::PUBLIC_KEY_SENT:
            if (pkt->getCmdSet() == 2 && pkt->getCmdId() == 1) {
                _handlePublicKeyExchange(pkt->getPayload());
            }
            break;
        case ConnectionState::SESSION_KEY_REQUESTED:
            if (pkt->getCmdSet() == 2 && pkt->getCmdId() == 2) {
                _handleSessionKeyResponse(pkt->getPayload());
            }
            break;
        case ConnectionState::AUTH_STATUS_REQUESTED:
             if (pkt->getCmdSet() == 0x35 && pkt->getCmdId() == 0x89) {
                _handleAuthStatusResponse(pkt->getPayload());
            }
            break;
        case ConnectionState::AUTHENTICATING:
            if (pkt->getCmdSet() == 0x35 && pkt->getCmdId() == 0x86) {
                _handleAuthResponse(pkt->getPayload());
            }
            break;
        case ConnectionState::AUTHENTICATED:
            _handleDataNotification(pkt);
            break;
        default:
            Serial.println(">>> [WARN] Notification received in unexpected state");
            break;
    }
}

// ============================================================================
// Authentication Flow
// ============================================================================

bool EcoflowESP32::_startAuthentication() {
    _setState(ConnectionState::PUBLIC_KEY_EXCHANGE);
    _sendPublicKey();
    return true;
}

void EcoflowESP32::_sendPublicKey() {
    uint8_t pubKey[40];
    if (EcoflowECDH::generate_public_key(pubKey, _private_key)) {
        auto cmd = EcoflowCommands::buildPublicKey(pubKey, sizeof(pubKey));
        _sendCommand(cmd);
        _setState(ConnectionState::PUBLIC_KEY_SENT);
    } else {
        Serial.println(">>> [ERROR] Failed to generate public key");
        disconnect();
    }
}

void EcoflowESP32::_handlePublicKeyExchange(const std::vector<uint8_t>& payload) {
    if (!payload.empty()) {
        if (EcoflowECDH::compute_shared_secret(payload.data(), _shared_key, _private_key)) {
            mbedtls_md5(_shared_key, 20, _iv);
            memcpy(_sessionIV, _iv, 16); // Use the same IV for the session
            _requestSessionKey();
        } else {
            Serial.println(">>> [ERROR] Failed to compute shared secret");
            disconnect();
        }
    }
}

void EcoflowESP32::_requestSessionKey() {
    auto cmd = EcoflowCommands::buildSessionKeyRequest();
    _sendCommand(cmd);
    _setState(ConnectionState::SESSION_KEY_REQUESTED);
}

void EcoflowESP32::_handleSessionKeyResponse(const std::vector<uint8_t>& payload) {
    if (payload.size() >= 17) { // 1 byte something + 16 bytes seed
        EcoflowECDH::generateSessionKey(payload.data() + 16, payload.data(), _sessionKey);
        _sessionKeyEstablished = true;
        _requestAuthStatus();
    } else {
        Serial.println(">>> [ERROR] Session key response too short");
        disconnect();
    }
}

void EcoflowESP32::_requestAuthStatus() {
    auto cmd = EcoflowCommands::buildAuthStatusRequest(_sessionKey, _sessionIV);
    _sendCommand(cmd);
    _setState(ConnectionState::AUTH_STATUS_REQUESTED);
}

void EcoflowESP32::_handleAuthStatusResponse(const std::vector<uint8_t>& payload) {
    // Nothing to do with the response, just proceed to send credentials
    _sendAuthCredentials();
}

void EcoflowESP32::_sendAuthCredentials() {
    auto cmd = EcoflowCommands::buildAuthentication(_userId, _deviceSn, _sessionKey, _sessionIV);
    _sendCommand(cmd);
    _setState(ConnectionState::AUTHENTICATING);
}

void EcoflowESP32::_handleAuthResponse(const std::vector<uint8_t>& payload) {
    if (payload.size() > 0 && payload[0] == 0) {
        _setState(ConnectionState::AUTHENTICATED);
        _startKeepAliveTask();
        Serial.println(">>> Authentication successful!");
    } else {
        Serial.println(">>> Authentication failed!");
        disconnect();
    }
}

void EcoflowESP32::_handleDataNotification(Packet* pkt) {
    EcoflowDataParser::parsePacket(*pkt, _data);
}

// ============================================================================
// Private Helpers
// ============================================================================

bool EcoflowESP32::_resolveCharacteristics() {
    if (!pClient) {
        return false;
    }

    NimBLERemoteService* pSvc = pClient->getService("00000001-0000-1000-8000-00805f9b34fb");
    if (!pSvc) {
        return false;
    }

    pWriteChr = pSvc->getCharacteristic("00000002-0000-1000-8000-00805f9b34fb");
    pReadChr = pSvc->getCharacteristic("00000003-0000-1000-8000-00805f9b34fb");

    if(pReadChr) {
        pReadChr->subscribe(true, handleNotificationCallback, true);
    }

    return pWriteChr && pReadChr;
}

void EcoflowESP32::_startKeepAliveTask() {
    if (_keepAliveTaskHandle) {
        vTaskDelete(_keepAliveTaskHandle);
        _keepAliveTaskHandle = nullptr;
    }
    xTaskCreate(keepAliveTask, "keepAlive", 4096, this, 5, &_keepAliveTaskHandle);
}

void EcoflowESP32::_stopKeepAliveTask() {
    if (_keepAliveTaskHandle) {
        vTaskDelete(_keepAliveTaskHandle);
        _keepAliveTaskHandle = nullptr;
    }
}

bool EcoflowESP32::_sendCommand(const std::vector<uint8_t>& packet) {
    if (!pWriteChr || !isConnected()) {
        return false;
    }
    pWriteChr->writeValue(packet.data(), packet.size(), true);
    _lastCommandTime = millis();
    return true;
}