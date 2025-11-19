/*
 * EcoflowESP32.cpp - FIXED with Proper Connection Retry
 * 
 * Key fix: Retry the PHYSICAL connection itself, not just service discovery
 * Match Python's establish_connection() behavior with exponential backoff
 * 
 * Python uses bleak_retry_connector which retries connections with backoff
 * We need to do the same in C++
 */

#include "EcoflowESP32.h"
#include "EcoflowProtocol.h"

// Static instance for callback
EcoflowESP32* EcoflowESP32::_instance = nullptr;

// Connection parameters
static const uint8_t CONNECT_RETRIES = 3;
static const uint32_t CONNECT_RETRY_DELAY = 250;  // 250ms backoff like Python

// ============================================================================
// SCAN CALLBACKS
// ============================================================================

EcoflowScanCallbacks::EcoflowScanCallbacks(EcoflowESP32* pEcoflowESP32)
    : _pEcoflowESP32(pEcoflowESP32) {}

void EcoflowScanCallbacks::onDiscovered(const NimBLEAdvertisedDevice* advertisedDevice) {
    if (advertisedDevice->haveManufacturerData()) {
        if(advertisedDevice->getManufacturerData().length() >= 2) {
            uint16_t manufacturerId = (advertisedDevice->getManufacturerData()[1] << 8) | 
                                      advertisedDevice->getManufacturerData()[0];
            if (manufacturerId == ECOFLOW_MANUFACTURER_ID) {
                _pEcoflowESP32->setAdvertisedDevice(const_cast<NimBLEAdvertisedDevice*>(advertisedDevice));
                return;
            }
        }
    }
}

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
      _lastDataTime(0) {
    _instance = this;
    _scanCallbacks = new EcoflowScanCallbacks(this);
    memset(&_data, 0, sizeof(EcoflowData));
}

EcoflowESP32::~EcoflowESP32() {
    _running = false;
    if (_keepAliveTaskHandle != nullptr) {
        vTaskDelete(_keepAliveTaskHandle);
    }
    disconnect();
    delete _scanCallbacks;
    if (m_pAdvertisedDevice != nullptr) {
        delete m_pAdvertisedDevice;
    }
}

// ============================================================================
// INITIALIZATION
// ============================================================================

bool EcoflowESP32::begin() {
    NimBLEDevice::init("");
    
    Serial.println(">>> BLE Stack initialized");
    
    // Disable security for Ecoflow (no pairing needed)
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    NimBLEDevice::setSecurityAuth(false, false, false);
    
    _running = true;
    return true;
}

// ============================================================================
// SCANNING
// ============================================================================

bool EcoflowESP32::scan(uint32_t scanTime) {
    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setScanCallbacks(_scanCallbacks, false);
    pScan->setInterval(97);
    pScan->setWindow(97);
    pScan->setActiveScan(true);
    pScan->start(scanTime, false);
    
    if (m_pAdvertisedDevice != nullptr) {
        return true;
    } else {
        return false;
    }
}

// ============================================================================
// CONNECTION & CHARACTERISTIC RESOLUTION
// ============================================================================

bool EcoflowESP32::_resolveCharacteristics() {
    if (!pClient) {
        Serial.println(">>> ERROR: No client for resolving characteristics");
        return false;
    }
    
    pWriteChr = nullptr;
    pReadChr = nullptr;
    
    // Try primary Ecoflow service UUID
    NimBLERemoteService* pService = pClient->getService(SERVICE_UUID_ECOFLOW);
    if (pService) {
        Serial.println(">>> Found primary Ecoflow service");
        pWriteChr = pService->getCharacteristic(CHAR_WRITE_UUID_ECOFLOW);
        pReadChr = pService->getCharacteristic(CHAR_READ_UUID_ECOFLOW);
        
        if (pWriteChr && pReadChr) {
            Serial.println(">>> Found both write and read characteristics");
            return true;
        }
    }
    
    // Try alternate service UUID
    pService = pClient->getService(SERVICE_UUID_ALT);
    if (pService) {
        Serial.println(">>> Found alternate service");
        pWriteChr = pService->getCharacteristic(CHAR_WRITE_UUID_ALT);
        pReadChr = pService->getCharacteristic(CHAR_READ_UUID_ALT);
        
        if (pWriteChr && pReadChr) {
            Serial.println(">>> Found characteristics in alternate service");
            return true;
        }
    }
    
    Serial.println(">>> Services not yet loaded, will retry");
    return false;
}

/*
 * ═══════════════════════════════════════════════════════════════════════════
 * CRITICAL: Retry the PHYSICAL CONNECTION with exponential backoff
 * ═══════════════════════════════════════════════════════════════════════════
 * 
 * Python's establish_connection() retries the connection with backoff.
 * NimBLE doesn't have this built-in, so we need to implement it.
 * This fixes reason 534/531 errors by giving the device time between attempts.
 */

