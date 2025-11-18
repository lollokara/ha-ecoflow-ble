#include "EcoflowESP32.h"
#include "EcoflowProtocol.h"

// UUIDs from the Python library
static NimBLEUUID serviceUUID1("00001801-0000-1000-8000-00805f9b34fb");
static NimBLEUUID serviceUUID2("70D51000-2C7F-4E75-AE8A-D758951CE4E0");

static NimBLEUUID writeCharUUID1("70D51001-2C7F-4E75-AE8A-D758951CE4E0");
static NimBLEUUID readCharUUID1("70D51002-2C7F-4E75-AE8A-D758951CE4E0");

static NimBLEUUID writeCharUUID2("0000ff01-0000-1000-8000-00805f9b34fb");
static NimBLEUUID readCharUUID2("0000ff02-0000-1000-8000-00805f9b34fb");


EcoflowScanCallbacks::EcoflowScanCallbacks(EcoflowESP32* pEcoflowESP32) : _pEcoflowESP32(pEcoflowESP32) {}

void EcoflowScanCallbacks::onDiscovered(const NimBLEAdvertisedDevice* advertisedDevice) {
    Serial.println("onDiscovered: Found a device");
    Serial.print("Device address: ");
    Serial.println(advertisedDevice->getAddress().toString().c_str());

    if (advertisedDevice->haveManufacturerData()) {
        std::string manufacturerData = advertisedDevice->getManufacturerData();
        if (manufacturerData.length() >= 2) {
            uint16_t manufacturerId = (static_cast<uint8_t>(manufacturerData[1]) << 8) | static_cast<uint8_t>(manufacturerData[0]);
             if (manufacturerId == 0xB5B5) {
                Serial.println("onDiscovered: Ecoflow device found!");
                _pEcoflowESP32->setAdvertisedDevice(const_cast<NimBLEAdvertisedDevice*>(advertisedDevice));
            }
        }
    }
}

EcoflowESP32* EcoflowESP32::_instance = nullptr;

EcoflowESP32::EcoflowESP32() : pClient(nullptr)
{
    _instance = this;
    _scanCallbacks = new EcoflowScanCallbacks(this);
}

EcoflowESP32::~EcoflowESP32()
{
    delete _scanCallbacks;
    if(m_pDeviceAddress) {
        delete m_pDeviceAddress;
    }
}

void EcoflowESP32::setAdvertisedDevice(NimBLEAdvertisedDevice* device) {
    Serial.println("setAdvertisedDevice: Setting device");
    if (m_pDeviceAddress == nullptr) {
        m_pDeviceAddress = new NimBLEAddress(device->getAddress());
        Serial.println("setAdvertisedDevice: Device address set");
    }
}

bool EcoflowESP32::begin()
{
    Serial.println(">>>>>>>>>>>>> EcoflowESP32 Library Version: 4.0 <<<<<<<<<<<<<");
    NimBLEDevice::init("");
    return true;
}

bool EcoflowESP32::scan(uint32_t scanTime) {
    Serial.println("scan: Starting scan");
    if(m_pDeviceAddress) {
        delete m_pDeviceAddress;
        m_pDeviceAddress = nullptr;
    }
    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setScanCallbacks(_scanCallbacks);
    pScan->setInterval(100);
    pScan->setWindow(99);
    pScan->setActiveScan(true);
    pScan->start(scanTime, false);
    Serial.println("scan: Scan finished");
    return m_pDeviceAddress != nullptr;
}


void EcoflowESP32::onConnect(NimBLEClient* pclient) {
    Serial.println("onConnect: Connected");
    if(pclient->discoverAttributes()) {
        Serial.println("onConnect: Attributes discovered");

        NimBLERemoteService* pService = nullptr;
        pService = pclient->getService(serviceUUID2);
        if (pService) {
            pWriteChr = pService->getCharacteristic(writeCharUUID1);
            pReadChr = pService->getCharacteristic(readCharUUID1);
        }

        if (!pWriteChr || !pReadChr) {
            pService = pclient->getService(serviceUUID1);
            if (pService) {
                pWriteChr = pService->getCharacteristic(writeCharUUID2);
                pReadChr = pService->getCharacteristic(readCharUUID2);
            }
        }

        if (!pWriteChr || !pReadChr) {
            Serial.println("onConnect: Failed to get characteristics");
            pclient->disconnect();
            return;
        }

        Serial.println("onConnect: Characteristics found");
        Serial.printf("Write characteristic: %s\n", pWriteChr->getUUID().toString().c_str());
        Serial.printf("Read characteristic: %s\n", pReadChr->getUUID().toString().c_str());

        if(pReadChr->canNotify()) {
            Serial.println("onConnect: Subscribing to notifications...");
            if(!pReadChr->subscribe(true, notifyCallback)) {
                Serial.println("onConnect: Failed to subscribe to notifications");
                pclient->disconnect();
                return;
            }
        }

        Serial.println("onConnect: Sending request for data...");
        sendCommand(CMD_REQUEST_DATA, sizeof(CMD_REQUEST_DATA));

    } else {
        Serial.println("onConnect: Failed to discover attributes");
        pclient->disconnect();
    }
}

void EcoflowESP32::onDisconnect(NimBLEClient* pclient, int reason) {
  Serial.print("onDisconnect: Disconnected, reason: ");
  Serial.println(reason);
}

bool EcoflowESP32::connectToDevice(NimBLEAdvertisedDevice* device) {
    setAdvertisedDevice(device);
    return connectToServer();
}

bool EcoflowESP32::connectToServer() {
    Serial.println("connectToServer: Attempting to connect");
    if (pClient == nullptr) {
        pClient = NimBLEDevice::createClient();
        pClient->setClientCallbacks(this);
        pClient->setConnectTimeout(10);
        pClient->setConnectionParams(12, 12, 0, 51);
    }

    if (m_pDeviceAddress == nullptr) {
        Serial.println("connectToServer: No device address set");
        return false;
    }

    if (!pClient->connect(*m_pDeviceAddress, false)) {
        Serial.println("connectToServer: Failed to connect");
        return false;
    }
    
    Serial.println("connectToServer: Connection process started");
    return true;
}

bool EcoflowESP32::sendCommand(const uint8_t* command, size_t size) {
    if (!pClient->isConnected() || !pWriteChr) {
        return false;
    }

    return pWriteChr->writeValue(command, size, false);
}

void EcoflowESP32::notifyCallback(NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
    Serial.println("notifyCallback: Notification received");
    if (_instance) {
        _instance->parse(pData, length);
    }
}

void EcoflowESP32::parse(uint8_t* pData, size_t length) {
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