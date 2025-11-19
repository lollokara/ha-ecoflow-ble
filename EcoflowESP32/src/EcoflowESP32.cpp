/*
 * EcoflowESP32_DISCOVERY_FIXED.cpp - SERVICE DISCOVERY DIAGNOSTIC (FIXED)
 * 
 * This version enumerates ALL services and characteristics on the Delta 3
 * using simpler UUID printing that works with all NimBLE versions.
 */

#include "EcoflowESP32.h"
#include "EcoflowProtocol.h"
#include <string>

// Static instance for callback
EcoflowESP32* EcoflowESP32::_instance = nullptr;

// Connection parameters
static const uint8_t CONNECT_RETRIES = 3;
static const uint32_t CONNECT_RETRY_DELAY = 250;

// ============================================================================
// HELPER: Print UUID using toString() method
// ============================================================================

static void printUUID(const NimBLEUUID& uuid) {
    Serial.print(uuid.toString().c_str());
}

// ============================================================================
// HELPER: Print characteristic properties
// ============================================================================

static void printCharProperties(NimBLERemoteCharacteristic* pChr) {
    if (!pChr) return;
    
    Serial.print(" [");
    if (pChr->canRead()) Serial.print("R");
    if (pChr->canWrite()) Serial.print("W");
    if (pChr->canWriteNoResponse()) Serial.print("w");
    if (pChr->canNotify()) Serial.print("N");
    if (pChr->canIndicate()) Serial.print("I");
    Serial.print("]");
}

// ============================================================================
// DUMP ALL SERVICES - SIMPLIFIED VERSION (FIXED METHOD NAME)
// ============================================================================

void EcoflowESP32::dumpAllServices() {
    if (!pClient || !pClient->isConnected()) {
        Serial.println(">>> ERROR: Not connected, cannot dump services");
        return;
    }
    
    Serial.println("\n╔════════════════════════════════════════════════════════╗");
    Serial.println("║ FULL SERVICE & CHARACTERISTIC ENUMERATION             ║");
    Serial.println("╚════════════════════════════════════════════════════════╝\n");
    
    // Get all services - FIXED: use getServices() not getAllServices()
    std::vector<NimBLERemoteService*>* pServices = pClient->getServices(true);
    
    if (!pServices || pServices->empty()) {
        Serial.println(">>> No services found");
        return;
    }
    
    Serial.print(">>> Found ");
    Serial.print(pServices->size());
    Serial.println(" services:\n");
    
    for (size_t i = 0; i < pServices->size(); i++) {
        NimBLERemoteService* pService = pServices->at(i);
        
        Serial.print("SERVICE [");
        Serial.print(i);
        Serial.print("]: ");
        printUUID(pService->getUUID());
        Serial.println();
        
        // Get all characteristics for this service
        std::vector<NimBLERemoteCharacteristic*>* pChars = pService->getCharacteristics(true);
        
        if (pChars && !pChars->empty()) {
            Serial.print("  Characteristics: ");
            Serial.print(pChars->size());
            Serial.println();
            
            for (size_t j = 0; j < pChars->size(); j++) {
                NimBLERemoteCharacteristic* pChr = pChars->at(j);
                
                Serial.print("    [");
                Serial.print(j);
                Serial.print("]: ");
                printUUID(pChr->getUUID());
                printCharProperties(pChr);
                Serial.println();
            }
        } else {
            Serial.println("  (No characteristics)");
        }
        
        Serial.println();
    }
    
    Serial.println("╔════════════════════════════════════════════════════════╗");
    Serial.println("║ END SERVICE ENUMERATION - Copy the UUIDs above!       ║");
    Serial.println("╚════════════════════════════════════════════════════════╝\n");
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
}

// ============================================================================
// INITIALIZATION
// ============================================================================

