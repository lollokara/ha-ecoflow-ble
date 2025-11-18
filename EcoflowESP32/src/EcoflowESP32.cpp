#include "EcoflowESP32.h"
#include "EcoflowProtocol.h"

static NimBLEUUID serviceUUID("00000001-0000-1000-8000-00805f9b34fb");
static NimBLEUUID writeCharUUID("00000002-0000-1000-8000-00805f9b34fb");
static NimBLEUUID readCharUUID("00000003-0000-1000-8000-00805f9b34fb");

EcoflowScanCallbacks::EcoflowScanCallbacks(EcoflowESP32* pEcoflowESP32) : _pEcoflowESP32(pEcoflowESP32) {}

void EcoflowScanCallbacks::onResult(NimBLEAdvertisedDevice* advertisedDevice) {
    Serial.print("Device found: ");
    Serial.print(advertisedDevice->getAddress().toString().c_str());
    
    if (advertisedDevice->haveName()) {
        Serial.print(", Name: ");
        Serial.print(advertisedDevice->getName().c_str());
    }

    if (advertisedDevice->haveServiceUUID()) {
        Serial.print(", Service UUID: ");
        Serial.print(advertisedDevice->getServiceUUID().toString().c_str());
    }
    
    Serial.println();

    if (advertisedDevice->haveManufacturerData()) {
        std::string manufacturerData = advertisedDevice->getManufacturerData();
        Serial.print("  Raw Manufacturer Data: ");
        for (int i = 0; i < manufacturerData.length(); i++) {
            Serial.printf("%02X ", (uint8_t)manufacturerData[i]);
        }
        Serial.println();

        if (manufacturerData.length() >= 2) {
            uint16_t manufacturerId = (static_cast<uint8_t>(manufacturerData[1]) << 8) | static_cast<uint8_t>(manufacturerData[0]);
            Serial.print("  Parsed Manufacturer ID: ");
            Serial.println(manufacturerId);

            if (manufacturerId == 46517) { // 0xB5B5
                Serial.println("  Found Ecoflow device by manufacturer data");
                advertisedDevice->getScan()->stop();
                _pEcoflowESP32->setAdvertisedDevice(advertisedDevice);
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
    if (_pAdvertisedDevice) {
        delete _pAdvertisedDevice;
    }
}

void EcoflowESP32::setAdvertisedDevice(NimBLEAdvertisedDevice* device) {
    if (_pAdvertisedDevice) {
        delete _pAdvertisedDevice;
    }
    _pAdvertisedDevice = new NimBLEAdvertisedDevice(*device);
}

bool EcoflowESP32::begin()
{
    NimBLEDevice::init("");
    return true;
}

NimBLEAdvertisedDevice* EcoflowESP32::scan(uint32_t scanTime) {
    if (_pAdvertisedDevice) {
        delete _pAdvertisedDevice;
        _pAdvertisedDevice = nullptr;
    }
    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setScanCallbacks(_scanCallbacks);
    pScan->setInterval(100);
    pScan->setWindow(99);
    pScan->setActiveScan(true);
    pScan->start(scanTime, false); // blocking call
    return _pAdvertisedDevice;
}


void EcoflowESP32::onConnect(NimBLEClient* pclient) {
  Serial.println("Connected");
}

void EcoflowESP32::onDisconnect(NimBLEClient* pclient) {
  Serial.println("Disconnected");
}

bool EcoflowESP32::connectToDevice(NimBLEAdvertisedDevice* device) {
    if (pClient == nullptr) {
        pClient = NimBLEDevice::createClient();
        pClient->setClientCallbacks(this);
    }

    if (!pClient->connect(device)) {
        Serial.println("Failed to connect");
        return false;
    }

    NimBLERemoteService* pSvc = nullptr;

    pSvc = pClient->getService(serviceUUID);
    if(pSvc) {
        pWriteChr = pSvc->getCharacteristic(writeCharUUID);
        pReadChr = pSvc->getCharacteristic(readCharUUID);
    }

    if (!pWriteChr || !pReadChr) {
        pClient->disconnect();
        return false;
    }

    if(pReadChr->canNotify()) {
        pReadChr->subscribe(true, notifyCallback);
    }

    return true;
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

static int get_int(const uint8_t* data, int offset) {
    return (data[offset] << 24) | (data[offset + 1] << 16) | (data[offset + 2] << 8) | data[offset + 3];
}

void EcoflowESP32::parse(uint8_t* pData, size_t length) {
    if (length < 123) {
        return;
    }

    _data.batteryLevel = get_int(pData, 35);
    _data.outputPower = get_int(pData, 18);
    _data.inputPower = get_int(pData, 22);
    _data.acOn = get_int(pData, 62) != 0;
    _data.dcOn = get_int(pData, 83) != 0;
    _data.usbOn = (get_int(pData, 79) & 0x1) != 0;
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
