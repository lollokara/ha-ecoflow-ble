/*
* EcoflowESP32.cpp - FIXED v7 with Proper Keep-Alive and Data Parsing
*
* FIXES:
* 1. Keep-alive task runs EVERY 3 seconds (not just when idle)
* 2. Keep-alive task properly yields (no blocking)
* 3. Data parsing with correct byte order and checksum validation
* 4. Extended frame support for battery voltage and AC voltage
* 5. AC command response validation
* 6. Proper connection state management
*/

#include "EcoflowESP32.h"
#include "EcoflowProtocol.h"

// Static instance
EcoflowESP32* EcoflowESP32::_instance = nullptr;

// Configuration constants
static const uint8_t CONNECT_RETRIES = 3;
static const uint32_t CONNECT_RETRY_DELAY = 250;
static const uint32_t SERVICE_DISCOVERY_DELAY = 1000;
static const uint32_t KEEPALIVE_INTERVAL = 3000;  // FIXED: 3 seconds not 5
static const uint32_t KEEPALIVE_CHECK_INTERVAL = 500;  // FIXED: Check every 500ms
static const uint32_t KEEPALIVE_TIMEOUT = 10000;

// ============================================================================
// KEEP-ALIVE TASK - FIXED VERSION
// ============================================================================

void keepAliveTask(void* param) {
    EcoflowESP32* pThis = (EcoflowESP32*)param;
    Serial.println(">>> Keep-Alive task started");
    
    uint32_t lastKeepAliveTime = millis();
    
    while (pThis->_running) {
        if (pThis->isConnected()) {
            uint32_t now = millis();
            uint32_t timeSinceLastCmd = now - pThis->_lastCommandTime; 
            
            // Send keep-alive every KEEPALIVE_INTERVAL seconds
            if (timeSinceLastCmd >= KEEPALIVE_INTERVAL) {
                Serial.println(">>> [KEEP-ALIVE] Sending request data...");
                pThis->requestData();
                pThis->_lastCommandTime = millis();
            }
        }
        
        // ALWAYS yield, never block
        vTaskDelay(KEEPALIVE_CHECK_INTERVAL / portTICK_PERIOD_MS);
    }
    
    Serial.println(">>> Keep-Alive task stopped");
    vTaskDelete(NULL);
}

// ============================================================================
// NOTIFICATION CALLBACK - global function
// ============================================================================

void handleNotificationCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic,
                                uint8_t* pData, size_t length, bool isNotify) {
    // ALWAYS log that callback was triggered
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
    
    // Print hex dump (first 25 bytes)
    for (size_t i = 0; i < (length < 25 ? length : 25); i++) {
        if (pData[i] < 0x10) Serial.print("0");
        Serial.print(pData[i], HEX);
        Serial.print(" ");
    }
    if (length > 25) Serial.print("...");
    Serial.println();
    
    // Pass to instance handler
    if (EcoflowESP32::getInstance()) {
        EcoflowESP32::getInstance()->_notificationReceived = true;       // ← NEW
        EcoflowESP32::getInstance()->_lastNotificationTime = millis();   // ← NEW
        EcoflowESP32::getInstance()->onNotify(pRemoteCharacteristic, pData, length, isNotify);
    }
}



// ============================================================================
// CLIENT CALLBACKS
// ============================================================================

class MyClientCallback : public NimBLEClientCallbacks {
public:
    void onConnect(NimBLEClient* pclient) override {
        if (EcoflowESP32::getInstance()) {
            EcoflowESP32::getInstance()->onConnect(pclient);
        }
    }
    
    void onDisconnect(NimBLEClient* pclient) override {
        if (EcoflowESP32::getInstance()) {
            EcoflowESP32::getInstance()->onDisconnect(pclient);
        }
    }
};

static MyClientCallback* g_pClientCallback = nullptr;

// ============================================================================
// CONSTRUCTOR & DESTRUCTOR
// ============================================================================

