#include "EcoflowESP32.h"
#include "EcoflowProtocol.h"
#include "EcoflowDataParser.h"
#include <NimBLEDevice.h>

EcoflowESP32* EcoflowESP32::_instance = nullptr;

void EcoflowESP32::notifyCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    if (_instance) {
        std::vector<Packet> packets = EncPacket::parsePackets(pData, length, _instance->_crypto);
        for (auto &packet : packets) {
            _instance->_handlePacket(&packet);
        }
    }
}

EcoflowClientCallback::EcoflowClientCallback(EcoflowESP32* instance) : _instance(instance) {}

void EcoflowClientCallback::onConnect(NimBLEClient* pClient) {
    if (_instance) {
        _instance->onConnect(pClient);
    }
}

void EcoflowClientCallback::onDisconnect(NimBLEClient* pClient) {
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
        _instance->_pScan->stop();
        _instance->_pAdvertisedDevice = advertisedDevice;
    }
}

void EcoflowESP32::_startScan() {
    _state = ConnectionState::SCANNING;
    if (!_pScan) {
        _pScan = NimBLEDevice::getScan();
        _pScan->setAdvertisedDeviceCallbacks(new AdvertisedDeviceCallbacks(this));
        _pScan->setActiveScan(true);
        _pScan->setInterval(100);
        _pScan->setWindow(99);
    }
    _pScan->start(30, false);
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
    _state = ConnectionState::CONNECTED;
    NimBLERemoteService* pSvc = pClient->getService("00000001-0000-1000-8000-00805f9b34fb");
    if (pSvc) {
        _pWriteChr = pSvc->getCharacteristic("00000002-0000-1000-8000-00805f9b34fb");
        _pReadChr = pSvc->getCharacteristic("00000003-0000-1000-8000-00805f9b34fb");
        if (_pReadChr && _pReadChr->canNotify()) {
            _pReadChr->subscribe(true, notifyCallback);
        }
    }
    _startAuthentication();
}

void EcoflowESP32::onDisconnect(NimBLEClient* pClient) {
    _state = ConnectionState::DISCONNECTED;
    _pAdvertisedDevice = nullptr;
}

void EcoflowESP32::update() {
    if (_state == ConnectionState::DISCONNECTED) {
        if (_pAdvertisedDevice && millis() - _lastConnectionAttempt > 5000) {
            _lastConnectionAttempt = millis();
            if (_connectionRetries < 5) {
                if (_pClient->connect(_pAdvertisedDevice)) {
                    _state = ConnectionState::ESTABLISHING_CONNECTION;
                    _connectionRetries++;
                }
            } else {
                _pAdvertisedDevice = nullptr;
                _connectionRetries = 0;
            }
        } else if (!_pAdvertisedDevice) {
            _startScan();
        }
    } else if (_state == ConnectionState::SCANNING) {
        if (_pScan->isScanning() == false) {
            if (!_pAdvertisedDevice) {
                _startScan();
            }
        }
    } else if (_state > ConnectionState::CONNECTED && _state < ConnectionState::AUTHENTICATED) {
        if (millis() - _lastConnectionAttempt > 10000) {
            _pClient->disconnect();
        }
    } else if (_state == ConnectionState::AUTHENTICATED) {
        if (millis() - _lastKeepAliveTime > 5000) {
            _lastKeepAliveTime = millis();
            requestData();
        }
    }
}

void EcoflowESP32::_startAuthentication() {
    _state = ConnectionState::PUBLIC_KEY_EXCHANGE;
    _crypto.generate_keys();

    std::vector<uint8_t> payload;
    payload.push_back(0x01);
    payload.push_back(0x00);
    uint8_t* pub_key = _crypto.get_public_key();
    payload.insert(payload.end(), pub_key, pub_key + 41);

    EncPacket enc_packet(EncPacket::FRAME_TYPE_COMMAND, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, payload);
    _sendCommand(enc_packet.toBytes());
}

