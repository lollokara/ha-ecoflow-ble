/*
 * EcoflowESP32_WORKING.cpp - Corrected for your NimBLE version
 * 
 * Issues fixed:
 * 1. onNotify is not in NimBLEClientCallbacks - use notify_callback function instead
 * 2. subscribe() expects a lambda/function pointer, not a class instance
 * 3. Removed duplicate getInstance() definition
 */

#include "EcoflowESP32.h"
#include "EcoflowProtocol.h"
#include <Arduino.h>

// Static instance for callback
EcoflowESP32* EcoflowESP32::_instance = nullptr;

static const uint8_t CONNECT_RETRIES = 3;
static const uint32_t CONNECT_RETRY_DELAY = 250;
static const uint32_t SERVICE_DISCOVERY_DELAY = 500;

// ============================================================================
// FORWARD DECLARATION
// ============================================================================
void handleNotificationCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic,
                                uint8_t* pData,
                                size_t length,
                                bool isNotify);

// ============================================================================
// CLIENT CALLBACKS - For connection/disconnection only
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
      _lastDataTime(0) {
    _instance = this;
    memset(&_data, 0, sizeof(EcoflowData));
}

EcoflowESP32::~EcoflowESP32() {
    _running = false;
    if (_keepAliveTaskHandle != nullptr) {
        vTaskDelete(_keepAliveTaskHandle);
    }
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

        // Set up global callback handler
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

            // Register callback handler for connect/disconnect
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

    // Check if we successfully connected
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

    // ========================================================================
    // SERVICE DISCOVERY DELAY
    // ========================================================================
    Serial.println(">>> Waiting for service discovery...");
    delay(SERVICE_DISCOVERY_DELAY);

    // Try to resolve characteristics
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

    // Check if we found the characteristics
    if (!pWriteChr || !pReadChr) {
        Serial.println(">>> ERROR: Could not find characteristics");
        return false;
    }

    // ========================================================================
    // CRITICAL: ENABLE NOTIFICATIONS ON READ CHARACTERISTIC
    // ========================================================================
    Serial.println(">>> Enabling notifications...");
    
    try {
        // This is the KEY line - subscribe with lambda callback
        // When Delta 3 sends data, this lambda is called
        pReadChr->subscribe(true, [](NimBLERemoteCharacteristic* pRemoteCharacteristic,
                                      uint8_t* pData,
                                      size_t length,
                                      bool isNotify) {
            handleNotificationCallback(pRemoteCharacteristic, pData, length, isNotify);
        }, false);
        
        Serial.println(">>> ✓ Notifications enabled");
        _subscribedToNotifications = true;
    } catch (const std::exception& e) {
        Serial.print(">>> ERROR: Failed to enable notifications: ");
        Serial.println(e.what());
        return false;
    }

    // ========================================================================
    // SUCCESS
    // ========================================================================
    Serial.println(">>> ✓ Connected to Delta 3");
    Serial.print(">>> MTU: ");
    Serial.println(pClient->getMTU());
    Serial.println(">>> Waiting for device data...");

    _connected = true;
    _authenticated = true;
    _lastDataTime = millis();

    return true;
}

// ============================================================================
// NOTIFICATION HANDLER (Global function called by lambda)
// ============================================================================
void handleNotificationCallback(NimBLERemoteCharacteristic* pRemoteCharacteristic,
                                uint8_t* pData,
                                size_t length,
                                bool isNotify) {
    if (!pRemoteCharacteristic || !pData || length == 0) {
        return;
    }

    // Log that we received data
    Serial.print(">>> [NOTIFY] Received ");
    Serial.print(length);
    Serial.print(" bytes: ");
    for (size_t i = 0; i < (length < 20 ? length : 20); i++) {
        if (pData[i] < 0x10) Serial.print("0");
        Serial.print(pData[i], HEX);
        Serial.print(" ");
    }
    if (length > 20) Serial.print("...");
    Serial.println();

    // Pass to instance handler
    if (EcoflowESP32::getInstance()) {
        EcoflowESP32::getInstance()->onNotify(pRemoteCharacteristic, pData, length, isNotify);
    }
}

// ============================================================================
// INSTANCE NOTIFICATION HANDLER
// ============================================================================
void EcoflowESP32::onNotify(NimBLERemoteCharacteristic* pRemoteCharacteristic,
                            uint8_t* pData,
                            size_t length,
                            bool isNotify) {
    if (!pRemoteCharacteristic || !pData || length == 0) {
        return;
    }

    // Parse the incoming data from Delta 3
    parse(pData, length);
    _lastDataTime = millis();
}

// ============================================================================
// CALLBACKS
// ============================================================================
void EcoflowESP32::onConnect(NimBLEClient* pclient) {
    Serial.print(">>> [");
    Serial.print(millis());
    Serial.println("] Connected callback triggered");
    if (pclient) {
        Serial.print(">>>   - MTU: ");
        Serial.println(pclient->getMTU());
    }
}

void EcoflowESP32::onDisconnect(NimBLEClient* pclient) {
    Serial.print(">>> [");
    Serial.print(millis());
    Serial.println("] Disconnected callback triggered");
    _connected = false;
    _subscribedToNotifications = false;
}

// ============================================================================
// DISCONNECTION
// ============================================================================
void EcoflowESP32::disconnect() {
    _running = false;
    _connected = false;
    _subscribedToNotifications = false;

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
// DATA PARSING
// ============================================================================
void EcoflowESP32::parse(uint8_t* pData, size_t length) {
    if (length < 20) {
        return;
    }

    // Check frame header
    if (pData[0] != 0xAA || pData[1] != 0x02) {
        return;
    }

    // Extract data fields
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
        pWriteChr->writeValue((uint8_t*)command, size, false);
        Serial.print(">>> Command sent (");
        Serial.print(size);
        Serial.println(" bytes)");
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
    return sendCommand(on ? CMD_AC_ON : CMD_AC_OFF, 
                      on ? sizeof(CMD_AC_ON) : sizeof(CMD_AC_OFF));
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

// ============================================================================
// HELPER METHODS
// ============================================================================
void EcoflowESP32::update() {
    // Periodic update
}