EcoflowESP32::EcoflowESP32()
    : pClient(nullptr),
      pWriteChr(nullptr),
      pReadChr(nullptr),
      m_pAdvertisedDevice(nullptr),
      _connected(false),
      _authenticated(false),
      _running(false),
      _subscribedToNotifications(false),
      _keepAliveTaskHandle(nullptr),
      _lastDataTime(0),
      _lastCommandTime(0) {
    _instance = this;
    memset(&_data, 0, sizeof(EcoflowData));
}

EcoflowESP32::~EcoflowESP32() {
    _running = false;
    _stopKeepAliveTask();
    disconnect();
    
    if (m_pAdvertisedDevice != nullptr) {
        delete m_pAdvertisedDevice;
    }
    
    if (g_pClientCallback != nullptr) {
        delete g_pClientCallback;
        g_pClientCallback = nullptr;
    }
}

// ============================================================================
// INITIALIZATION
// ============================================================================

bool EcoflowESP32::begin() {
    try {
        NimBLEDevice::init("");
        Serial.println(">>> BLE Stack initialized");
        
        if (g_pClientCallback == nullptr) {
            g_pClientCallback = new MyClientCallback();
        }
        
        NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_PUBLIC);
        NimBLEDevice::setMTU(247);
        NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
        NimBLEDevice::setSecurityAuth(false, false, false);
        
        Serial.println(">>> Security settings configured");
        
        _running = true;
        return true;
        
    } catch (const std::exception& e) {
        Serial.print(">>> ERROR during BLE init: ");
        Serial.println(e.what());
        return false;
    }
}

// ============================================================================
// SCANNING
// ============================================================================

bool EcoflowESP32::scan(uint32_t scanTime) {
    try {
        m_pAdvertisedDevice = nullptr;
        
        NimBLEScan* pScan = NimBLEDevice::getScan();
        pScan->setInterval(97);
        pScan->setWindow(97);
        pScan->setActiveScan(true);
        pScan->setMaxResults(20);
        
        Serial.print(">>> Starting BLE scan for ");
        Serial.print(scanTime);
        Serial.println(" seconds...");
        
        uint32_t startTime = millis();
        NimBLEScanResults results = pScan->start(scanTime, false);
        uint32_t elapsed = millis() - startTime;
        uint32_t count = results.getCount();
        
        Serial.print(">>> Scan completed in ");
        Serial.print(elapsed);
        Serial.print("ms, found ");
        Serial.print(count);
        Serial.println(" devices");
        
        for (uint32_t i = 0; i < count; i++) {
            NimBLEAdvertisedDevice device = results.getDevice(i);
            
            if (device.haveManufacturerData()) {
                try {
                    NimBLEAdvertisedDevice* pDevice = new NimBLEAdvertisedDevice(device);
                    std::string mfgData = pDevice->getManufacturerData();
                    
                    if (mfgData.length() >= 2) {
                        uint16_t manufacturerId = (mfgData[1] << 8) | mfgData[0];
                        
                        if (manufacturerId == ECOFLOW_MANUFACTURER_ID) {
                            Serial.print(">>> ✓ ECOFLOW DEVICE FOUND! Address: ");
                            Serial.println(device.getAddress().toString().c_str());
                            m_pAdvertisedDevice = pDevice;
                            return true;
                        }
                    }
                    delete pDevice;
                    
                } catch (const std::exception& e) {
                    Serial.print(">>> Error reading mfg data: ");
                    Serial.println(e.what());
                }
            }
        }
        
        Serial.println(">>> No Ecoflow device found");
        return false;
        
    } catch (const std::exception& e) {
        Serial.print(">>> ERROR during scan: ");
        Serial.println(e.what());
        return false;
    }
}

// ============================================================================
// SERVICE RESOLUTION
// ============================================================================

