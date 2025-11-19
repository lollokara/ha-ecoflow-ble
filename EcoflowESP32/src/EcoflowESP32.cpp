#include "EcoflowESP32.h"
#include "EcoflowProtocol.h"

#include <mbedtls/md5.h>

// ---------------------------------------------------------------------------
// Static instance
// ---------------------------------------------------------------------------
EcoflowESP32* EcoflowESP32::_instance = nullptr;

// Keep‑alive config
static const uint8_t  CONNECT_RETRIES          = 3;
static const uint32_t CONNECT_RETRY_DELAY_MS   = 250;
static const uint32_t SERVICE_DISCOVERY_DELAY  = 1000;
static const uint32_t KEEPALIVE_INTERVAL_MS    = 3000; // 3 s
static const uint32_t KEEPALIVE_CHECK_MS       = 500;  // 0.5 s
static const uint32_t AUTH_TIMEOUT_MS          = 5000; // 5 s

// ---------------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------------
void keepAliveTask(void* param);
void handleNotificationCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic,
                                uint8_t* pData, size_t length, bool isNotify);

// ---------------------------------------------------------------------------
// MD5 helper
// ---------------------------------------------------------------------------
static void md5_hash(const std::string& input, uint8_t* output_hash) {
    mbedtls_md5_context ctx;
    mbedtls_md5_init(&ctx);
    mbedtls_md5_starts(&ctx);
    mbedtls_md5_update(&ctx, (const uint8_t*)input.c_str(), input.length());
    mbedtls_md5_finish(&ctx, output_hash);
    mbedtls_md5_free(&ctx);
}

// ---------------------------------------------------------------------------
// Keep‑alive task: send requestData + poll read characteristic if needed
// ---------------------------------------------------------------------------
void keepAliveTask(void* param) {
    EcoflowESP32* pThis = static_cast<EcoflowESP32*>(param);
    Serial.println(">>> Keep‑Alive task started");

    while (pThis && pThis->_running) {
        if (pThis->isConnected() && pThis->_authenticated) {
            uint32_t now = millis();
            uint32_t dt  = now - pThis->_lastCommandTime;
            if (dt >= KEEPALIVE_INTERVAL_MS) {
                Serial.println(">>> [KEEP‑ALIVE] Sending request data…");
                pThis->_notificationReceived = false;
                pThis->requestData();
                pThis->_lastCommandTime = millis();

                // Wait a short time for notify; if nothing, poll the read char
                uint32_t waitStart = millis();
                while (millis() - waitStart < 400) {
                    if (pThis->_notificationReceived) {
                        break;
                    }
                    vTaskDelay(50 / portTICK_PERIOD_MS);
                }

                if (!pThis->_notificationReceived && pThis->pReadChr) {
                    try {
                        std::string val = pThis->pReadChr->readValue();
                        if (!val.empty()) {
                            Serial.print(">>> [POLL] Read ");
                            Serial.print(val.size());
                            Serial.println(" bytes from notify characteristic");
                            pThis->parse((uint8_t*)val.data(), val.size());
                        }
                    } catch (...) {
                        Serial.println(">>> [POLL] ERROR: readValue failed");
                    }
                }
            }
        }
        vTaskDelay(KEEPALIVE_CHECK_MS / portTICK_PERIOD_MS);
    }

    Serial.println(">>> Keep‑Alive task stopped");
    vTaskDelete(nullptr);
}

