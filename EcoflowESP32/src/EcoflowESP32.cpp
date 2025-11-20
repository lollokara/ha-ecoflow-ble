#include "EcoflowESP32.h"
#include "EcoflowProtocol.h"
#include "EcoflowDataParser.h"
#include <NimBLEDevice.h>
#include "esp_log.h"

static const char* TAG = "EcoflowESP32";

// Helper to print byte arrays for debugging
static void print_hex_esp(const uint8_t* data, size_t size, const char* label) {
    if (size == 0) return;
    char hex_str[size * 3 + 1];
    for (size_t i = 0; i < size; i++) {
        sprintf(hex_str + i * 3, "%02x ", data[i]);
    }
    hex_str[size * 3] = '\0';
    ESP_LOGD(TAG, "%s: %s", label, hex_str);
}


EcoflowESP32* EcoflowESP32::_instance = nullptr;

void EcoflowESP32::notifyCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    ESP_LOGI(TAG, "Notify callback received %d bytes", length);
    print_hex_esp(pData, length, "Notify Raw Data");
    if (_instance) {
        if (_instance->_state == ConnectionState::PUBLIC_KEY_EXCHANGE || _instance->_state == ConnectionState::REQUESTING_SESSION_KEY) {
             ESP_LOGD(TAG, "Routing notification to simple auth handler");
            std::vector<uint8_t> data(pData, pData + length);
            _instance->_handleSimpleAuthResponse(data);
        } else {
            ESP_LOGD(TAG, "Routing notification to packet parser");
            std::vector<Packet> packets = EncPacket::parsePackets(pData, length, _instance->_crypto, _instance->isAuthenticated());
            ESP_LOGD(TAG, "Parsed %d packets from notification", packets.size());
            for (auto &packet : packets) {
                _instance->_handlePacket(&packet);
            }
        }
    }
}

EcoflowClientCallback::EcoflowClientCallback(EcoflowESP32* instance) : _instance(instance) {}

void EcoflowClientCallback::onConnect(NimBLEClient* pClient) {
    ESP_LOGI(TAG, "Connected to device");
    if (_instance) {
        _instance->onConnect(pClient);
    }
}

void EcoflowClientCallback::onDisconnect(NimBLEClient* pClient) {
    ESP_LOGI(TAG, "Disconnected from device");
    if (_instance) {
        _instance->onDisconnect(pClient);
    }
}

EcoflowESP32::EcoflowESP32() {
    _instance = this;
    _clientCallback = new EcoflowClientCallback(this);
}

EcoflowESP32::~EcoflowESP32() {
    if (_pClient) {
        NimBLEDevice::deleteClient(_pClient);
    }
    delete _clientCallback;
}

EcoflowESP32::AdvertisedDeviceCallbacks::AdvertisedDeviceCallbacks(EcoflowESP32* instance) : _instance(instance) {}

void EcoflowESP32::AdvertisedDeviceCallbacks::onResult(NimBLEAdvertisedDevice* advertisedDevice) {
    if (advertisedDevice->getAddress().toString() == _instance->_ble_address) {
        ESP_LOGI(TAG, "Found device");
        _instance->_pScan->stop();
        _instance->_pAdvertisedDevice = new NimBLEAdvertisedDevice(*advertisedDevice);
    }
}

void EcoflowESP32::_startScan() {
    if (millis() - _lastScanTime > 10000) {
        _lastScanTime = millis();
        _state = ConnectionState::SCANNING;
        if (!_pScan) {
            _pScan = NimBLEDevice::getScan();
            _pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks(this), true);
            _pScan->setActiveScan(true);
            _pScan->setInterval(100);
            _pScan->setWindow(99);
        }
        _pScan->start(5, false);
    }
}

bool EcoflowESP32::begin(const std::string& userId, const std::string& deviceSn, const std::string& ble_address) {
    _userId = userId;
    _deviceSn = deviceSn;
    _ble_address = ble_address;
    NimBLEDevice::init("");
    _pClient = NimBLEDevice::createClient();
    _pClient->setClientCallbacks(_clientCallback);
    _startScan();
    return true;
}