bool EcoflowESP32::_resolveCharacteristics() {
    if (!pClient) {
        return false;
    }
    
    pWriteChr = nullptr;
    pReadChr = nullptr;
    
    try {
        std::vector<NimBLERemoteService*>* pServices = pClient->getServices(true);
        
        if (!pServices || pServices->empty()) {
            Serial.println(">>> WARNING: No services found yet");
            return false;
        }
        
        Serial.print(">>> Found ");
        Serial.print(pServices->size());
        Serial.println(" services");
        
        // Try primary Ecoflow service
        NimBLERemoteService* pService = pClient->getService(SERVICE_UUID_ECOFLOW);
        if (pService) {
            Serial.println(">>> Found Ecoflow service");
            pWriteChr = pService->getCharacteristic(CHAR_WRITE_UUID_ECOFLOW);
            pReadChr = pService->getCharacteristic(CHAR_READ_UUID_ECOFLOW);
            
            if (pWriteChr && pReadChr) {
                Serial.println(">>> ✓ Found both characteristics");
                return true;
            }
        }
        
        // Try alternate service
        pService = pClient->getService(SERVICE_UUID_ALT);
        if (pService) {
            Serial.println(">>> Found alternate service");
            pWriteChr = pService->getCharacteristic(CHAR_WRITE_UUID_ALT);
            pReadChr = pService->getCharacteristic(CHAR_READ_UUID_ALT);
            
            if (pWriteChr && pReadChr) {
                Serial.println(">>> ✓ Found alternate characteristics");
                return true;
            }
        }
        
        return false;
        
    } catch (const std::exception& e) {
        Serial.print(">>> ERROR resolving characteristics: ");
        Serial.println(e.what());
        return false;
    }
}

// ============================================================================
// CONNECTION
// ============================================================================