void EcoflowESP32::_handlePacket(Packet* pkt) {
    if (_state == ConnectionState::AUTHENTICATED) {
        EcoflowDataParser::parsePacket(*pkt, _data);
        if (pkt->getDest() == 0x21 && pkt->getCmdSet() != 0x01 && pkt->getCmdId() != 0x01) {
            Packet reply(pkt->getDest(), pkt->getSrc(), pkt->getCmdSet(), pkt->getCmdId(), pkt->getPayload(), 0x01, 0x01, pkt->getVersion(), pkt->getSeq());
            EncPacket enc_reply(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, reply.toBytes());
            _sendCommand(enc_reply.toBytes(&_crypto));
        }
    } else {
        _handleAuthPacket(pkt);
    }
}

void EcoflowESP32::_handleAuthPacket(Packet* pkt) {
    const auto& payload = pkt->getPayload();
    if (_state == ConnectionState::PUBLIC_KEY_EXCHANGE) {
        if (pkt->getCmdId() == 0x01 && payload.size() >= 43) {
            print_hex(payload.data() + 2, 41, "Peer Public Key");
            if (_crypto.compute_shared_secret(payload.data() + 2, 41)) {
                _state = ConnectionState::REQUESTING_SESSION_KEY;
                std::vector<uint8_t> req_payload = {0x02};
                EncPacket enc_packet(EncPacket::FRAME_TYPE_COMMAND, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, req_payload);
                _sendCommand(enc_packet.toBytes());
            }
        }
    } else if (_state == ConnectionState::REQUESTING_SESSION_KEY) {
        if (pkt->getCmdId() == 0x02 && payload.size() >= 18) {
            std::vector<uint8_t> decrypted_payload(payload.size() - 1);
            mbedtls_aes_context aes_ctx;
            mbedtls_aes_init(&aes_ctx);
            mbedtls_aes_setkey_dec(&aes_ctx, _crypto.get_shared_secret(), 128);
            mbedtls_aes_crypt_cbc(&aes_ctx, MBEDTLS_AES_DECRYPT, payload.size() - 1, _crypto.get_iv(), payload.data() + 1, decrypted_payload.data());
            mbedtls_aes_free(&aes_ctx);

            uint8_t padding = decrypted_payload.back();
            if (padding > 0 && padding <= 16) {
                decrypted_payload.resize(decrypted_payload.size() - padding);
            }
            print_hex(decrypted_payload.data(), decrypted_payload.size(), "Decrypted Payload");

            _crypto.generate_session_key(decrypted_payload.data() + 16, decrypted_payload.data());

            _state = ConnectionState::AUTHENTICATING;

            uint8_t md5_data[16];
            mbedtls_md5((const unsigned char*)(_userId + _deviceSn).c_str(), _userId.length() + _deviceSn.length(), md5_data);
            char hex_data[33];
            for(int i=0; i<16; i++) {
                sprintf(&hex_data[i*2], "%02X", md5_data[i]);
            }
            hex_data[32] = 0;
            std::vector<uint8_t> auth_payload(hex_data, hex_data + 32);

            Packet auth_pkt(0x21, 0x35, 0x35, 0x86, auth_payload);
            EncPacket enc_auth(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, auth_pkt.toBytes());
            _sendCommand(enc_auth.toBytes(&_crypto));
        }
    } else if (_state == ConnectionState::AUTHENTICATING) {
        if (pkt->getCmdSet() == 0x35 && pkt->getCmdId() == 0x86 && payload[0] == 0x00) {
            _state = ConnectionState::AUTHENTICATED;
        }
    }
}

bool EcoflowESP32::_sendCommand(const std::vector<uint8_t>& command) {
    if (_pWriteChr && isConnected()) {
        _pWriteChr->writeValue(command.data(), command.size(), true);
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
    Packet packet(0x21, 0x35, 0x35, 0x01, {});
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
