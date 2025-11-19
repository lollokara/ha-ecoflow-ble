#include "EcoflowESP32.h"
#include "EcoflowProtocol.h"

EcoflowScanCallbacks::EcoflowScanCallbacks(EcoflowESP32* pEcoflowESP32) : _pEcoflowESP32(pEcoflowESP32) {}

void EcoflowScanCallbacks::onDiscovered(const NimBLEAdvertisedDevice* advertisedDevice) {
    Serial.print("Found device: ");
    Serial.print(advertisedDevice->getAddress().toString().c_str());
    Serial.print(" Name: ");
    Serial.print(advertisedDevice->getName().c_str());

    if (advertisedDevice->haveManufacturerData()) {
        std::string manufacturerData = advertisedDevice->getManufacturerData();
        Serial.print(" Manufacturer Data: ");
        for(int i=0; i<manufacturerData.length(); i++) {
            Serial.printf("%02X ", (uint8_t)manufacturerData[i]);
        }

        if (advertisedDevice->getManufacturerData().length() >= 2) {
            uint16_t manufacturerId = (advertisedDevice->getManufacturerData()[1] << 8) | advertisedDevice->getManufacturerData()[0];
            if (manufacturerId == 46517) { // Ecoflow manufacturer ID
                Serial.println(" - Ecoflow device found!");
                _pEcoflowESP32->setAdvertisedDevice(const_cast<NimBLEAdvertisedDevice*>(advertisedDevice));
            }
        }
    }

    if(advertisedDevice->haveServiceUUID()) {
        Serial.print(" Service UUIDs: ");
        for (int i = 0; i < advertisedDevice->getServiceUUIDCount(); i++) {
            Serial.print(advertisedDevice->getServiceUUID(i).toString().c_str());
            Serial.print(" ");
        }
    }
    Serial.println();
}

EcoflowESP32* EcoflowESP32::_instance = nullptr;

EcoflowESP32::EcoflowESP32() : pClient(nullptr), m_deviceAddressType(0)
{
    _instance = this;
    _scanCallbacks = new EcoflowScanCallbacks(this);
}

EcoflowESP32::~EcoflowESP32()
{
    delete _scanCallbacks;
}

void EcoflowESP32::setAdvertisedDevice(NimBLEAdvertisedDevice* device) {
    if (m_deviceAddress.empty()) {
        m_deviceAddress = device->getAddress().toString();
        m_deviceAddressType = device->getAddressType();
        NimBLEDevice::getScan()->stop();
    }
}

bool EcoflowESP32::begin()
{
    Serial.println(">>>>>>>>>>>>> EcoflowESP32 Library Version: 3.0 <<<<<<<<<<<<<");
    NimBLEDevice::init("");
    NimBLEDevice::setMTU(136);
    NimBLEDevice::setSecurityIOCap(BLE_HS_IO_NO_INPUT_OUTPUT);
    NimBLEDevice::setSecurityAuth(true, true, true);
    return true;
}

bool EcoflowESP32::scan(uint32_t scanTime) {
    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setScanCallbacks(_scanCallbacks);
    pScan->setInterval(100);
    pScan->setWindow(99);
    pScan->setActiveScan(true);
    pScan->start(scanTime, false);
    return !m_deviceAddress.empty();
}


void EcoflowESP32::onConnect(NimBLEClient* pclient) {
  Serial.println("Connected");
}

void EcoflowESP32::onDisconnect(NimBLEClient* pclient, int reason) {
  Serial.print("Disconnected, reason: ");
  Serial.println(reason);
}

void EcoflowESP32::onAuthenticationComplete(ble_gap_conn_desc* desc) {
    if (!desc->sec_state.encrypted) {
        Serial.println("Authentication failed");
        return;
    }

    Serial.println("Authentication successful");
}

bool EcoflowESP32::connectToDevice(NimBLEAdvertisedDevice* device) {
    m_pAdvertisedDevice = device;
    return connectToServer();
}

bool EcoflowESP32::connectToServer() {
    if (pClient == nullptr) {
        pClient = NimBLEDevice::createClient();
        pClient->setClientCallbacks(this);
    }

    Serial.println("Attempting to connect…");
    if (m_deviceAddress.empty()) {
        Serial.println("No device address set");
        return false;
    }

    if (!pClient->connect(NimBLEAddress(m_deviceAddress, m_deviceAddressType))) {
        Serial.println("Failed to connect");
        return false;
    }

    if (!pClient->secureConnection()) {
        Serial.println("Failed to secure connection");
        return false;
    }

    if (!pClient->exchangeMTU()) {
        Serial.println("Failed to set MTU");
        return false;
    }
    pWriteChr = nullptr;
    pReadChr = nullptr;

    NimBLERemoteService* pService = nullptr;

    // Try first service UUID
    pService = pClient->getService(serviceUUID1);
    if (pService) {
        pWriteChr = pService->getCharacteristic(writeCharUUID1);
        pReadChr = pService->getCharacteristic(readCharUUID1);
    }

    // If characteristics not found, try second service UUID
    if (!pWriteChr || !pReadChr) {
        pService = pClient->getService(serviceUUID2);
        if (pService) {
            pWriteChr = pService->getCharacteristic(writeCharUUID2);
            pReadChr = pService->getCharacteristic(readCharUUID2);
        }
    }

    if (!pWriteChr || !pReadChr) {
        Serial.println("Failed to find characteristics");
        pClient->disconnect();
        return false;
    }

    Serial.println("Found characteristics");

    if(pReadChr->canNotify()) {
        if(pReadChr->subscribe(true, notifyCallback)) {
            Serial.println("Subscribed to notifications");
            return true;
        } else {
            Serial.println("Failed to subscribe to notifications");
            pClient->disconnect();
            return false;
        }
    }

    Serial.println("Read characteristic does not support notifications");
    return false;
}

bool EcoflowESP32::sendCommand(const uint8_t* command, size_t size) {
    if (!pClient->isConnected() || !pWriteChr) {
        return false;
    }

    return pWriteChr->writeValue(command, size, false);
}

void EcoflowESP32::notifyCallback(NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    if (_instance) {
        _instance->parse(pData, length);
    }
}

void EcoflowESP32::parse(uint8_t* pData, size_t length) {
    // TODO: This parsing is likely incorrect and needs to be updated based on the protocol
    if (length < 20) { // Reduced length check for now
        return;
    }

    Serial.print("Received data: ");
    for(int i=0; i<length; i++) {
        Serial.printf("%02X ", pData[i]);
    }
    Serial.println();
}

void EcoflowESP32::setAC(bool on) {
    if (on) {
        sendCommand(CMD_AC_ON, sizeof(CMD_AC_ON));
    } else {
        sendCommand(CMD_AC_OFF, sizeof(CMD_AC_OFF));
    }
}

void EcoflowESP32::setDC(bool on) {
    if (on) {
        sendCommand(CMD_12V_ON, sizeof(CMD_12V_ON));
    } else {
        sendCommand(CMD_12V_OFF, sizeof(CMD_12V_OFF));
    }
}

void EcoflowESP32::setUSB(bool on) {
    if (on) {
        sendCommand(CMD_USB_ON, sizeof(CMD_USB_ON));
    } else {
        sendCommand(CMD_USB_OFF, sizeof(CMD_USB_OFF));
    }
}

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