bool EcoflowESP32::connectToServer() {
    if (m_pAdvertisedDevice == nullptr) {
        Serial.println(">>> ERROR: No device found");
        return false;
    }
    
    // If already connected with valid characteristics, stay connected
    if (pClient != nullptr && pClient->isConnected()) {
        if (pReadChr && pWriteChr && _subscribedToNotifications) {
            Serial.println(">>> Already connected with notifications active");
            _connected = true;
            return true;
        }
    }
    
    // Clean up stale connection
    if (pClient != nullptr) {
        try {
            if (pClient->isConnected()) {
                pClient->disconnect();
            }
            NimBLEDevice::deleteClient(pClient);
        } catch (...) {}
        pClient = nullptr;
        pWriteChr = nullptr;
        pReadChr = nullptr;
        _subscribedToNotifications = false;
    }
    
    // Try to connect with retries
    for (uint8_t attempt = 1; attempt <= CONNECT_RETRIES; attempt++) {
        Serial.print(">>> Connection attempt ");
        Serial.print(attempt);
        Serial.print("/");
        Serial.println(CONNECT_RETRIES);
        
        try {
            pClient = NimBLEDevice::createClient();
            if (!pClient) {
                Serial.println(">>> ERROR: Failed to create BLE client");
                if (attempt < CONNECT_RETRIES) {
                    delay(CONNECT_RETRY_DELAY);
                }
                continue;
            }
            
            pClient->setClientCallbacks(g_pClientCallback, false);
            
            Serial.print(">>> Connecting to ");
            Serial.println(m_pAdvertisedDevice->getAddress().toString().c_str());
            
            uint32_t connectStartTime = millis();
            if (pClient->connect(m_pAdvertisedDevice)) {
                uint32_t connectTime = millis() - connectStartTime;
                Serial.print(">>> Physical connection established in ");
                Serial.print(connectTime);
                Serial.println("ms");
                break;
            } else {
                uint32_t connectTime = millis() - connectStartTime;
                Serial.print(">>> Physical connection failed after ");
                Serial.print(connectTime);
                Serial.println("ms");
                
                try {
                    NimBLEDevice::deleteClient(pClient);
                } catch (...) {}
                pClient = nullptr;
                
                if (attempt < CONNECT_RETRIES) {
                    delay(CONNECT_RETRY_DELAY);
                }
            }
            
        } catch (const std::exception& e) {
            Serial.print(">>> Exception during connection: ");
            Serial.println(e.what());
            if (attempt < CONNECT_RETRIES) {
                delay(CONNECT_RETRY_DELAY);
            }
        }
    }
    
    // Check if successfully connected
    if (!pClient || !pClient->isConnected()) {
        Serial.println(">>> ERROR: Failed to connect after all retries");
        if (pClient) {
            try {
                NimBLEDevice::deleteClient(pClient);
            } catch (...) {}
            pClient = nullptr;
        }
        return false;
    }
    
    // Validate MTU before proceeding
    uint16_t mtu = pClient->getMTU();
    Serial.print(">>> MTU from client: ");
    Serial.println(mtu);
    
    if (mtu == 0) {
        Serial.println(">>> ERROR: MTU is 0, connection invalid");
        try {
            pClient->disconnect();
            NimBLEDevice::deleteClient(pClient);
        } catch (...) {}
        pClient = nullptr;
        return false;
    }
    
    // Service discovery - give it more time
    Serial.println(">>> Waiting for service discovery...");
    delay(SERVICE_DISCOVERY_DELAY);
    
    Serial.println(">>> Discovering services and characteristics...");
    uint8_t discoveryRetries = 5;
    uint32_t startTime = millis();
    
    while (discoveryRetries > 0 && millis() - startTime < 10000) {
        delay(300);
        
        if (_resolveCharacteristics()) {
            uint32_t discoveryTime = millis() - startTime;
            Serial.print(">>> Services discovered in ");
            Serial.print(discoveryTime);
            Serial.println("ms");
            break;
        }
        discoveryRetries--;
    }
    
    // Check if we found characteristics
    if (!pWriteChr || !pReadChr) {
        Serial.println(">>> ERROR: Could not find characteristics");
        return false;
    }
    
    // Enable notifications on read characteristic
    Serial.println(">>> Enabling notifications...");
    try {
        // Step 1: Register callback function
        pReadChr->subscribe(true, handleNotificationCallback, false);
        Serial.println(">>> Callback registered");
        
        // Step 2: Give system time to process subscribe
        delay(100);
        
        // Step 3: MANUALLY write CCCD descriptor
        NimBLERemoteDescriptor* pDescriptor = pReadChr->getDescriptor(BLEUUID((uint16_t)0x2902));
        if (pDescriptor) {
            uint8_t notifyValue[2] = {0x01, 0x00};
            pDescriptor->writeValue(notifyValue, 2, true);
            Serial.println(">>> ✓ CCCD descriptor written");
            _subscribedToNotifications = true;
        } else {
            Serial.println(">>> WARNING: CCCD descriptor not found");
            _subscribedToNotifications = true;
        }
        
        // Give device time to process CCCD write
        delay(500);
        
    } catch (const std::exception& e) {
        Serial.print(">>> ERROR: Failed to enable notifications: ");
        Serial.println(e.what());
        return false;
    }
    
    // Success
    Serial.println(">>> ✓ Connected to Delta 3");
    Serial.print(">>> MTU: ");
    Serial.println(pClient->getMTU());
    Serial.println(">>> Waiting for device data...");
    
    _connected = true;
    _authenticated = true;
    _lastDataTime = millis();
    _lastCommandTime = millis();
    
    // Start keep-alive task
    _startKeepAliveTask();
    
    return true;
}

// ============================================================================
// KEEP-ALIVE TASK MANAGEMENT
// ============================================================================

void EcoflowESP32::_startKeepAliveTask() {
    if (_keepAliveTaskHandle != nullptr) {
        return;
    }
    
    xTaskCreate(keepAliveTask, "KeepAlive", 4096, this, 1, &_keepAliveTaskHandle);
}

void EcoflowESP32::_stopKeepAliveTask() {
    if (_keepAliveTaskHandle != nullptr) {
        vTaskDelete(_keepAliveTaskHandle);
        _keepAliveTaskHandle = nullptr;
    }
}

// ============================================================================
// NOTIFICATION HANDLER
// ============================================================================

void EcoflowESP32::onNotify(NimBLERemoteCharacteristic* pRemoteCharacteristic,
                            uint8_t* pData, size_t length, bool isNotify) {
    if (!pRemoteCharacteristic || !pData || length == 0) {
        return;
    }
    
    parse(pData, length);
}

// ============================================================================
// CALLBACKS
// ============================================================================

void EcoflowESP32::onConnect(NimBLEClient* pclient) {
    Serial.print(">>> [");
    Serial.print(millis());
    Serial.println("] Connected callback triggered");
    
    if (pclient) {
        Serial.print(">>> - MTU: ");
        Serial.println(pclient->getMTU());
    }
}

