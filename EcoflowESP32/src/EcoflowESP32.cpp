#include "EcoflowESP32.h"
#include "EcoflowProtocol.h"
#include "EcoflowDataParser.h"
#include <NimBLEDevice.h>
#include "esp_log.h"
#include "pd335_sys.pb.h"
#include <pb_encode.h>

static const char* TAG = "EcoflowESP32";

// Helper to print byte arrays for debugging
static void print_hex_esp(const uint8_t* data, size_t size, const char* label) {
    if (size == 0) return;
    char hex_str[size * 3 + 1];
    for (size_t i = 0; i < size; i++) {
        sprintf(hex_str + i * 3, "%02x ", data[i]);
    }
    hex_str[size * 3] = '\0';
    ESP_LOGV(TAG, "%s: %s", label, hex_str);
}


std::vector<EcoflowESP32*> EcoflowESP32::_instances;

void EcoflowESP32::notifyCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    ESP_LOGV(TAG, "Notify callback received %d bytes", length);
    print_hex_esp(pData, length, "Notify Data");

    NimBLEClient* pClient = pRemoteCharacteristic->getRemoteService()->getClient();
    for (auto* instance : _instances) {
        if (instance->_pClient == pClient && instance->_ble_queue) {
            BleNotification* notification = new BleNotification;
            notification->data = new uint8_t[length];
            memcpy(notification->data, pData, length);
            notification->length = length;
            xQueueSend(instance->_ble_queue, &notification, portMAX_DELAY);
            return;
        }
    }
    ESP_LOGW(TAG, "Notification received but no matching instance found");
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
    _instances.push_back(this);
    _clientCallback = new EcoflowClientCallback(this);
}

EcoflowESP32::~EcoflowESP32() {
    if (_pClient) {
        NimBLEDevice::deleteClient(_pClient);
    }
    delete _clientCallback;

    for (auto it = _instances.begin(); it != _instances.end(); ++it) {
        if (*it == this) {
            _instances.erase(it);
            break;
        }
    }
}

bool EcoflowESP32::begin(const std::string& userId, const std::string& deviceSn, const std::string& ble_address, uint8_t protocolVersion) {
    _userId = userId;
    _deviceSn = deviceSn;
    _ble_address = ble_address;
    _protocolVersion = protocolVersion;

    if (!_pClient) {
        _pClient = NimBLEDevice::createClient();
        _pClient->setClientCallbacks(_clientCallback);
    }

    if (!_ble_queue) {
        _ble_queue = xQueueCreate(10, sizeof(BleNotification*));
    }

    if (!_ble_task_handle) {
        // Increased stack size to prevent potential overflow
        xTaskCreate(ble_task_entry, "ble_task", 12288, this, 5, &_ble_task_handle);
    }
    return true;
}

void EcoflowESP32::connectTo(NimBLEAdvertisedDevice* device) {
    if (_pAdvertisedDevice) delete _pAdvertisedDevice;
    _pAdvertisedDevice = new NimBLEAdvertisedDevice(*device);
    _state = ConnectionState::CREATED; // Signal task to connect
    _connectionRetries = 0; // Reset retries
}

void EcoflowESP32::onConnect(NimBLEClient* pClient) {
    _connectionRetries = 0;
    _state = ConnectionState::SERVICE_DISCOVERY;
    _lastAuthActivity = millis();
    ESP_LOGI("EcoflowESP32", "onConnect: State changed to SERVICE_DISCOVERY");
}

void EcoflowESP32::disconnectAndForget() {
    if (_pClient && _pClient->isConnected()) {
        _pClient->disconnect();
    }
    _ble_address = "";
    _deviceSn = "";
    _state = ConnectionState::NOT_CONNECTED;
    if (_pAdvertisedDevice) {
        delete _pAdvertisedDevice;
        _pAdvertisedDevice = nullptr;
    }
}