// ---------------------------------------------------------------------------
// Notification callback
// ---------------------------------------------------------------------------
void handleNotificationCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic,
                                uint8_t* pData, size_t length, bool isNotify) {
    static uint32_t callbackCount = 0;
    callbackCount++;

    Serial.print(">>> [CALLBACK #");
    Serial.print(callbackCount);
    Serial.print("] Triggered, length=");
    Serial.println(length);

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

// ---------------------------------------------------------------------------
// NimBLE client callbacks
// ---------------------------------------------------------------------------
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

// ---------------------------------------------------------------------------
// Constructor / destructor
// ---------------------------------------------------------------------------
EcoflowESP32::EcoflowESP32()
    : pClient(nullptr),
      pWriteChr(nullptr),
      pReadChr(nullptr),
      m_pAdvertisedDevice(nullptr),
      _connected(false),
      _authenticated(false),
      _subscribedToNotifications(false),
      _keepAliveTaskHandle(nullptr),
      _lastDataTime(0),
      _lastCommandTime(0),
      _running(false),
      _notificationReceived(false),
      _lastNotificationTime(0) {
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

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------
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
    _userId   = userId;
    _deviceSn = deviceSn;
    Serial.print(">>> Credentials set: userId=");
    Serial.print(_userId.c_str());
    Serial.print(", deviceSn=");
    Serial.println(_deviceSn.c_str());
}

// Scan for Ecoflow by manufacturer ID
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

        uint32_t start   = millis();
        NimBLEScanResults results = pScan->start(scanTime, false);
        uint32_t elapsed = millis() - start;
        uint32_t count   = results.getCount();

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
                (static_cast<uint8_t>(mfgData[1]) << 8) |
                 static_cast<uint8_t>(mfgData[0]);

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

    // Reuse existing client if still connected and characteristics ready
    if (pClient && pClient->isConnected()) {
        if (pWriteChr && pReadChr && _subscribedToNotifications) {
            Serial.println(">>> Already connected with notifications active");
            _connected = true;

            // Ensure auth + keepalive are running on reused connection
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

        // else clean up incomplete state
        try { pClient->disconnect(); } catch (...) {}
        try { NimBLEDevice::deleteClient(pClient); } catch (...) {}
        pClient = nullptr;
        pWriteChr = nullptr;
        pReadChr  = nullptr;
        _subscribedToNotifications = false;
    }

    // Try to connect with retries
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
            uint32_t t0 = millis();
            bool ok = pClient->connect(m_pAdvertisedDevice);
            uint32_t tConnect = millis() - t0;
            if (!ok) {
                Serial.print(">>> Physical connection failed after ");
                Serial.print(tConnect);
                Serial.println(" ms");
                try { NimBLEDevice::deleteClient(pClient); } catch (...) {}
                pClient = nullptr;
                if (attempt < CONNECT_RETRIES) delay(CONNECT_RETRY_DELAY_MS);
                continue;
            }

            Serial.print(">>> Physical connection established in ");
            Serial.print(tConnect);
            Serial.println(" ms");
            break;
        } catch (...) {
            Serial.println(">>> Exception during connection");
            if (attempt < CONNECT_RETRIES) delay(CONNECT_RETRY_DELAY_MS);
        }
    }

    if (!pClient || !pClient->isConnected()) {
        Serial.println(">>> ERROR: Failed to connect after all retries");
        if (pClient) {
            try { NimBLEDevice::deleteClient(pClient); } catch (...) {}
            pClient = nullptr;
        }
        return false;
    }

    uint16_t mtu = pClient->getMTU();
    Serial.print(">>> MTU from client: ");
    Serial.println(mtu);

    Serial.println(">>> Waiting for service discovery…");
    delay(SERVICE_DISCOVERY_DELAY);

    Serial.println(">>> Discovering services and characteristics…");
    if (!_resolveCharacteristics()) {
        Serial.println(">>> ERROR: Could not resolve Ecoflow characteristics");
        return false;
    }

    // Enable notifications
    Serial.println(">>> Enabling notifications…");
    try {
        pReadChr->subscribe(true, handleNotificationCallback, false);
        Serial.println(">>> Callback registered");
        delay(100);

        NimBLERemoteDescriptor* pDesc =
            pReadChr->getDescriptor(BLEUUID((uint16_t)0x2902));
        if (pDesc) {
            uint8_t val[2] = {0x01, 0x00};
            pDesc->writeValue(val, 2, true);
            Serial.println(">>> ✓ CCCD descriptor written");
        } else {
            Serial.println(">>> WARNING: CCCD descriptor not found");
        }

        _subscribedToNotifications = true;
        delay(500);
    } catch (...) {
        Serial.println(">>> ERROR: Failed to enable notifications");
        return false;
    }

    // Mark as connected BEFORE authentication so sendCommand works
    _connected        = true;
    _lastDataTime     = millis();
    _lastCommandTime  = millis();

    // Protocol-level authentication
    if (!_authenticate()) {
        Serial.println(">>> ERROR: Authentication failed");
        _connected = false;
        return false;
    }

    Serial.println(">>> ✓ Connected to Delta 3");
    Serial.print(">>> MTU: ");
    Serial.println(pClient->getMTU());
    Serial.println(">>> Waiting for device data…");

    _startKeepAliveTask();
    return true;
}