void EcoflowESP32::onConnect(NimBLEClient* pClient) {
    _connectionRetries = 0;
    _state = ConnectionState::CONNECTED;
    ESP_LOGI("EcoflowESP32", "onConnect: State changed to CONNECTED, performing service discovery");
    NimBLERemoteService* pSvc = pClient->getService("00000001-0000-1000-8000-00805f9b34fb");
    if (pSvc) {
        _pWriteChr = pSvc->getCharacteristic("00000002-0000-1000-8000-00805f9b34fb");
        _pReadChr = pSvc->getCharacteristic("00000003-0000-1000-8000-00805f9b34fb");
        if (_pReadChr && _pReadChr->canNotify()) {
            if(_pReadChr->subscribe(true, notifyCallback)) {
                ESP_LOGI(TAG, "Subscribed to notifications");
                delay(100); // Wait for the subscription to be processed
            } else {
                ESP_LOGE(TAG, "Failed to subscribe to notifications");
                pClient->disconnect();
            }
        }
    } else {
        ESP_LOGE("EcoflowESP32", "onConnect: Service not found, disconnecting");
        pClient->disconnect();
    }
}

void EcoflowESP32::onDisconnect(NimBLEClient* pClient) {
    _state = ConnectionState::DISCONNECTED;
    delete _pAdvertisedDevice;
    _pAdvertisedDevice = nullptr;
}

void EcoflowESP32::update() {
    if (_state != _lastState) {
        ESP_LOGI(TAG, "State changed: %d", (int)_state);
        _lastState = _state;
    }

    if (_pAdvertisedDevice) {
        if (!_pClient->isConnected()) {
            if (millis() - _lastConnectionAttempt > 5000) {
                _lastConnectionAttempt = millis();
                if (_connectionRetries < 5) {
                    ESP_LOGI(TAG, "Connecting...");
                    _state = ConnectionState::ESTABLISHING_CONNECTION;
                    _connectionRetries++;
                    _pClient->connect(_pAdvertisedDevice);
                } else {
                    delete _pAdvertisedDevice;
                    _pAdvertisedDevice = nullptr;
                    _connectionRetries = 0;
                }
            }
        }
    } else {
        _startScan();
    }

    if (_state == ConnectionState::CONNECTED && _pClient->isConnected()) {
        _startAuthentication();
    }
    else if (_state > ConnectionState::CONNECTED && _state < ConnectionState::AUTHENTICATED) {
        if (millis() - _lastConnectionAttempt > 10000) {
            _pClient->disconnect();
        }
    } else if (_state == ConnectionState::AUTHENTICATED) {
        if (millis() - _lastKeepAliveTime > 5000) {
            _lastKeepAliveTime = millis();
            ESP_LOGI(TAG, "Authenticated. Requesting data...");
            requestData();
        }
    }
}

void EcoflowESP32::_startAuthentication() {
    ESP_LOGI(TAG, "Starting authentication");
    _state = ConnectionState::PUBLIC_KEY_EXCHANGE;
    Packet::reset_sequence();
    if (!_crypto.generate_keys()) {
        ESP_LOGE(TAG, "Failed to generate keys");
        return;
    }

    std::vector<uint8_t> payload;
    payload.push_back(0x01);
    payload.push_back(0x00);
    uint8_t* pub_key = _crypto.get_public_key();
    payload.insert(payload.end(), pub_key, pub_key + _crypto.get_public_key_len());

    print_hex_esp(payload.data(), payload.size(), "Public Key Payload");

    EncPacket enc_packet(EncPacket::FRAME_TYPE_COMMAND, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, payload);
    _sendCommand(enc_packet.toBytes());
}

