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

        if (manufacturerData.length() >= 2) {
            uint16_t manufacturerId = (static_cast<uint8_t>(manufacturerData[1]) << 8) | static_cast<uint8_t>(manufacturerData[0]);
             if (manufacturerId == 0xB5B5) {
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

EcoflowESP32::EcoflowESP32() : pClient(nullptr)
{
    _instance = this;
    _scanCallbacks = new EcoflowScanCallbacks(this);
}

EcoflowESP32::~EcoflowESP32()
{
    delete _scanCallbacks;
    if (_pServerAddress) {
        delete _pServerAddress;
    }
}

void EcoflowESP32::setAdvertisedDevice(NimBLEAdvertisedDevice* device) {
    if (_pServerAddress) {
        delete _pServerAddress;
    }
    _pServerAddress = new NimBLEAddress(device->getAddress());
    device->getScan()->stop();
}

bool EcoflowESP32::begin()
{
    Serial.println(">>>>>>>>>>>>> EcoflowESP32 Library Version: 2.5 <<<<<<<<<<<<<");
    NimBLEDevice::init("");
    return true;
}

bool EcoflowESP32::scan(uint32_t scanTime) {
    NimBLEScan* pScan = NimBLEDevice::getScan();
    pScan->setScanCallbacks(_scanCallbacks);
    pScan->setInterval(100);
    pScan->setWindow(99);
    pScan->setActiveScan(true);
    pScan->start(scanTime, false);
    return _pServerAddress != nullptr;
}

bool EcoflowESP32::connectToServer() {
    if (pClient == nullptr) {
        pClient = NimBLEDevice::createClient();
    }

    Serial.println("Attempting to connect...");
    if (!_pServerAddress) {
        Serial.println("No device address set");
        return false;
    }

    if (!pClient->connect(*_pServerAddress)) {
        Serial.println("Failed to connect");
        return false;
    }
    Serial.println("Connected successfully");

    // Let's try to find the characteristics by iterating through services
    auto services = pClient->getServices();
    if(services.empty()) {
        Serial.println("Failed to get services");
        pClient->disconnect();
        return false;
    }

    for(auto service : services) {
        Serial.printf("Found service: %s\n", service->getUUID().toString().c_str());
        if (service->getUUID().equals(serviceUUID1) || service->getUUID().equals(serviceUUID2)) {
             pWriteChr = service->getCharacteristic(writeCharUUID1);
             if(pWriteChr)
             {
                pReadChr = service->getCharacteristic(readCharUUID1);
                if(pReadChr)
                    break;
             }
             pWriteChr = service->getCharacteristic(writeCharUUID2);
             if(pWriteChr)
             {
                pReadChr = service->getCharacteristic(readCharUUID2);
                if(pReadChr)
                    break;
             }
        }
    }


    if (!pWriteChr || !pReadChr) {
        Serial.println("Failed to get characteristics");
        pClient->disconnect();
        return false;
    }
    Serial.println("Characteristics found");
    Serial.printf("Write characteristic: %s\n", pWriteChr->getUUID().toString().c_str());
    Serial.printf("Read characteristic: %s\n", pReadChr->getUUID().toString().c_str());


    if(pReadChr->canNotify()) {
        Serial.println("Subscribing to notifications...");
        if(!pReadChr->subscribe(true, notifyCallback)) {
            Serial.println("Failed to subscribe to notifications");
            pClient->disconnect();
            return false;
        }
    }

    Serial.println("Sending request for data...");
    sendCommand(CMD_REQUEST_DATA, sizeof(CMD_REQUEST_DATA));

    delay(500); // Wait for notifications
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

    // The parsing logic from the python code needs to be ported here.
    // For now, let's just print the raw data.
    // _data.batteryLevel = get_int(pData, 35);
    // _data.outputPower = get_int(pData, 18);
    // _data.inputPower = get_int(pData, 22);
    // _data.acOn = get_int(pData, 62) != 0;
    // _data.dcOn = get_int(pData, 83) != 0;
    // _data.usbOn = (get_int(pData, 79) & 0x1) != 0;
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