void EcoflowESP32::disconnect() {
    _connected    = false;
    _authenticated = false;
    _subscribedToNotifications = false;

    if (pClient) {
        try {
            if (pClient->isConnected()) pClient->disconnect();
        } catch (...) {}
        try {
            NimBLEDevice::deleteClient(pClient);
        } catch (...) {}
        pClient   = nullptr;
        pWriteChr = nullptr;
        pReadChr  = nullptr;
    }
}

void EcoflowESP32::update() {
    // Placeholder for any periodic non-BLE tasks
}

// ---------------------------------------------------------------------------
// Characteristics resolution
// ---------------------------------------------------------------------------
bool EcoflowESP32::_resolveCharacteristics() {
    if (!pClient) return false;

    pWriteChr = nullptr;
    pReadChr  = nullptr;

    try {
        std::vector<NimBLERemoteService*>* services =
            pClient->getServices(true);
        if (!services || services->empty()) {
            Serial.println(">>> WARNING: No services found");
            return false;
        }

        Serial.print(">>> Found ");
        Serial.print(services->size());
        Serial.println(" services");

        NimBLERemoteService* pService =
            pClient->getService(SERVICE_UUID_ECOFLOW);
        if (pService) {
            Serial.println(">>> Found Ecoflow service");

            pWriteChr = pService->getCharacteristic(CHAR_WRITE_UUID_ECOFLOW);
            pReadChr  = pService->getCharacteristic(CHAR_READ_UUID_ECOFLOW);

            if (pWriteChr && pReadChr) {
                Serial.println(">>> ✓ Found both characteristics");
                return true;
            }
        }

        // Try alternate service if needed
        pService = pClient->getService(SERVICE_UUID_ALT);
        if (pService) {
            Serial.println(">>> Found alternate service");
            pWriteChr = pService->getCharacteristic(CHAR_WRITE_UUID_ALT);
            pReadChr  = pService->getCharacteristic(CHAR_READ_UUID_ALT);
            if (pWriteChr && pReadChr) {
                Serial.println(">>> ✓ Found alternate characteristics");
                return true;
            }
        }

        return false;
    } catch (...) {
        Serial.println(">>> ERROR resolving characteristics");
        return false;
    }
}

// ---------------------------------------------------------------------------
// Callbacks from NimBLE
// ---------------------------------------------------------------------------
void EcoflowESP32::onConnect(NimBLEClient* /*pclient*/) {
    Serial.println(">>> [CALLBACK] Connected");
}

void EcoflowESP32::onDisconnect(NimBLEClient* /*pclient*/) {
    Serial.println(">>> [CALLBACK] Disconnected");
    _connected     = false;
    _authenticated = false;
}

// Called from notification callback
void EcoflowESP32::onNotify(NimBLERemoteCharacteristic* /*pRemoteCharacteristic*/,
                            uint8_t* pData, size_t length, bool /*isNotify*/) {
    parse(pData, length);
}

// ---------------------------------------------------------------------------
// Keep‑alive task management
// ---------------------------------------------------------------------------
void EcoflowESP32::_startKeepAliveTask() {
    if (_keepAliveTaskHandle) return;
    xTaskCreate(keepAliveTask, "EcoflowKeepAlive", 4096,
                this, 1, &_keepAliveTaskHandle);
}

void EcoflowESP32::_stopKeepAliveTask() {
    if (_keepAliveTaskHandle) {
        vTaskDelete(_keepAliveTaskHandle);
        _keepAliveTaskHandle = nullptr;
    }
}

