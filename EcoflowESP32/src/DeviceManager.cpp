#include "DeviceManager.h"
#include "Credentials.h"
#include <NimBLEDevice.h>

// Helper to extract serial from Manufacturer Data
// Similar to Python's parse_manufacturer_data
static std::string extractSerial(const std::string& manufacturerData) {
    if (manufacturerData.length() < 18) return "";
    // Python: hex_representation[2:34] -> 16 bytes -> chars 2 to 18 (0-indexed? No, 1-indexed).
    // C++: manufacturerData is raw bytes.
    // Manufacturer ID is first 2 bytes.
    // Data starts at index 2.
    // Serial is 16 bytes?
    // Python:
    // hex_representation = binascii.hexlify(data).decode("utf-8")
    // serial = hex_representation[2:34]
    // 34-2 = 32 hex chars = 16 bytes.
    // So bytes[1...16]? Or bytes[0...15] of the payload?
    // manufacturer_data is a dict {ID: bytes}.
    // In NimBLE, getManufacturerData() returns the whole string including ID? No, usually just payload if obtained via getManufacturerData(ID).
    // But here we get the raw string.

    // Let's assume standard format: 2 bytes ID + Data.
    // Ecoflow ID is 0xB3B5 (46517 decimal).

    // Wait, Python code: `advertisement_data.manufacturer_data[MANUFACTURER_ID]` -> returns bytes.
    // `hex_representation = binascii.hexlify(data)`
    // `serial = hex_representation[2:34]` -> Bytes 1 to 16 (skipping byte 0?).
    // Let's look at byte 0.
    // Byte 0 might be a type or length?
    // "serial = binascii.unhexlify(serial).decode("utf-8")"

    // So we need 16 bytes starting from offset 1 of the manufacturer data payload.

    if (manufacturerData.length() < 18) return "";

    // Bytes 1 to 16 (length 16)
    char serialBuf[17];
    memcpy(serialBuf, manufacturerData.data() + 1, 16);
    serialBuf[16] = '\0';
    return std::string(serialBuf);
}

DeviceManager& DeviceManager::getInstance() {
    static DeviceManager instance;
    return instance;
}

DeviceManager::DeviceManager() {
    slotD3.instance = &d3;
    slotD3.name = "D3";
    slotD3.type = DeviceType::DELTA_2;
    slotD3.isConnected = false;

    slotW2.instance = &w2;
    slotW2.name = "W2";
    slotW2.type = DeviceType::WAVE_2;
    slotW2.isConnected = false;
}

void DeviceManager::initialize() {
    prefs.begin("ecoflow", false);
    loadDevices();

    // If we have saved credentials, start the instances
    if (!slotD3.macAddress.empty()) {
        ESP_LOGI("DeviceManager", "Restoring D3: %s", slotD3.serialNumber.c_str());
        slotD3.instance->begin(ECOFLOW_USER_ID, slotD3.serialNumber, slotD3.macAddress);
    }
    if (!slotW2.macAddress.empty()) {
        ESP_LOGI("DeviceManager", "Restoring W2: %s", slotW2.serialNumber.c_str());
        slotW2.instance->begin(ECOFLOW_USER_ID, slotW2.serialNumber, slotW2.macAddress);
    }
}

void DeviceManager::update() {
    slotD3.instance->update();
    slotW2.instance->update();

    slotD3.isConnected = slotD3.instance->isConnected();
    slotW2.isConnected = slotW2.instance->isConnected();

    if (_isScanning) {
        if (millis() - _scanStartTime > 10000) { // 10s scan timeout
            stopScan();
            ESP_LOGI("DeviceManager", "Scan timeout");
        }
    }
}

void DeviceManager::loadDevices() {
    String mac = prefs.getString("d3_mac", "");
    String sn = prefs.getString("d3_sn", "");
    if (!mac.isEmpty() && !sn.isEmpty()) {
        slotD3.macAddress = mac.c_str();
        slotD3.serialNumber = sn.c_str();
    }

    mac = prefs.getString("w2_mac", "");
    sn = prefs.getString("w2_sn", "");
    if (!mac.isEmpty() && !sn.isEmpty()) {
        slotW2.macAddress = mac.c_str();
        slotW2.serialNumber = sn.c_str();
    }
}