void EcoflowESP32::onDisconnect(NimBLEClient* pClient) {
    _state = ConnectionState::DISCONNECTED;
    if (_pAdvertisedDevice) {
        delete _pAdvertisedDevice;
        _pAdvertisedDevice = nullptr;
    }
}

void EcoflowESP32::update() {
}

void EcoflowESP32::ble_task_entry(void* pvParameters) {
    EcoflowESP32* self = (EcoflowESP32*)pvParameters;
    for (;;) {
        if (self->_state != self->_lastState) {
            ESP_LOGI(TAG, "State changed: %d", (int)self->_state);
            self->_lastState = self->_state;
        }

        if (self->_pAdvertisedDevice) {
            if (!self->_pClient->isConnected()) {
                // Check if we are waiting for a retry delay
                if (millis() - self->_lastConnectionAttempt > 5000) { // 5s retry delay
                    self->_lastConnectionAttempt = millis();
                    if (self->_connectionRetries < MAX_CONNECT_ATTEMPTS) {
                        ESP_LOGI(TAG, "Connecting... (Attempt %d/%d)", self->_connectionRetries + 1, MAX_CONNECT_ATTEMPTS);
                        self->_state = ConnectionState::ESTABLISHING_CONNECTION;
                        self->_connectionRetries++;
                        if(self->_pClient->connect(self->_pAdvertisedDevice)) {
                             // Connect successful, onConnect will be called
                        } else {
                             ESP_LOGE(TAG, "Connect failed");
                        }
                    } else {
                        ESP_LOGE(TAG, "Max connection attempts reached. Waiting for manager.");
                        delete self->_pAdvertisedDevice;
                        self->_pAdvertisedDevice = nullptr;
                        self->_connectionRetries = 0;
                        self->_state = ConnectionState::DISCONNECTED;
                    }
                }
            }
        } else {
            if (self->_state >= ConnectionState::CONNECTED && self->_state < ConnectionState::DISCONNECTED) {
                if (!self->_pClient->isConnected()) {
                     ESP_LOGW(TAG, "Client disconnected unexpectedly");
                     self->onDisconnect(self->_pClient);
                }
            }
        }

        if (self->_pClient->isConnected()) {
            if (self->_state == ConnectionState::SERVICE_DISCOVERY) {
                NimBLERemoteService* pSvc = self->_pClient->getService("00000001-0000-1000-8000-00805f9b34fb");
                if (pSvc) {
                    self->_pWriteChr = pSvc->getCharacteristic("00000002-0000-1000-8000-00805f9b34fb");
                    self->_pReadChr = pSvc->getCharacteristic("00000003-0000-1000-8000-00805f9b34fb");
                    if (self->_pReadChr && self->_pWriteChr) {
                        self->_state = ConnectionState::SUBSCRIBING_NOTIFICATIONS;
                    }
                } else {
                    ESP_LOGE(TAG, "Service not found, disconnecting");
                    self->_pClient->disconnect();
                }
            } else if (self->_state == ConnectionState::SUBSCRIBING_NOTIFICATIONS) {
                if (self->_pReadChr->canNotify()) {
                    if(self->_pReadChr->subscribe(true, notifyCallback)) {
                        ESP_LOGI(TAG, "Subscribed to notifications");
                        self->_state = ConnectionState::CONNECTED;
                    } else {
                        ESP_LOGE(TAG, "Failed to subscribe to notifications");
                        self->_pClient->disconnect();
                    }
                }
            } else if (self->_state == ConnectionState::CONNECTED) {
                self->_startAuthentication();
            } else if (self->_state > ConnectionState::CONNECTED && self->_state < ConnectionState::AUTHENTICATED) {
                if (millis() - self->_lastAuthActivity > 10000) { // New timeout for auth steps
                    ESP_LOGW(TAG, "Authentication timed out");
                    self->_pClient->disconnect();
                }
            } else if (self->_state == ConnectionState::AUTHENTICATED) {
                if (millis() - self->_lastKeepAliveTime > 5000) {
                    self->_lastKeepAliveTime = millis();
                    self->requestData();
                }
            }
        }

        BleNotification* notification;
        if (xQueueReceive(self->_ble_queue, &notification, 0) == pdTRUE) {
            if (self->_state == ConnectionState::PUBLIC_KEY_EXCHANGE) {
                std::vector<uint8_t> data(notification->data, notification->data + notification->length);
                auto parsed_payload = EncPacket::parseSimple(data.data(), data.size());
                if (!parsed_payload.empty() && parsed_payload.size() >= 43 && parsed_payload[0] == 0x01) {
                    uint8_t peer_pub_key[41];
                    peer_pub_key[0] = 0x04;
                    memcpy(peer_pub_key + 1, parsed_payload.data() + 3, 40);
                    if (self->_crypto.compute_shared_secret(peer_pub_key, sizeof(peer_pub_key))) {
                        self->_state = ConnectionState::REQUESTING_SESSION_KEY;
                        ESP_LOGD(TAG, "Shared secret computed, requesting session key");
                        std::vector<uint8_t> req_payload = {0x02};
                        EncPacket enc_packet(EncPacket::FRAME_TYPE_COMMAND, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, req_payload);
                        self->_sendCommand(enc_packet.toBytes());
                    } else {
                        ESP_LOGE(TAG, "Failed to compute shared secret");
                    }
                } else {
                    ESP_LOGE(TAG, "Invalid public key response");
                }
            } else if (self->_state == ConnectionState::REQUESTING_SESSION_KEY) {
                std::vector<uint8_t> data(notification->data, notification->data + notification->length);
                auto parsed_payload = EncPacket::parseSimple(data.data(), data.size());
                if (!parsed_payload.empty() && parsed_payload.size() > 1 && parsed_payload[0] == 0x02) {
                    std::vector<uint8_t> decrypted_payload(parsed_payload.size() - 1);
                    self->_crypto.decrypt_shared(parsed_payload.data() + 1, parsed_payload.size() - 1, decrypted_payload.data());

                    if (!decrypted_payload.empty()) {
                        uint8_t padding = decrypted_payload.back();
                        if (padding > 0 && padding <= 16 && decrypted_payload.size() >= padding) {
                            decrypted_payload.resize(decrypted_payload.size() - padding);
                        }
                    }

                    if (decrypted_payload.size() >= 18) {
                        self->_crypto.generate_session_key(decrypted_payload.data() + 16, decrypted_payload.data());
                        self->_state = ConnectionState::REQUESTING_AUTH_STATUS;
                        ESP_LOGD(TAG, "Session key generated, requesting auth status");
                        Packet auth_status_pkt(0x21, 0x35, 0x35, 0x89, {}, 0x01, 0x01, self->_protocolVersion, self->_txSeq++, 0x0d);
                        EncPacket enc_auth_status(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, auth_status_pkt.toBytes());
                        self->_sendCommand(enc_auth_status.toBytes(&self->_crypto));
                    } else {
                        ESP_LOGE(TAG, "Decrypted session key info too short");
                    }
                }
            } else {
                std::vector<Packet> packets = EncPacket::parsePackets(notification->data, notification->length, self->_crypto, self->_rxBuffer, self->isAuthenticated());
                for (auto &packet : packets) {
                    self->_handlePacket(&packet);
                }
            }
            delete[] notification->data;
            delete notification;
        }

        vTaskDelay(10 / portTICK_PERIOD_MS);
    }
}