bool EcoflowESP32::begin() {
    try {
        NimBLEDevice::init("");
        Serial.println(">>> BLE Stack initialized");
        
        // Enhanced security settings for better compatibility
        NimBLEDevice::setOwnAddrType(BLE_OWN_ADDR_PUBLIC);
        NimBLEDevice::setMTU(247);
        NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
        NimBLEDevice::setSecurityAuth(false, false, false);
        
        Serial.println(">>> Security settings configured");
        Serial.println(">>>   - Own address type: PUBLIC");
        Serial.println(">>>   - MTU: 247 bytes");
        Serial.println(">>>   - Security: Disabled (no pairing required)");
        
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
        pScan->setMaxResults(20);  // Store up to 20 devices
        
        Serial.print(">>> Starting BLE scan for ");
        Serial.print(scanTime);
        Serial.println(" seconds...");
        
        uint32_t startTime = millis();
        NimBLEScanResults results = pScan->start(scanTime, false);
        uint32_t elapsed = millis() - startTime;
        
        uint32_t count = results.getCount();
        Serial.print(">>> Scan found ");
        Serial.print(count);
        Serial.println(" devices");
        
        for (uint32_t i = 0; i < count; i++) {
            NimBLEAdvertisedDevice device = results.getDevice(i);
            
            Serial.print(">>> Device [");
            Serial.print(i);
            Serial.print("]: ");
            Serial.println(device.getAddress().toString().c_str());
            
            if (device.haveManufacturerData()) {
                try {
                    NimBLEAdvertisedDevice* pDevice = new NimBLEAdvertisedDevice(device);
                    std::string mfgData = pDevice->getManufacturerData();
                    
                    if (mfgData.length() >= 2) {
                        uint16_t manufacturerId = (mfgData[1] << 8) | mfgData[0];
                        
                        Serial.print(">>>   Manufacturer ID: ");
                        Serial.println(manufacturerId);
                        
                        if (manufacturerId == ECOFLOW_MANUFACTURER_ID) {
                            Serial.print(">>> ✓ ECOFLOW DEVICE FOUND!");
                            Serial.print(" Address: ");
                            Serial.println(device.getAddress().toString().c_str());
                            
                            m_pAdvertisedDevice = pDevice;
                            
                            Serial.print(">>> Device found in ");
                            Serial.print(elapsed);
                            Serial.println("ms");
                            Serial.print(">>> Device address: ");
                            Serial.println(m_pAdvertisedDevice->getAddress().toString().c_str());
                            
                            return true;
                        }
                    }
                    delete pDevice;
                } catch (const std::exception& e) {
                    Serial.print(">>>   Error reading mfg data: ");
                    Serial.println(e.what());
                }
            }
        }
        
        Serial.print(">>> No Ecoflow device found after ");
        Serial.print(elapsed);
        Serial.println("ms");
        return false;
        
    } catch (const std::exception& e) {
        Serial.print(">>> ERROR during scan: ");
        Serial.println(e.what());
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

    try {
        // Try primary Ecoflow service UUID
        NimBLERemoteService* pService = pClient->getService(SERVICE_UUID_ECOFLOW);
        if (pService) {
            Serial.println(">>> Found primary Ecoflow service");
            pWriteChr = pService->getCharacteristic(CHAR_WRITE_UUID_ECOFLOW);
            pReadChr = pService->getCharacteristic(CHAR_READ_UUID_ECOFLOW);
            if (pWriteChr && pReadChr) {
                Serial.println(">>> Found both write and read characteristics (primary)");
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
                Serial.println(">>> Found both write and read characteristics (alternate)");
                return true;
            }
        }

        Serial.println(">>> Services not yet loaded, will retry");
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

    if (pClient != nullptr && pClient->isConnected()) {
        if (pReadChr && pWriteChr) {
            Serial.println(">>> Already connected with valid characteristics");
            _connected = true;
            return true;
        }
    }

    if (pClient != nullptr) {
        Serial.println(">>> Cleaning up stale connection");
        try {
            if (pClient->isConnected()) {
                pClient->disconnect();
            }
            NimBLEDevice::deleteClient(pClient);
        } catch (...) {
            // Ignore
        }
        pClient = nullptr;
        pWriteChr = nullptr;
        pReadChr = nullptr;
    }

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

            pClient->setClientCallbacks(this, false);
            
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
                } catch (...) {
                    // Ignore
                }
                pClient = nullptr;
                if (attempt < CONNECT_RETRIES) {
                    Serial.print(">>> Waiting ");
                    Serial.print(CONNECT_RETRY_DELAY);
                    Serial.println("ms before retry...");
                    delay(CONNECT_RETRY_DELAY);
                }
            }
        } catch (const std::exception& e) {
            Serial.print(">>> Exception during connection attempt: ");
            Serial.println(e.what());
            if (attempt < CONNECT_RETRIES) {
                delay(CONNECT_RETRY_DELAY);
            }
        }
    }

    if (!pClient || !pClient->isConnected()) {
        Serial.println(">>> ERROR: Failed to connect after all retries");
        if (pClient) {
            try {
                NimBLEDevice::deleteClient(pClient);
            } catch (...) {
                // Ignore
            }
            pClient = nullptr;
        }
        return false;
    }

    // ========================================================================
    // DIAGNOSTIC: Dump all services immediately
    // ========================================================================
    Serial.println("\n>>> CONNECTION SUCCESSFUL - DUMPING ALL SERVICES NOW\n");
    delay(500);  // Give device time to prepare
    dumpAllServices();
    
    // Then try to find services as before
    Serial.println(">>> Now attempting to discover specific services...");
    
    uint8_t retries = 5;
    uint32_t startTime = millis();
    while (retries > 0 && millis() - startTime < 10000) {
        delay(500);
        if (_resolveCharacteristics()) {
            uint32_t discoveryTime = millis() - startTime;
            Serial.print(">>> Services discovered in ");
            Serial.print(discoveryTime);
            Serial.println("ms");
            break;
        }
        retries--;
        if (retries > 0) {
            Serial.print(">>> Service discovery retry (");
            Serial.print(retries);
            Serial.println(" remaining)");
        }
    }

    if (!pWriteChr || !pReadChr) {
        Serial.println(">>> WARNING: Could not find expected characteristics");
        Serial.println(">>> Check the service dump above for correct UUIDs");
        Serial.println(">>> Update EcoflowProtocol.h with the UUIDs you see above");
        
        // Don't fail completely - let user see what's available
        _connected = true;
        _authenticated = false;
        return true;  // Allow manual testing
    }

    Serial.println(">>> ✓ Connected to device and services resolved");
    Serial.print(">>> MTU: ");
    Serial.println(pClient->getMTU());
    Serial.println(">>> Device is ready for manual command testing");
    
    _subscribedToNotifications = false;
    _connected = true;
    _authenticated = true;
    _lastDataTime = millis();

    Serial.println(">>> Connection sequence COMPLETE");
    return true;
}

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
    } catch (...) {
        // Ignore
    }
    
    pWriteChr = nullptr;
    pReadChr = nullptr;
}

