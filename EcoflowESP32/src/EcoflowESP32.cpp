#include "EcoflowESP32.h"
#include <MD5Builder.h>

// Helper to calculate MD5 hash using Arduino's MD5Builder
static void md5_hash(const std::string& input, uint8_t* output_hash) {
    MD5Builder md5;
    md5.begin();
    md5.add((uint8_t*)input.c_str(), input.length());
    md5.calculate();
    md5.getBytes(output_hash);
}


// Struct to hold the decoded key-value pairs from the device
struct DecodedParam {
    char key[32];
    int32_t value;
};

// Nanopb callback for decoding the map field in DeviceAllData
static bool decode_alldata_map_callback(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    EcoflowESP32* instance = (EcoflowESP32*)(*arg);

    DeviceAllData_ParamsEntry entry = DeviceAllData_ParamsEntry_init_zero;

    char key_buffer[32];
    entry.key.funcs.decode = [](pb_istream_t *stream, const pb_field_t *field, void **arg) -> bool {
        char* dest = (char*)(*arg);
        uint64_t len = stream->bytes_left;
        if (len > 31) len = 31; // Prevent buffer overflow
        if (!pb_read(stream, (pb_byte_t*)dest, len)) return false;
        dest[len] = '\0';
        return true;
    };
    entry.key.arg = key_buffer;

    if (!pb_decode(stream, DeviceAllData_ParamsEntry_fields, &entry)) {
        return false;
    }

    instance->updateData(key_buffer, entry.value);

    return true;
}


// Static instance
EcoflowESP32* EcoflowESP32::_instance = nullptr;

// Keep-alive config
static const uint32_t KEEPALIVE_INTERVAL_MS = 3000; // 3 s
static const uint32_t KEEPALIVE_CHECK_MS = 500;  // 0.5 s
static const uint32_t AUTH_TIMEOUT_MS = 5000; // 5s

// Forward declarations
void keepAliveTask(void* param);
void handleNotificationCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic,
                                uint8_t* pData, size_t length, bool isNotify);

// Keep-alive task: sends a heartbeat
void keepAliveTask(void* param) {
    EcoflowESP32* pThis = static_cast<EcoflowESP32*>(param);
    Serial.println(">>> Keep-Alive task started");

    while (pThis && pThis->_running) {
        if (pThis->isConnected()) {
            uint32_t now = millis();
            if (now - pThis->_lastCommandTime >= KEEPALIVE_INTERVAL_MS) {
                pThis->sendHeartbeat();
            }
        }
        vTaskDelay(KEEPALIVE_CHECK_MS / portTICK_PERIOD_MS);
    }

    Serial.println(">>> Keep-Alive task stopped");
    vTaskDelete(nullptr);
}

// Notification callback
void handleNotificationCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic,
                                uint8_t* pData, size_t length, bool isNotify) {
    EcoflowESP32* inst = EcoflowESP32::getInstance();
    if (inst) {
        inst->onNotify(pRemoteCharacteristic, pData, length, isNotify);
    }
}

// NimBLE client callbacks
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