void EcoflowESP32::_startAuthentication() {
    ESP_LOGI(TAG, "Starting authentication");
    _state = ConnectionState::PUBLIC_KEY_EXCHANGE;
    _txSeq = 0;
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

void EcoflowESP32::_handlePacket(Packet* pkt) {
    ESP_LOGD(TAG, "_handlePacket: Handling packet with cmdId=0x%02x", pkt->getCmdId());
    if (_state == ConnectionState::AUTHENTICATED) {
        EcoflowDataParser::parsePacket(*pkt, _data);
        if (pkt->getDest() == 0x21) {
            ESP_LOGD(TAG, "Replying to packet with cmdSet=0x%02x, cmdId=0x%02x", pkt->getCmdSet(), pkt->getCmdId());
            Packet reply(pkt->getDest(), pkt->getSrc(), pkt->getCmdSet(), pkt->getCmdId(), pkt->getPayload(), 0x01, 0x01, pkt->getVersion(), pkt->getSeq(), 0x0d);
            EncPacket enc_reply(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, reply.toBytes());
            _sendCommand(enc_reply.toBytes(&_crypto));
        }
    } else {
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

            Packet auth_pkt(0x21, 0x35, 0x35, 0x86, auth_payload, 0x01, 0x01, _protocolVersion, _txSeq++, 0x0d);
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

void EcoflowESP32::_sendConfigPacket(const pd335_sys_ConfigWrite& config) {
    if (!isAuthenticated()) return;

    uint8_t buffer[128];
    pb_ostream_t stream = pb_ostream_from_buffer(buffer, sizeof(buffer));
    if (!pb_encode(&stream, pd335_sys_ConfigWrite_fields, &config)) {
        ESP_LOGE(TAG, "Failed to encode config protobuf message");
        return;
    }

    std::vector<uint8_t> payload(buffer, buffer + stream.bytes_written);
    Packet packet(0x20, 0x02, 0xFE, 0x11, payload, 0x01, 0x01, _protocolVersion, _txSeq++);
    EncPacket enc_packet(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, packet.toBytes());
    _sendCommand(enc_packet.toBytes(&_crypto));
}


bool EcoflowESP32::_sendCommand(const std::vector<uint8_t>& command) {
    if (_pWriteChr && isConnected()) {
        print_hex_esp(command.data(), command.size(), "Sending command");
        _pWriteChr->writeValue(command.data(), command.size(), false); // Write without response
        return true;
    }
    return false;
}

// --- Updated Getters with Protocol Version Logic ---

int EcoflowESP32::getBatteryLevel() {
    if (_protocolVersion == 2) return _data.wave2.batSoc;
    return (int)_data.delta3.batteryLevel;
}
int EcoflowESP32::getInputPower() {
    if (_protocolVersion == 2) return (_data.wave2.batPwrWatt > 0) ? _data.wave2.batPwrWatt : 0;
    return (int)_data.delta3.inputPower;
}
int EcoflowESP32::getOutputPower() {
    if (_protocolVersion == 2) return (_data.wave2.batPwrWatt < 0) ? abs(_data.wave2.batPwrWatt) : 0;
    return (int)_data.delta3.outputPower;
}
int EcoflowESP32::getBatteryVoltage() { return 0; } // Not in filtered list
int EcoflowESP32::getACVoltage() { return 0; } // Not in filtered list
int EcoflowESP32::getACFrequency() { return 0; } // Not in filtered list

// New getters
int EcoflowESP32::getSolarInputPower() {
    if (_protocolVersion == 2) return _data.wave2.mpptPwrWatt;
    return (int)_data.delta3.solarInputPower;
}
int EcoflowESP32::getAcOutputPower() {
    if (_protocolVersion == 2) return 0; // Wave 2 is DC only usually?
    return (int)abs(_data.delta3.acOutputPower);
}
int EcoflowESP32::getDcOutputPower() {
    if (_protocolVersion == 2) return _data.wave2.psdrPwrWatt;
    return (int)abs(_data.delta3.dc12vOutputPower);
}
int EcoflowESP32::getCellTemperature() {
    if (_protocolVersion == 2) return (int)_data.wave2.outLetTemp;
    return _data.delta3.cellTemperature;
}
int EcoflowESP32::getAmbientTemperature() {
    if (_protocolVersion == 2) return (int)_data.wave2.envTemp;
    return 0;
}
int EcoflowESP32::getMaxChgSoc() {
    if (_protocolVersion == 2) return 100; // Wave 2 doesn't have this exposed in packet?
    return _data.delta3.batteryChargeLimitMax;
}
int EcoflowESP32::getMinDsgSoc() {
    if (_protocolVersion == 2) return 0;
    return _data.delta3.batteryChargeLimitMin;
}
int EcoflowESP32::getAcChgLimit() {
    if (_protocolVersion == 2) return 0;
    return _data.delta3.acChargingSpeed;
}

bool EcoflowESP32::isAcOn() {
    if (_protocolVersion == 2) return (_data.wave2.mode != 0);
    return _data.delta3.acOn;
}
bool EcoflowESP32::isDcOn() {
    if (_protocolVersion == 2) return (_data.wave2.powerMode != 0);
    return _data.delta3.dcOn;
}
bool EcoflowESP32::isUsbOn() {
    if (_protocolVersion == 2) return false;
    return _data.delta3.usbOn;
}

bool EcoflowESP32::isConnected() {
    return _state >= ConnectionState::CONNECTED && _state <= ConnectionState::AUTHENTICATED;
}

bool EcoflowESP32::isConnecting() {
    return (_state >= ConnectionState::CREATED && _state < ConnectionState::AUTHENTICATED) || _state == ConnectionState::ESTABLISHING_CONNECTION;
}

bool EcoflowESP32::isAuthenticated() { return _state == ConnectionState::AUTHENTICATED; }

bool EcoflowESP32::requestData() {
    if (!isAuthenticated()) return false;
    Packet packet(0x01, 0x02, 0xFE, 0x11, {}, 0x01, 0x01, _protocolVersion, _txSeq++, 0x0d);
    EncPacket enc_packet(EncPacket::FRAME_TYPE_PROTOCOL, EncPacket::PAYLOAD_TYPE_VX_PROTOCOL, packet.toBytes());
    return _sendCommand(enc_packet.toBytes(&_crypto));
}

bool EcoflowESP32::setDC(bool on) {
    pd335_sys_ConfigWrite config = pd335_sys_ConfigWrite_init_zero;
    config.has_cfg_dc_12v_out_open = true;
    config.cfg_dc_12v_out_open = on;
    _sendConfigPacket(config);
    return true;
}

bool EcoflowESP32::setBatterySOCLimits(int maxChg, int minDsg) {
    pd335_sys_ConfigWrite config = pd335_sys_ConfigWrite_init_zero;

    if (maxChg >= 50 && maxChg <= 100) {
        config.has_cfg_max_chg_soc = true;
        config.cfg_max_chg_soc = maxChg;
    }
    if (minDsg >= 0 && minDsg <= 30) {
        config.has_cfg_min_dsg_soc = true;
        config.cfg_min_dsg_soc = minDsg;
    }

    _sendConfigPacket(config);
    return true;
}

bool EcoflowESP32::setUSB(bool on) {
    pd335_sys_ConfigWrite config = pd335_sys_ConfigWrite_init_zero;
    config.has_cfg_usb_open = true;
    config.cfg_usb_open = on;
    _sendConfigPacket(config);
    return true;
}

bool EcoflowESP32::setAC(bool on) {
    pd335_sys_ConfigWrite config = pd335_sys_ConfigWrite_init_zero;
    config.has_cfg_ac_out_open = true;
    config.cfg_ac_out_open = on;
    _sendConfigPacket(config);
    return true;
}

bool EcoflowESP32::setAcChargingLimit(int watts) {
    if (watts < 200 || watts > 2900) {
        ESP_LOGW(TAG, "AC Charging limit %d W out of range", watts);
    }
    pd335_sys_ConfigWrite config = pd335_sys_ConfigWrite_init_zero;

    config.has_cfg_ac_in_chg_mode = true;
    config.cfg_ac_in_chg_mode = pd335_sys_AC_IN_CHG_MODE_AC_IN_CHG_MODE_SELF_DEF_POW;

    config.has_cfg_plug_in_info_ac_in_chg_pow_max = true;
    config.cfg_plug_in_info_ac_in_chg_pow_max = watts;

    _sendConfigPacket(config);
    return true;
}