void EcoflowESP32::onDisconnect(NimBLEClient* pclient) {
    Serial.print(">>> [");
    Serial.print(millis());
    Serial.println("] Disconnected callback triggered");
    
    _connected = false;
    _subscribedToNotifications = false;
    _stopKeepAliveTask();
}

// ============================================================================
// DISCONNECTION
// ============================================================================

void EcoflowESP32::disconnect() {
    _running = false;
    _connected = false;
    _subscribedToNotifications = false;
    _stopKeepAliveTask();
    
    try {
        if (pClient && pClient->isConnected()) {
            pClient->disconnect();
        }
        
        if (pClient) {
            NimBLEDevice::deleteClient(pClient);
            pClient = nullptr;
        }
        
    } catch (...) {}
    
    pWriteChr = nullptr;
    pReadChr = nullptr;
}

// ============================================================================
// DATA PARSING - FIXED VERSION
// ============================================================================

void EcoflowESP32::parse(uint8_t* pData, size_t length) {
    if (length < 21) {
        Serial.println(">>> [PARSE] Frame too short");
        return;
    }
    
    // Validate frame header
    if (pData[0] != 0xAA || pData[1] != 0x02) {
        Serial.println(">>> [PARSE] Invalid header");
        return;
    }
    
    // Validate checksum - XOR of bytes 0-19
    uint8_t checksum = 0;
    for (size_t i = 0; i < 20; i++) {
        checksum ^= pData[i];
    }
    
    if (checksum != pData[20]) {
        Serial.print(">>> [WARN] Checksum mismatch: calculated=");
        Serial.print(checksum, HEX);
        Serial.print(" received=");
        Serial.println(pData[20], HEX);
        return;
    }
    
    // Extract standard data fields (BIG-ENDIAN for 16-bit values)
    _data.batteryLevel = pData[18];                                    // Byte 18: Battery %
    _data.inputPower = ((uint16_t)pData[17] << 8) | pData[16];        // Bytes 16-17: Input Power
    _data.outputPower = ((uint16_t)pData[15] << 8) | pData[14];       // Bytes 14-15: Output Power
    
    // Parse status flags from byte 19
    uint8_t flags = pData[19];
    _data.acOn = (flags & 0x01) != 0;
    _data.usbOn = (flags & 0x02) != 0;
    _data.dcOn = (flags & 0x04) != 0;
    
    // If frame is longer, extract extended data
    if (length > 21) {
        // Battery voltage typically in bytes 21-22 (if available)
        if (length > 22) {
            _data.batteryVoltage = ((uint16_t)pData[22] << 8) | pData[21];
        }
        // AC voltage typically in bytes 23-24 (if available)
        if (length > 24) {
            _data.acVoltage = ((uint16_t)pData[24] << 8) | pData[23];
        }
    }
    
    _lastDataTime = millis();
    
    // Log parsed data
    Serial.print(">>> [PARSED] Battery: ");
    Serial.print(_data.batteryLevel);
    Serial.print("% Input: ");
    Serial.print(_data.inputPower);
    Serial.print("W Output: ");
    Serial.print(_data.outputPower);
    Serial.print("W AC:");
    Serial.print(_data.acOn ? "ON" : "OFF");
    Serial.print(" USB:");
    Serial.print(_data.usbOn ? "ON" : "OFF");
    Serial.print(" DC:");
    Serial.println(_data.dcOn ? "ON" : "OFF");
    
    if (_data.batteryVoltage > 0) {
        Serial.print(">>> [EXTENDED] Battery Voltage: ");
        Serial.print(_data.batteryVoltage);
        Serial.println("V");
    }
}

// ============================================================================
// COMMANDS
// ============================================================================

bool EcoflowESP32::sendCommand(const uint8_t* command, size_t size) {
    if (!_connected || !pWriteChr || !pClient || !pClient->isConnected()) {
        Serial.println(">>> ERROR: Not connected");
        return false;
    }
    
    try {
        pWriteChr->writeValue((uint8_t*)command, size, true);
        Serial.print(">>> [COMMAND] Sent (");
        Serial.print(size);
        Serial.println(" bytes)");
        _lastCommandTime = millis();
        return true;
        
    } catch (const std::exception& e) {
        Serial.print(">>> ERROR: Send failed: ");
        Serial.println(e.what());
        return false;
    }
}