// ============================================================================
// CALLBACKS
// ============================================================================

void EcoflowESP32::onConnect(NimBLEClient* pclient) {
    uint32_t now = millis();
    Serial.print(">>> [");
    Serial.print(now);
    Serial.println("] Connected callback triggered");
    if (pclient) {
        Serial.print(">>>   - MTU: ");
        Serial.println(pclient->getMTU());
    }
}

void EcoflowESP32::onDisconnect(NimBLEClient* pclient) {
    uint32_t now = millis();
    Serial.print(">>> [");
    Serial.print(now);
    Serial.println("] Disconnected callback triggered");
    
    _connected = false;
    _subscribedToNotifications = false;
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
        Serial.println(">>> ERROR: Not connected, cannot send command");
        return false;
    }

    try {
        pWriteChr->writeValue((uint8_t*)command, size, false);
        Serial.print(">>> Command sent (");
        Serial.print(size);
        Serial.println(" bytes)");
        return true;
    } catch (const std::exception& e) {
        Serial.print(">>> ERROR: Send command failed: ");
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
    return sendCommand(on ? CMD_AC_ON : CMD_AC_OFF, on ? sizeof(CMD_AC_ON) : sizeof(CMD_AC_OFF));
}

bool EcoflowESP32::setDC(bool on) {
    Serial.print(">>> Setting DC 12V to ");
    Serial.println(on ? "ON" : "OFF");
    return sendCommand(on ? CMD_12V_ON : CMD_12V_OFF, on ? sizeof(CMD_12V_ON) : sizeof(CMD_12V_OFF));
}

bool EcoflowESP32::setUSB(bool on) {
    Serial.print(">>> Setting USB to ");
    Serial.println(on ? "ON" : "OFF");
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
    return _connected && pClient && pClient->isConnected();
}

// ============================================================================
// KEEP-ALIVE TASK (POLLING) - DISABLED FOR DEBUG
// ============================================================================

void EcoflowESP32::_keepAliveTask() {
    // DEBUG: Not running in debug mode
}

void EcoflowESP32::_keepAliveTaskStatic(void* pvParameters) {
    // DEBUG: Not called in debug mode
}

// ============================================================================
// HELPER METHODS
// ============================================================================

void EcoflowESP32::setAdvertisedDevice(NimBLEAdvertisedDevice* device) {
    if (m_pAdvertisedDevice != nullptr) {
        delete m_pAdvertisedDevice;
    }
    if (device != nullptr) {
        m_pAdvertisedDevice = new NimBLEAdvertisedDevice(*device);
    }
}

void EcoflowESP32::update() {
    // DEBUG: Manual command testing only, no auto-update
}