void EcoflowESP32::_handleSimpleAuthResponse(const std::vector<uint8_t>& data) {
    ESP_LOGD(TAG, "_handleSimpleAuthResponse: Handling simple auth response");
    print_hex_esp(data.data(), data.size(), "Raw Simple Auth Response");
    auto parsed_payload = EncPacket::parseSimple(data.data(), data.size());
    if (parsed_payload.empty()) {
        ESP_LOGE(TAG, "Failed to parse simple auth response");
        return;
    }
    print_hex_esp(parsed_payload.data(), parsed_payload.size(), "Parsed Simple Auth Response");

    if (_state == ConnectionState::PUBLIC_KEY_EXCHANGE) {
        ESP_LOGD(TAG, "Handling public key response");
        if (parsed_payload.size() >= 43 && parsed_payload[0] == 0x01) { // 1 (status) + 1 (type) + 1 (size) + 40 (key)
            uint8_t peer_pub_key[41];
            peer_pub_key[0] = 0x04;
            memcpy(peer_pub_key + 1, parsed_payload.data() + 3, 40);
            print_hex_esp(peer_pub_key, sizeof(peer_pub_key), "Peer Public Key");

            if (_crypto.compute_shared_secret(peer_pub_key, sizeof(peer_pub_key))) {
                _state = ConnectionState::REQUESTING_SESSION_KEY;
                ESP_LOGD(TAG, "Shared secret computed, requesting session key");
                std::vector<uint8_t> req_payload = {0x02};
                EncPacket enc_packet(EncPacket::FRAME_TYPE_COMMAND, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, req_payload);
                _sendCommand(enc_packet.toBytes());
            } else {
                ESP_LOGE(TAG, "Failed to compute shared secret");
            }
        } else {
            ESP_LOGE(TAG, "Invalid public key response");
        }
    } else if (_state == ConnectionState::REQUESTING_SESSION_KEY) {
        ESP_LOGD(TAG, "Handling session key response");
        if (parsed_payload.size() > 1 && parsed_payload[0] == 0x02) {
            std::vector<uint8_t> decrypted_payload(parsed_payload.size() - 1);
            _crypto.decrypt_shared(parsed_payload.data() + 1, parsed_payload.size() - 1, decrypted_payload.data());

            // Remove PKCS7 padding
            if (!decrypted_payload.empty()) {
                uint8_t padding = decrypted_payload.back();
                if (padding > 0 && padding <= 16 && decrypted_payload.size() >= padding) {
                    decrypted_payload.resize(decrypted_payload.size() - padding);
                }
            }
            print_hex_esp(decrypted_payload.data(), decrypted_payload.size(), "Decrypted Session Key Info");

            if (decrypted_payload.size() >= 18) {
                _crypto.generate_session_key(decrypted_payload.data() + 16, decrypted_payload.data());
                _state = ConnectionState::REQUESTING_AUTH_STATUS;
                ESP_LOGD(TAG, "Session key generated, requesting auth status");
                Packet auth_status_pkt(0x21, 0x35, 0x35, 0x89, {}, 0x01, 0x01, 0x03);
                EncPacket enc_auth_status(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, auth_status_pkt.toBytes());
                _sendCommand(enc_auth_status.toBytes(&_crypto));
            } else {
                ESP_LOGE(TAG, "Decrypted session key info too short");
            }
        }
    }
}

void EcoflowESP32::_handlePacket(Packet* pkt) {
    ESP_LOGD(TAG, "_handlePacket: Handling packet with cmdSet=0x%02x, cmdId=0x%02x", pkt->getCmdSet(), pkt->getCmdId());
    if (_state == ConnectionState::AUTHENTICATED) {
        ESP_LOGI(TAG, "Packet received while authenticated. Routing to data parser.");
        EcoflowDataParser::parsePacket(*pkt, _data);
        ESP_LOGD(TAG, "Checking if reply is needed for packet: dest=0x%02x", pkt->getDest());
        if (pkt->getDest() == 0x21) {
            ESP_LOGD(TAG, "Replying to packet with cmdSet=0x%02x, cmdId=0x%02x", pkt->getCmdSet(), pkt->getCmdId());
            Packet reply(pkt->getDest(), pkt->getSrc(), pkt->getCmdSet(), pkt->getCmdId(), pkt->getPayload(), 0x01, 0x01, pkt->getVersion(), pkt->getSeq());
            EncPacket enc_reply(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, reply.toBytes());
            _sendCommand(enc_reply.toBytes(&_crypto));
        }
    } else {
        ESP_LOGD(TAG, "Packet received while not authenticated. Routing to auth handler.");
        _handleAuthPacket(pkt);
    }
}