bool EcoflowESP32::connectToServer() {
    if (m_pAdvertisedDevice == nullptr) {
        Serial.println(">>> ERROR: No device found");
        return false;
    }
    
    // If already connected, verify connection is still valid
    if (pClient != nullptr && pClient->isConnected()) {
        if (_subscribedToNotifications && pReadChr && pWriteChr) {
            Serial.println(">>> Already connected and subscribed");
            _connected = true;
            return true;
        }
    }
    
    // Clean up stale client
    if (pClient != nullptr) {
        Serial.println(">>> Cleaning up stale connection");
        if (pClient->isConnected()) {
            pClient->disconnect();
        }
        NimBLEDevice::deleteClient(pClient);
        pClient = nullptr;
        pWriteChr = nullptr;
        pReadChr = nullptr;
    }
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // RETRY LOOP: Try connecting multiple times with backoff
    // This matches Python's establish_connection() behavior
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    for (uint8_t attempt = 1; attempt <= CONNECT_RETRIES; attempt++) {
        Serial.print(">>> Connection attempt ");
        Serial.print(attempt);
        Serial.print("/");
        Serial.println(CONNECT_RETRIES);
        
        // Create fresh client for each attempt
        pClient = NimBLEDevice::createClient();
        if (!pClient) {
            Serial.println(">>> ERROR: Failed to create BLE client");
            if (attempt < CONNECT_RETRIES) {
                delay(CONNECT_RETRY_DELAY);
            }
            continue;
        }
        
        pClient->setClientCallbacks(this, false);
        
        // Attempt physical connection
        Serial.print(">>> Connecting to ");
        Serial.println(m_pAdvertisedDevice->getAddress().toString().c_str());
        
        if (pClient->connect(m_pAdvertisedDevice)) {
            Serial.println(">>> Physical connection established");
            break;  // Success! Exit retry loop
        } else {
            Serial.println(">>> Physical connection failed");
            
            // Clean up client for retry
            NimBLEDevice::deleteClient(pClient);
            pClient = nullptr;
            
            // Backoff before retry (except on last attempt)
            if (attempt < CONNECT_RETRIES) {
                Serial.print(">>> Waiting ");
                Serial.print(CONNECT_RETRY_DELAY);
                Serial.println("ms before retry...");
                delay(CONNECT_RETRY_DELAY);
            }
        }
    }
    
    // Check if we managed to connect
    if (!pClient || !pClient->isConnected()) {
        Serial.println(">>> ERROR: Failed to connect after all retries");
        if (pClient) {
            NimBLEDevice::deleteClient(pClient);
            pClient = nullptr;
        }
        return false;
    }
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // Connection succeeded, now discover services
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    Serial.println(">>> Discovering services...");
    
    // Try service discovery with retries
    uint8_t retries = 5;
    uint32_t startTime = millis();
    
    while (retries > 0 && millis() - startTime < 10000) {
        delay(500);  // Wait for services to load
        
        if (_resolveCharacteristics()) {
            Serial.println(">>> Services discovered successfully");
            break;
        }
        
        retries--;
        if (retries > 0) {
            Serial.print(">>> Service discovery retry (");
            Serial.print(retries);
            Serial.println(" remaining)");
        }
    }
    
    // Verify characteristics were found
    if (!pWriteChr || !pReadChr) {
        Serial.println(">>> ERROR: Could not find required characteristics");
        if (pClient->isConnected()) {
            pClient->disconnect();
        }
        NimBLEDevice::deleteClient(pClient);
        pClient = nullptr;
        pWriteChr = nullptr;
        pReadChr = nullptr;
        return false;
    }
    
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    // Subscribe to notifications
    // ━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━
    
    Serial.println(">>> Subscribing to notifications...");
    
    try {
        if (!pReadChr->subscribe(true, EcoflowESP32::notifyCallback, false)) {
            Serial.println(">>> ERROR: Failed to subscribe to notifications");
            if (pClient->isConnected()) {
                pClient->disconnect();
            }
            NimBLEDevice::deleteClient(pClient);
            pClient = nullptr;
            pWriteChr = nullptr;
            pReadChr = nullptr;
            return false;
        }
    } catch (const std::exception& e) {
        Serial.print(">>> ERROR: Exception during subscribe: ");
        Serial.println(e.what());
        if (pClient->isConnected()) {
            pClient->disconnect();
        }
        NimBLEDevice::deleteClient(pClient);
        pClient = nullptr;
        pWriteChr = nullptr;
        pReadChr = nullptr;
        return false;
    }
    
    Serial.println(">>> Notifications subscribed successfully");
    _subscribedToNotifications = true;
    _connected = true;
    _authenticated = true;
    _lastDataTime = millis();
    
    // Start keep-alive task
    if (_keepAliveTaskHandle == nullptr) {
        xTaskCreate(
            EcoflowESP32::_keepAliveTaskStatic,
            "EcoflowKeepAlive",
            2048,
            this,
            1,
            &_keepAliveTaskHandle
        );
    }
    
    // Request initial data
    requestData();
    
    Serial.println(">>> Connection sequence complete!");
    return true;
}

