#include "EcoflowESP32.h"
#include "EcoflowProtocol.h"

// Static instance for callback
EcoflowESP32* EcoflowESP32::_instance = nullptr;

// ============================================================================
// SCAN CALLBACKS - Minimal logging to prevent serial buffer overflow
// ============================================================================

EcoflowScanCallbacks::EcoflowScanCallbacks(EcoflowESP32* pEcoflowESP32) 
    : _pEcoflowESP32(pEcoflowESP32) {}

void EcoflowScanCallbacks::onDiscovered(const NimBLEAdvertisedDevice* advertisedDevice) {
    // Only log if it's an Ecoflow device
    if (advertisedDevice->haveManufacturerData()) {
        if(advertisedDevice->getManufacturerData().length() >= 2) {
            uint16_t manufacturerId = (advertisedDevice->getManufacturerData()[1] << 8) | advertisedDevice->getManufacturerData()[0];
            if (manufacturerId == 46517) {
                _pEcoflowESP32->setAdvertisedDevice(const_cast<NimBLEAdvertisedDevice*>(advertisedDevice));
                // Silent detection - main.cpp will handle logging
                return;
            }
        }
    }
    // Silent for non-Ecoflow devices
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
    
    // Configure security settings
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    NimBLEDevice::setSecurityAuth(true, false, false);
    
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
    pWriteChr = nullptr;
    pReadChr = nullptr;
    
    // Try primary service UUID
    NimBLERemoteService* pService = pClient->getService("70D51000-2C7F-4E75-AE8A-D758951CE4E0");
    
    if (pService) {
        pWriteChr = pService->getCharacteristic("70D51001-2C7F-4E75-AE8A-D758951CE4E0");
        pReadChr = pService->getCharacteristic("70D51002-2C7F-4E75-AE8A-D758951CE4E0");
        
        if (pWriteChr && pReadChr) {
            return true;
        }
    }
    
    // Try alternate service UUID
    pService = pClient->getService("00001801-0000-1000-8000-00805f9b34fb");
    
    if (pService) {
        pWriteChr = pService->getCharacteristic("0000ff01-0000-1000-8000-00805f9b34fb");
        pReadChr = pService->getCharacteristic("0000ff02-0000-1000-8000-00805f9b34fb");
        
        if (pWriteChr && pReadChr) {
            return true;
        }
    }
    
    return false;
}

bool EcoflowESP32::connectToServer() {
    if (m_pAdvertisedDevice == nullptr) {
        return false;
    }
    
    if (pClient == nullptr) {
        pClient = NimBLEDevice::createClient();
        pClient->setClientCallbacks(this, false);
    }
    
    if (pClient->isConnected()) {
        return true;
    }
    
    if (!pClient->connect(m_pAdvertisedDevice, false)) {
        return false;
    }
    
    _connected = true;
    
    // Request MTU
    pClient->exchangeMTU();
    
    // Resolve characteristics
    delay(500);
    
    if (!_resolveCharacteristics()) {
        pClient->disconnect();
        _connected = false;
        return false;
    }
    
    // Verify capabilities
    if (!pWriteChr->canWrite() || !pReadChr->canNotify()) {
        pClient->disconnect();
        _connected = false;
        return false;
    }
    
    // Subscribe to notifications
    if (!pReadChr->subscribe(true, EcoflowESP32::notifyCallback)) {
        pClient->disconnect();
        _connected = false;
        return false;
    }
    
    _subscribedToNotifications = true;
    _lastDataTime = millis();
    
    // Start keep-alive task
    if (_keepAliveTaskHandle == nullptr) {
        xTaskCreate(
            _keepAliveTaskStatic,
            "EcoflowKeepAlive",
            4096,
            this,
            2,
            &_keepAliveTaskHandle
        );
    }
    
    _running = true;
    _authenticated = true;
    
    return true;
}

void EcoflowESP32::disconnect() {
    _running = false;
    _connected = false;
    _authenticated = false;
    _subscribedToNotifications = false;
    
    if (pClient && pClient->isConnected()) {
        if (pReadChr) {
            pReadChr->unsubscribe();
        }
        pClient->disconnect();
    }
    
    pWriteChr = nullptr;
    pReadChr = nullptr;
}

// ============================================================================
// CONNECTION CALLBACKS
// ============================================================================