void EcoflowESP32::_handleAuthPacket(Packet* pkt) {
    ESP_LOGD(TAG, "_handleAuthPacket: Handling auth packet with cmdId=0x%02x", pkt->getCmdId());
    const auto& payload = pkt->getPayload();

    if (_state == ConnectionState::REQUESTING_AUTH_STATUS) {
        ESP_LOGD(TAG, "Handling auth status response");
        if (pkt->getCmdSet() == 0x35 && pkt->getCmdId() == 0x89) {
            _state = ConnectionState::AUTHENTICATING;
            ESP_LOGD(TAG, "Auth status OK, authenticating");

            uint8_t md5_data[16];
            mbedtls_md5((const unsigned char*)(_userId + _deviceSn).c_str(), _userId.length() + _deviceSn.length(), md5_data);
            char hex_data[33];
            for(int i=0; i<16; i++) {
                sprintf(&hex_data[i*2], "%02X", md5_data[i]);
            }
            hex_data[32] = 0;
            std::vector<uint8_t> auth_payload(hex_data, hex_data + 32);

            Packet auth_pkt(0x21, 0x35, 0x35, 0x86, auth_payload, 0x01, 0x01, 0x03);
            EncPacket enc_auth(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, auth_pkt.toBytes());
            _sendCommand(enc_auth.toBytes(&_crypto));
        }
    } else if (_state == ConnectionState::AUTHENTICATING) {
        ESP_LOGD(TAG, "Handling authentication response");
        if (pkt->getCmdSet() == 0x35 && pkt->getCmdId() == 0x86 && payload.size() > 0 && payload[0] == 0x00) {
            _state = ConnectionState::AUTHENTICATED;
            ESP_LOGI(TAG, "Authentication successful!");
        } else {
            ESP_LOGE(TAG, "Authentication failed!");
        }
    }
}

bool EcoflowESP32::_sendCommand(const std::vector<uint8_t>& command) {
    if (_pWriteChr && isConnected()) {
        print_hex_esp(command.data(), command.size(), "Sending command");
        _pWriteChr->writeValue(command.data(), command.size(), false); // Write without response
        return true;
    }
    return false;
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

bool EcoflowESP32::isConnected() { return _state >= ConnectionState::CONNECTED; }
bool EcoflowESP32::isAuthenticated() { return _state == ConnectionState::AUTHENTICATED; }

bool EcoflowESP32::requestData() {
    if (!isAuthenticated()) return false;
    ESP_LOGD(TAG, "Constructing data request packet (cmd_set=0xFE, cmd_id=0x11)");
    Packet packet(0x01, 0x02, 0xFE, 0x11, {}, 0x01, 0x01, 0x03);
    EncPacket enc_packet(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, packet.toBytes());
    return _sendCommand(enc_packet.toBytes(&_crypto));
}

bool EcoflowESP32::setDC(bool on) {
    if (!isAuthenticated()) return false;
    std::vector<uint8_t> payload = { (uint8_t)(on ? 1 : 0) };
    Packet packet(0x21, 0x35, 0x35, 0x92, payload);
    EncPacket enc_packet(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, packet.toBytes());
    return _sendCommand(enc_packet.toBytes(&_crypto));
}

bool EcoflowESP32::setUSB(bool on) {
    if (!isAuthenticated()) return false;
    std::vector<uint8_t> payload = { (uint8_t)(on ? 1 : 0) };
    Packet packet(0x21, 0x35, 0x35, 0x93, payload);
    EncPacket enc_packet(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, packet.toBytes());
    return _sendCommand(enc_packet.toBytes(&_crypto));
}

bool EcoflowESP32::setAC(bool on) {
    if (!isAuthenticated()) return false;
    std::vector<uint8_t> payload = { (uint8_t)(on ? 1 : 0) };
    Packet packet(0x21, 0x35, 0x35, 0x91, payload);
    EncPacket enc_packet(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, packet.toBytes());
    return _sendCommand(enc_packet.toBytes(&_crypto));
}