// ---------------------------------------------------------------------------
// Authentication with MD5-based secretKey
// ---------------------------------------------------------------------------
bool EcoflowESP32::_authenticate() {
    if (_userId.empty() || _deviceSn.empty()) {
        Serial.println(">>> ERROR: Credentials not set (call setCredentials first)");
        return false;
    }

    Serial.println(">>> [AUTH] Starting autoAuthentication...");
    Serial.print(">>> [AUTH] userId=");
    Serial.print(_userId.c_str());
    Serial.print(", deviceSn=");
    Serial.println(_deviceSn.c_str());

    // Build the secret key: MD5(userId + deviceSn)
    std::string secretInput = _userId + _deviceSn;
    uint8_t secretKey[16];
    md5_hash(secretInput, secretKey);

    Serial.print(">>> [AUTH] Secret key (MD5): ");
    for (int i = 0; i < 16; i++) {
        if (secretKey[i] < 0x10) Serial.print("0");
        Serial.print(secretKey[i], HEX);
    }
    Serial.println();

    // Build auth frame:
    // AA 02 01 01 [16 bytes of secretKey] [XOR checksum]
    uint8_t authFrame[21];
    authFrame[0] = 0xAA;
    authFrame[1] = 0x02;
    authFrame[2] = 0x01;
    authFrame[3] = 0x01;
    for (int i = 0; i < 16; i++) {
        authFrame[4 + i] = secretKey[i];
    }

    uint8_t checksum = 0;
    for (int i = 0; i < 20; i++) {
        checksum ^= authFrame[i];
    }
    authFrame[20] = checksum;

    Serial.print(">>> [AUTH] Sending auth frame (21 bytes): ");
    for (int i = 0; i < 21; i++) {
        if (authFrame[i] < 0x10) Serial.print("0");
        Serial.print(authFrame[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    if (!sendCommand(authFrame, sizeof(authFrame))) {
        Serial.println(">>> ERROR: Failed to send auth frame");
        return false;
    }

    Serial.println(">>> [AUTH] Waiting for auth response...");
    uint32_t authStart = millis();
    _notificationReceived = false;

    while (millis() - authStart < AUTH_TIMEOUT_MS) {
        if (_notificationReceived) {
            Serial.println(">>> [AUTH] ✓ Auth response received!");
            _authenticated = true;
            _notificationReceived = false;
            return true;
        }
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }

    Serial.println(">>> [AUTH] WARNING: No auth response within timeout");
    // Some devices may not send explicit ACK; allow continuing
    _authenticated = true;
    return true;
}

// ---------------------------------------------------------------------------
// Commands
// ---------------------------------------------------------------------------
bool EcoflowESP32::sendCommand(const uint8_t* command, size_t size) {
    if (!_connected) {
        Serial.println(">>> ERROR: Not connected flag is false (sendCommand)");
        return false;
    }
    if (!pClient) {
        Serial.println(">>> ERROR: pClient is null (sendCommand)");
        return false;
    }
    if (!pClient->isConnected()) {
        Serial.println(">>> ERROR: pClient->isConnected() == false (sendCommand)");
        return false;
    }
    if (!pWriteChr) {
        Serial.println(">>> ERROR: pWriteChr is null (sendCommand)");
        return false;
    }
    if (!command || size == 0) {
        Serial.println(">>> ERROR: Invalid command buffer");
        return false;
    }

    try {
        pWriteChr->writeValue((uint8_t*)command, size, true);
        Serial.print(">>> [COMMAND] Sent ");
        Serial.print(size);
        Serial.println(" bytes");
        _lastCommandTime = millis();
        return true;
    } catch (...) {
        Serial.println(">>> ERROR: Send failed");
        return false;
    }
}

bool EcoflowESP32::requestData() {
    Serial.println(">>> Requesting device data…");
    return sendCommand(CMD_REQUEST_DATA, sizeof(CMD_REQUEST_DATA));
}

bool EcoflowESP32::setAC(bool on) {
    Serial.print(">>> Setting AC to ");
    Serial.println(on ? "ON" : "OFF");
    const uint8_t* cmd = on ? CMD_AC_ON : CMD_AC_OFF;
    size_t size        = on ? sizeof(CMD_AC_ON) : sizeof(CMD_AC_OFF);

    _notificationReceived = false;
    _lastNotificationTime = 0;
    if (!sendCommand(cmd, size)) return false;

    uint32_t start = millis();
    while (millis() - start < 2000) {
        if (_notificationReceived) break;
        vTaskDelay(100 / portTICK_PERIOD_MS);
    }
    return true;
}

bool EcoflowESP32::setDC(bool on) {
    Serial.print(">>> Setting DC 12V to ");
    Serial.println(on ? "ON" : "OFF");
    const uint8_t* cmd = on ? CMD_12V_ON : CMD_12V_OFF;
    size_t size        = on ? sizeof(CMD_12V_ON) : sizeof(CMD_12V_OFF);
    return sendCommand(cmd, size);
}

bool EcoflowESP32::setUSB(bool on) {
    Serial.print(">>> Setting USB to ");
    Serial.println(on ? "ON" : "OFF");
    const uint8_t* cmd = on ? CMD_USB_ON : CMD_USB_OFF;
    size_t size        = on ? sizeof(CMD_USB_ON) : sizeof(CMD_USB_OFF);
    return sendCommand(cmd, size);
}

// ---------------------------------------------------------------------------
// Data parsing (frame -> EcoflowData)
// ---------------------------------------------------------------------------
void EcoflowESP32::parse(uint8_t* pData, size_t length) {
    if (!pData || length < 21) {
        Serial.println(">>> [PARSE] Frame too short");
        return;
    }

    // Header check
    if (pData[0] != 0xAA || pData[1] != 0x02) {
        Serial.println(">>> [PARSE] Invalid header");
        return;
    }

    // Checksum: XOR of bytes 0..19 must equal byte 20
    uint8_t cs = 0;
    for (size_t i = 0; i < 20; ++i) cs ^= pData[i];
    if (cs != pData[20]) {
        Serial.print(">>> [PARSE] Checksum mismatch, calc=");
        if (cs < 0x10) Serial.print("0");
        Serial.print(cs, HEX);
        Serial.print(" recv=");
        if (pData[20] < 0x10) Serial.print("0");
        Serial.println(pData[20], HEX);
        return;
    }

    // CURRENT GUESS of layout – adjust once you diff with Python:
    // pData[14-15]: output power (BE)
    // pData[16-17]: input power (BE)
    // pData[18]:    battery level %
    // pData[19]:    flags bit0=AC, bit1=USB, bit2=DC
    _data.outputPower = (static_cast<uint16_t>(pData[14]) << 8) |
                         static_cast<uint16_t>(pData[15]);
    _data.inputPower  = (static_cast<uint16_t>(pData[16]) << 8) |
                         static_cast<uint16_t>(pData[17]);
    _data.batteryLevel = pData[18];

    uint8_t flags = pData[19];
    _data.acOn  = (flags & 0x01) != 0;
    _data.usbOn = (flags & 0x02) != 0;
    _data.dcOn  = (flags & 0x04) != 0;

    if (length >= 25) {
        _data.batteryVoltage = (static_cast<uint16_t>(pData[21]) << 8) |
                                static_cast<uint16_t>(pData[22]);
        _data.acVoltage      = (static_cast<uint16_t>(pData[23]) << 8) |
                                static_cast<uint16_t>(pData[24]);
        // AC frequency can be added once we know the correct offset
    }

    _lastDataTime = millis();

    Serial.print(">>> [PARSED] Battery=");
    Serial.print(_data.batteryLevel);
    Serial.print("%, In=");
    Serial.print(_data.inputPower);
    Serial.print("W, Out=");
    Serial.print(_data.outputPower);
    Serial.print("W, AC=");
    Serial.print(_data.acOn ? "ON" : "OFF");
    Serial.print(", USB=");
    Serial.print(_data.usbOn ? "ON" : "OFF");
    Serial.print(", DC=");
    Serial.println(_data.dcOn ? "ON" : "OFF");

    if (_data.batteryVoltage > 0) {
        Serial.print(">>> [PARSED] Battery V=");
        Serial.println(_data.batteryVoltage);
    }
    if (_data.acVoltage > 0) {
        Serial.print(">>> [PARSED] AC V=");
        Serial.println(_data.acVoltage);
    }
}

// ---------------------------------------------------------------------------
// Getters / state
// ---------------------------------------------------------------------------
int  EcoflowESP32::getBatteryLevel()   { return static_cast<int>(_data.batteryLevel); }
int  EcoflowESP32::getInputPower()     { return static_cast<int>(_data.inputPower); }
int  EcoflowESP32::getOutputPower()    { return static_cast<int>(_data.outputPower); }
int  EcoflowESP32::getBatteryVoltage() { return static_cast<int>(_data.batteryVoltage); }
int  EcoflowESP32::getACVoltage()      { return static_cast<int>(_data.acVoltage); }
int  EcoflowESP32::getACFrequency()    { return static_cast<int>(_data.acFrequency); }
bool EcoflowESP32::isAcOn()            { return _data.acOn; }
bool EcoflowESP32::isDcOn()            { return _data.dcOn; }
bool EcoflowESP32::isUsbOn()           { return _data.usbOn; }

bool EcoflowESP32::isConnected() {
    return _connected && pClient && pClient->isConnected();
}