// Constructor / destructor
EcoflowESP32::EcoflowESP32() {
    _instance = this;
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

// Public API
bool EcoflowESP32::begin() {
    NimBLEDevice::init("");
    NimBLEDevice::setMTU(136); // As per recommendation for Delta 3
    _running = true;
    return true;
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
    pScan->setInterval(97);
    pScan->setWindow(97);
    pScan->setActiveScan(true);

    NimBLEScanResults results = pScan->start(scanTime, false);

    for (uint32_t i = 0; i < results.getCount(); i++) {
        NimBLEAdvertisedDevice device = results.getDevice(i);
        if (device.haveManufacturerData()) {
            std::string mfgData = device.getManufacturerData();
            if (mfgData.length() >= 2) {
                uint16_t manufacturerId = (uint16_t)(mfgData[1] << 8) | mfgData[0];
                if (manufacturerId == ECOFLOW_MANUFACTURER_ID) {
                    m_pAdvertisedDevice = new NimBLEAdvertisedDevice(device);
                    return true;
                }
            }
        }
    }
    return false;
}


bool EcoflowESP32::connectToServer() {
    // Simplified connect logic
    pClient = NimBLEDevice::createClient();
    pClient->setClientCallbacks(&g_clientCallback, false);

    if (!pClient->connect(m_pAdvertisedDevice)) {
        return false;
    }

    if (!pClient->secureConnection()) {
        Serial.println("Failed to secure connection");
        return false;
    }

    if (!_resolveCharacteristics()) {
        return false;
    }

    pReadChr->subscribe(true, handleNotificationCallback, false);
    _connected = true;

    if (!_authenticate()) {
        return false;
    }

    if(_authenticated) {
        _startKeepAliveTask();
    }
    return _authenticated;
}

void EcoflowESP32::disconnect() {
    _connected = false;
    _authenticated = false;
    _stopKeepAliveTask();
    if (pClient && pClient->isConnected()) {
        pClient->disconnect();
    }
    // NimBLEDevice::deleteClient(pClient); // Can cause issues if not handled carefully
    pClient = nullptr;
}

void EcoflowESP32::update() {
    // For non-BLE tasks
}

// ... other public methods like getters ...
int EcoflowESP32::getBatteryLevel() { return _data.batteryLevel; }
int EcoflowESP32::getInputPower() { return _data.inputPower; }
int EcoflowESP32::getOutputPower() { return _data.outputPower; }
int EcoflowESP32::getBatteryVoltage() { return _data.batteryVoltage; }
int EcoflowESP32::getACVoltage() { return _data.acVoltage; }
int EcoflowESP32::getACFrequency() { return _data.acFrequency; }
bool EcoflowESP32::isAcOn() { return _data.acOn; }
bool EcoflowESP32::isDcOn() { return _data.dcOn; }
bool EcoflowESP32::isUsbOn() { return _data.usbOn; }
uint32_t EcoflowESP32::getLastDataTime() { return _lastDataTime; }

bool EcoflowESP32::_sendCommand(const ToDevice& message) {
    if (!isConnected()) return false;

    uint8_t buffer[256];
    size_t frame_len = create_command_frame(buffer, sizeof(buffer), message);
    if (frame_len == 0) {
        Serial.println("Failed to create command frame");
        return false;
    }

    if (!pWriteChr->writeValue(buffer, frame_len, true)) {
        Serial.println("Failed to write command");
        return false;
    }
    _lastCommandTime = millis();
    _sequence++;
    return true;
}

ToDevice create_auth_message(uint32_t seq) {
    ToDevice msg = ToDevice_init_zero;
    msg.seq = seq;
    msg.which_pdata = ToDevice_get_sn_tag;
    msg.pdata.get_sn = GetDeviceSn_init_zero;
    return msg;
}

bool EcoflowESP32::_authenticate() {
    if (_userId.empty() || _deviceSn.empty()) {
        Serial.println("ERROR: Credentials not set");
        return false;
    }

    _isAuthenticating = true;

    if (!_sendCommand(create_auth_message(_sequence))) {
        Serial.println("Failed to send auth frame");
        _isAuthenticating = false;
        return false;
    }

    // Wait for the auth response
    uint32_t startTime = millis();
    while (_isAuthenticating && (millis() - startTime < AUTH_TIMEOUT_MS)) {
        delay(100);
    }

    _isAuthenticating = false;
    return _authenticated;
}


void EcoflowESP32::updateData(const char* key, int32_t value) {
    if (strcmp(key, "bms.soc") == 0) {
        _data.batteryLevel = value;
    } else if (strcmp(key, "in.power") == 0) {
        _data.inputPower = value;
    } else if (strcmp(key, "out.power") == 0) {
        _data.outputPower = value;
    } else if (strcmp(key, "ac.on") == 0) {
        _data.acOn = (value != 0);
    } else if (strcmp(key, "dc.on") == 0) {
        _data.dcOn = (value != 0);
    } else if (strcmp(key, "usb.on") == 0) {
        _data.usbOn = (value != 0);
    }
    // Add other keys as needed
}


void EcoflowESP32::onNotify(NimBLERemoteCharacteristic*, uint8_t* pData, size_t length, bool) {
    FromDevice message = FromDevice_init_zero;

    if (length < 10) return; // Basic sanity check for frame size

    // Unpack the outer frame to get the protobuf payload
    size_t proto_len = pData[2];
    if (8 + proto_len + 2 > length) {
        Serial.println("Invalid frame length");
        return;
    }
    uint8_t* proto_payload = pData + 8;

    pb_istream_t proto_stream = pb_istream_from_buffer(proto_payload, proto_len);

    message.pdata.all_data.params.funcs.decode = decode_alldata_map_callback;
    message.pdata.all_data.params.arg = this;

    if (pb_decode(&proto_stream, FromDevice_fields, &message)) {
        if (_isAuthenticating && message.which_pdata == FromDevice_sn_info_tag) {
            _authenticated = true;
            _isAuthenticating = false;
            Serial.println("Authentication successful!");
        } else if (message.which_pdata == FromDevice_all_data_tag) {
            // Data is updated in the callback
            _lastDataTime = millis();
        }
    } else {
        Serial.println("Failed to decode protobuf message");
    }
}

void EcoflowESP32::parse(uint8_t* pData, size_t length) {
    // This function is now empty as the logic is in onNotify
}


// --- Commands ---
bool EcoflowESP32::requestData() {
    return _sendCommand(create_request_data_message(_sequence));
}

bool EcoflowESP32::sendHeartbeat() {
    return _sendCommand(create_heartbeat_message(_sequence));
}

bool EcoflowESP32::setAC(bool on) {
    return _sendCommand(create_set_data_message(_sequence, "ac.on", on ? 1 : 0));
}

bool EcoflowESP32::setDC(bool on) {
    return _sendCommand(create_set_data_message(_sequence, "dc.on", on ? 1 : 0));
}

bool EcoflowESP32::setUSB(bool on) {
    return _sendCommand(create_set_data_message(_sequence, "usb.on", on ? 1 : 0));
}


// Other private methods
void EcoflowESP32::_startKeepAliveTask() {
    if (_keepAliveTaskHandle) return;
    xTaskCreate(keepAliveTask, "EcoflowKeepAlive", 4096, this, 1, &_keepAliveTaskHandle);
}

void EcoflowESP32::_stopKeepAliveTask() {
    if (_keepAliveTaskHandle) {
        vTaskDelete(_keepAliveTaskHandle);
        _keepAliveTaskHandle = nullptr;
    }
}

bool EcoflowESP32::isConnected() {
    return _connected && pClient && pClient->isConnected();
}
bool EcoflowESP32::_resolveCharacteristics() {
    if (!pClient) return false;

    pWriteChr = nullptr;
    pReadChr = nullptr;

    NimBLERemoteService* pService = pClient->getService(SERVICE_UUID_ECOFLOW);
    if (pService) {
        pWriteChr = pService->getCharacteristic(CHAR_WRITE_UUID_ECOFLOW);
        pReadChr = pService->getCharacteristic(CHAR_READ_UUID_ECOFLOW);

        if (pWriteChr && pReadChr) {
            return true;
        }
    }
    return false;
}

void EcoflowESP32::onConnect(NimBLEClient* /*pclient*/) {
    Serial.println(">>> [CALLBACK] Connected");
}

void EcoflowESP32::onDisconnect(NimBLEClient* /*pclient*/) {
    Serial.println(">>> [CALLBACK] Disconnected");
    _connected     = false;
    _authenticated = false;
}