void EcoflowESP32::onConnect(NimBLEClient* pclient) {
}

void EcoflowESP32::onDisconnect(NimBLEClient* pclient, int reason) {
    _connected = false;
    _authenticated = false;
    _subscribedToNotifications = false;
}

// ============================================================================
// NOTIFICATION HANDLING
// ============================================================================

void EcoflowESP32::notifyCallback(NimBLERemoteCharacteristic* pBLERemoteCharacteristic,
                                   uint8_t* pData, size_t length, bool isNotify) {
    if (_instance) {
        _instance->parse(pData, length);
    }
}

void EcoflowESP32::parse(uint8_t* pData, size_t length) {
    if (length < 20) {
        return;
    }
    
    // Parse battery level (byte 18)
    if (length > 18) {
        _data.batteryLevel = pData[18];
    }
    
    // Parse input power (bytes 12-13, big-endian)
    if (length > 13) {
        _data.inputPower = (pData[12] << 8) | pData[13];
    }
    
    // Parse output power (bytes 14-15, big-endian)
    if (length > 15) {
        _data.outputPower = (pData[14] << 8) | pData[15];
    }
    
    // Parse accessory status (byte 19 - bitmask)
    if (length > 19) {
        uint8_t status = pData[19];
        _data.acOn = (status & 0x01) != 0;
        _data.usbOn = (status & 0x02) != 0;
        _data.dcOn = (status & 0x04) != 0;
    }
    
    _lastDataTime = millis();
}

// ============================================================================
// KEEP-ALIVE TASK
// ============================================================================

void EcoflowESP32::_keepAliveTaskStatic(void* pvParameters) {
    EcoflowESP32* pThis = (EcoflowESP32*)pvParameters;
    pThis->_keepAliveTask();
    vTaskDelete(nullptr);
}

void EcoflowESP32::_keepAliveTask() {
    while (_running) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        
        if (_connected && _authenticated && isConnected()) {
            // Process command queue
            taskENTER_CRITICAL(&_queueMutex);
            while (!_commandQueue.empty()) {
                Command cmd = _commandQueue.front();
                _commandQueue.pop();
                
                taskEXIT_CRITICAL(&_queueMutex);
                
                if (pWriteChr && pClient->isConnected()) {
                    pWriteChr->writeValue(cmd.data, cmd.length, false);
                }
                
                taskENTER_CRITICAL(&_queueMutex);
            }
            taskEXIT_CRITICAL(&_queueMutex);
            
            // Request data if no data received recently
            if ((millis() - _lastDataTime) > (DATA_REQUEST_INTERVAL * 2)) {
                requestData();
            }
        } else if (!_running) {
            break;
        }
    }
}

// ============================================================================
// COMMAND SENDING
// ============================================================================

bool EcoflowESP32::sendCommand(const uint8_t* command, size_t size) {
    if (size > 64 || !_connected || !pWriteChr) {
        return false;
    }
    
    Command cmd;
    memcpy(cmd.data, command, size);
    cmd.length = size;
    
    taskENTER_CRITICAL(&_queueMutex);
    _commandQueue.push(cmd);
    taskEXIT_CRITICAL(&_queueMutex);
    
    return true;
}

bool EcoflowESP32::requestData() {
    return sendCommand(CMD_REQUEST_DATA, sizeof(CMD_REQUEST_DATA));
}

bool EcoflowESP32::setAC(bool on) {
    return sendCommand(on ? CMD_AC_ON : CMD_AC_OFF, sizeof(CMD_AC_ON));
}

bool EcoflowESP32::setDC(bool on) {
    return sendCommand(on ? CMD_12V_ON : CMD_12V_OFF, sizeof(CMD_12V_ON));
}

bool EcoflowESP32::setUSB(bool on) {
    return sendCommand(on ? CMD_USB_ON : CMD_USB_OFF, sizeof(CMD_USB_ON));
}

// ============================================================================
// GETTERS
// ============================================================================

int EcoflowESP32::getBatteryLevel() {
    return _data.batteryLevel;
}

int EcoflowESP32::getInputPower() {
    return _data.inputPower;
}

int EcoflowESP32::getOutputPower() {
    return _data.outputPower;
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
    return _connected && _authenticated && (pClient != nullptr) && pClient->isConnected();
}

// ============================================================================
// UTILITIES
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