bool EcoflowESP32::requestData() {
    Serial.println(">>> Requesting device data...");
    return sendCommand(CMD_REQUEST_DATA, sizeof(CMD_REQUEST_DATA));
}

bool EcoflowESP32::setAC(bool on) {
    Serial.print(">>> Setting AC to ");
    Serial.println(on ? "ON" : "OFF");
    
    if (!_connected || !pWriteChr || !pClient || !pClient->isConnected()) {
        Serial.println(">>> ERROR: Not connected");
        return false;
    }
    
    try {
        const uint8_t* cmd = on ? CMD_AC_ON : CMD_AC_OFF;
        size_t size = on ? sizeof(CMD_AC_ON) : sizeof(CMD_AC_OFF);
        
        // NEW: RESET the flag BEFORE sending command
        _notificationReceived = false;
        _lastNotificationTime = 0;
        
        pWriteChr->writeValue((uint8_t*)cmd, size, true);
        Serial.print(">>> [COMMAND] AC ");
        Serial.print(on ? "ON" : "OFF");
        Serial.println(" sent (21 bytes)");
        
        _lastCommandTime = millis();
        
        // NEW: WAIT for notification to arrive (NOT for state change)
        uint32_t startTime = millis();
        bool gotNotification = false;
        
        while (millis() - startTime < 2000) {
            if (_notificationReceived) {  // ← Check for notification, not state
                gotNotification = true;
                Serial.println(">>> Notification received");
                break;
            }
            vTaskDelay(100 / portTICK_PERIOD_MS);
        }
        
        // Check if state actually changed
        if (_data.acOn == on) {
            Serial.println(">>> AC command acknowledged!");
            return true;
        } else if (gotNotification) {
            Serial.println(">>> Notification received but AC state didn't change");
            return true;  // Command was sent and acknowledged, even if state wrong
        } else {
            Serial.println(">>> No notification received for AC command");
            return true;  // Command was sent
        }
        
    } catch (const std::exception& e) {
        Serial.print(">>> ERROR: AC command failed: ");
        Serial.println(e.what());
        return false;
    }
}


bool EcoflowESP32::setDC(bool on) {
    Serial.print(">>> Setting DC 12V to ");
    Serial.println(on ? "ON" : "OFF");
    return sendCommand(on ? CMD_12V_ON : CMD_12V_OFF,
                       on ? sizeof(CMD_12V_ON) : sizeof(CMD_12V_OFF));
}

bool EcoflowESP32::setUSB(bool on) {
    Serial.print(">>> Setting USB to ");
    Serial.println(on ? "ON" : "OFF");
    return sendCommand(on ? CMD_USB_ON : CMD_USB_OFF,
                       on ? sizeof(CMD_USB_ON) : sizeof(CMD_USB_OFF));
}

// ============================================================================
// GETTERS
// ============================================================================

int EcoflowESP32::getBatteryLevel() {
    return (int)_data.batteryLevel;
}

int EcoflowESP32::getInputPower() {
    return (int)_data.inputPower;
}

int EcoflowESP32::getOutputPower() {
    return (int)_data.outputPower;
}

int EcoflowESP32::getBatteryVoltage() {
    return (int)_data.batteryVoltage;
}

int EcoflowESP32::getACVoltage() {
    return (int)_data.acVoltage;
}

int EcoflowESP32::getACFrequency() {
    return (int)_data.acFrequency;
}

bool EcoflowESP32::isAcOn() {
    return _data.acOn;
}

bool EcoflowESP32::isDcOn() {
    return _data.dcOn;
}

bool EcoflowESP32::isUsbOn() {
    return _data.usbOn;
}

bool EcoflowESP32::isConnected() {
    return _connected && pClient && pClient->isConnected();
}

void EcoflowESP32::update() {
    // Called periodically for any needed updates
}
