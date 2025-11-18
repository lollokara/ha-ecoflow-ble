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
}

void EcoflowESP32::setAdvertisedDevice(NimBLEAdvertisedDevice* device) {
    if (m_pAdvertisedDevice == nullptr) {
        m_pAdvertisedDevice = device;
        NimBLEDevice::getScan()->stop();
    }
}

bool EcoflowESP32::begin()
{
    Serial.println(">>>>>>>>>>>>> EcoflowESP32 Library Version: 3.0 <<<<<<<<<<<<<");
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
    return m_pAdvertisedDevice != nullptr;
}


void EcoflowESP32::onConnect(NimBLEClient* pclient) {
  Serial.println("Connected");
}

void EcoflowESP32::onDisconnect(NimBLEClient* pclient, int reason) {
  Serial.print("Disconnected, reason: ");
  Serial.println(reason);
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
    if (m_pAdvertisedDevice == nullptr) {
        Serial.println("No device address set");
        return false;
    }

    if (!pClient->connect(m_pAdvertisedDevice)) {
        Serial.println("Failed to connect");
        return false;
    }
    Serial.println("Connected successfully");

    auto services = pClient->getServices(true);
    if(services.empty()) {
        Serial.println("Failed to get services");
        pClient->disconnect();
        return false;
    }

    Serial.println("Services and Characteristics:");
    for(auto service : services) {
        Serial.printf("Service: %s\n", service->getUUID().toString().c_str());
        auto characteristics = service->getCharacteristics(true);
        for(auto characteristic : characteristics) {
            Serial.printf("  Characteristic: %s", characteristic->getUUID().toString().c_str());
            if(characteristic->canRead()) Serial.print(" R");
            if(characteristic->canWrite()) Serial.print(" W");
            if(characteristic->canNotify()) Serial.print(" N");
            if(characteristic->canIndicate()) Serial.print(" I");
            Serial.println();
        }
    }

    pClient->disconnect();
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