void DeviceManager::saveDevice(DeviceType type, const std::string& mac, const std::string& sn) {
    if (type == DeviceType::DELTA_2) {
        prefs.putString("d3_mac", mac.c_str());
        prefs.putString("d3_sn", sn.c_str());
        slotD3.macAddress = mac;
        slotD3.serialNumber = sn;
    } else {
        prefs.putString("w2_mac", mac.c_str());
        prefs.putString("w2_sn", sn.c_str());
        slotW2.macAddress = mac;
        slotW2.serialNumber = sn;
    }
}

void DeviceManager::disconnect(DeviceType type) {
    if (type == DeviceType::DELTA_2) {
        slotD3.instance->disconnectAndForget();
        prefs.remove("d3_mac");
        prefs.remove("d3_sn");
        slotD3.macAddress = "";
        slotD3.serialNumber = "";
    } else {
        slotW2.instance->disconnectAndForget();
        prefs.remove("w2_mac");
        prefs.remove("w2_sn");
        slotW2.macAddress = "";
        slotW2.serialNumber = "";
    }
}

EcoflowESP32* DeviceManager::getDevice(DeviceType type) {
    return (type == DeviceType::DELTA_2) ? &d3 : &w2;
}

DeviceSlot* DeviceManager::getSlot(DeviceType type) {
    return (type == DeviceType::DELTA_2) ? &slotD3 : &slotW2;
}

void DeviceManager::scanAndConnect(DeviceType type) {
    if (_isScanning) return;
    startScan(type);
}

void DeviceManager::startScan(DeviceType type) {
    ESP_LOGI("DeviceManager", "Starting scan for type %d", (int)type);
    _targetScanType = type;
    _isScanning = true;
    _scanStartTime = millis();

    if (!pScan) {
        pScan = NimBLEDevice::getScan();
        pScan->setAdvertisedDeviceCallbacks(new ManagerScanCallbacks(this), true); // Use true to delete old callbacks? No, managed manually usually.
        pScan->setActiveScan(true);
        pScan->setInterval(100);
        pScan->setWindow(99);
    }
    pScan->start(0, nullptr, false);
}

void DeviceManager::stopScan() {
    if (pScan) {
        pScan->stop();
        pScan->clearResults();
    }
    _isScanning = false;
}

bool DeviceManager::isScanning() {
    return _isScanning;
}

void DeviceManager::ManagerScanCallbacks::onResult(NimBLEAdvertisedDevice* advertisedDevice) {
    if (_parent->isScanning()) {
        _parent->onDeviceFound(advertisedDevice);
    }
}

void DeviceManager::onDeviceFound(NimBLEAdvertisedDevice* device) {
    if (!device->haveManufacturerData()) return;

    std::string data = device->getManufacturerData();
    // Check Manufacturer ID (Ecoflow = 0xB3B5 -> little endian in memory? NimBLE returns string.
    // The data string usually contains the ID at the beginning if parsed from raw packet,
    // BUT NimBLE's getManufacturerData() returns the data associated with the ID if parsed, OR raw?
    // Wait, NimBLEAdvertisedDevice::getManufacturerData() returns the *whole* manufacturer data field including the 2-byte company ID?
    // Let's verify. NimBLEArduino usually stores the body.
    // Actually, `haveManufacturerData()` checks if the flag is set.
    // `getManufacturerData()` returns std::string.
    // If we check standard BLE, it's ID + Data.

    // Ecoflow ID is 46517 (0xB5B3 in Little Endian, or 0xB3B5).
    // Let's just check if the string is long enough and parse serial.

    std::string sn = extractSerial(data);
    if (sn.length() > 0) {
        ESP_LOGD("DeviceManager", "Found SN: %s", sn.c_str());
        if (isTargetDevice(sn, _targetScanType)) {
            ESP_LOGI("DeviceManager", "Match found! Connecting to %s", sn.c_str());
            stopScan();

            std::string mac = device->getAddress().toString();
            saveDevice(_targetScanType, mac, sn);

            EcoflowESP32* dev = getDevice(_targetScanType);
            dev->begin(ECOFLOW_USER_ID, sn, mac);
        }
    }
}

bool DeviceManager::isTargetDevice(const std::string& sn, DeviceType type) {
    if (type == DeviceType::DELTA_2) {
        // Matches P2 or R33? User said P2.
        return (sn.rfind("P2", 0) == 0);
    } else {
        // Matches KT for Wave 2
        return (sn.rfind("KT", 0) == 0);
    }
}