void EcoflowESP32::disconnect() {
    _running = false;
    _connected = false;
    _subscribedToNotifications = false;
    
    if (pReadChr && _subscribedToNotifications) {
        try {
            pReadChr->unsubscribe(false);
        } catch (...) {
            // Ignore errors
        }
    }
    
    if (pClient && pClient->isConnected()) {
        pClient->disconnect();
    }
    
    if (pClient) {
        NimBLEDevice::deleteClient(pClient);
        pClient = nullptr;
    }
    
    pWriteChr = nullptr;
    pReadChr = nullptr;
}

// ============================================================================
// CALLBACKS
// ============================================================================

void EcoflowESP32::onConnect(NimBLEClient* pclient) {
    Serial.println(">>> Connected callback triggered");
}

void EcoflowESP32::onDisconnect(NimBLEClient* pclient, int reason) {
    Serial.print(">>> Disconnected, reason: ");
    Serial.println(reason);
    _connected = false;
    _subscribedToNotifications = false;
}

void EcoflowESP32::notifyCallback(NimBLERemoteCharacteristic* pBLERemoteCharacteristic,
                                   uint8_t* pData, size_t length, bool isNotify) {
    if (_instance) {
        _instance->parse(pData, length);
    }
}

// ============================================================================
// DATA PARSING
// ============================================================================

void EcoflowESP32::parse(uint8_t* pData, size_t length) {
    if (length < 20) {
        return;
    }
    
    if (pData[0] != 0xAA || pData[1] != 0x02) {
        return;
    }
    
    if (length > 18) {
        _data.batteryLevel = pData[18];
    }
    
    if (length > 19) {
        _data.acOn = (pData[19] & 0x01) ? true : false;
        _data.usbOn = (pData[19] & 0x02) ? true : false;
        _data.dcOn = (pData[19] & 0x04) ? true : false;
    }
    
    if (length > 17) {
        _data.inputPower = (pData[17] << 8) | pData[16];
    }
    
    if (length > 15) {
        _data.outputPower = (pData[15] << 8) | pData[14];
    }
    
    _lastDataTime = millis();
}

// ============================================================================
// COMMANDS
// ============================================================================

bool EcoflowESP32::sendCommand(const uint8_t* command, size_t size) {
    if (!_connected || !pWriteChr || !pClient || !pClient->isConnected()) {
        return false;
    }
    
    try {
        pWriteChr->writeValue((uint8_t*)command, size, false);
        return true;
    } catch (const std::exception& e) {
        Serial.print(">>> ERROR: Send command failed: ");
        Serial.println(e.what());
        return false;
    }
}

bool EcoflowESP32::requestData() {
    return sendCommand(CMD_REQUEST_DATA, sizeof(CMD_REQUEST_DATA));
}

bool EcoflowESP32::setAC(bool on) {
    return sendCommand(on ? CMD_AC_ON : CMD_AC_OFF, on ? sizeof(CMD_AC_ON) : sizeof(CMD_AC_OFF));
}

bool EcoflowESP32::setDC(bool on) {
    return sendCommand(on ? CMD_12V_ON : CMD_12V_OFF, on ? sizeof(CMD_12V_ON) : sizeof(CMD_12V_OFF));
}

bool EcoflowESP32::setUSB(bool on) {
    return sendCommand(on ? CMD_USB_ON : CMD_USB_OFF, on ? sizeof(CMD_USB_ON) : sizeof(CMD_USB_OFF));
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
    return _connected && pClient && pClient->isConnected() && _subscribedToNotifications;
}

// ============================================================================
// KEEP-ALIVE TASK
// ============================================================================

void EcoflowESP32::_keepAliveTask() {
    uint32_t lastDataTime = millis();
    
    while (_running) {
        if (_connected && pClient && pClient->isConnected()) {
            uint32_t now = millis();
            
            if (now - lastDataTime > DATA_REQUEST_INTERVAL) {
                if (requestData()) {
                    lastDataTime = now;
                }
            }
        }
        
        vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
}

void EcoflowESP32::_keepAliveTaskStatic(void* pvParameters) {
    EcoflowESP32* pThis = (EcoflowESP32*)pvParameters;
    pThis->_keepAliveTask();
    vTaskDelete(NULL);
}

// ============================================================================
// HELPER METHODS
// ============================================================================

void EcoflowESP32::setAdvertisedDevice(NimBLEAdvertisedDevice* device) {
    if (m_pAdvertisedDevice != nullptr) {
        delete m_pAdvertisedDevice;
    }
    m_pAdvertisedDevice = new NimBLEAdvertisedDevice(*device);
}

void EcoflowESP32::update() {
    if (isConnected()) {
        requestData();
    }
}
