#include "DeviceManager.h"
#include "Credentials.h"
#include <NimBLEDevice.h>

// Helper to extract serial from Manufacturer Data
// Similar to Python's parse_manufacturer_data
// Python: serial = hex_representation[2:34] -> 16 bytes, skipping the first byte (0) of the data payload.
// The data payload typically comes after the 2-byte Manufacturer ID.
// NimBLE's getManufacturerData() returns the raw payload associated with the ID if parsed, or the whole string?
// NimBLEAdvertisedDevice::getManufacturerData() returns std::string containing the payload ONLY (ID is key).
// Wait, looking at NimBLE source, it seems it just returns the payload bytes.
// If Python receives `manufacturer_data[ID]`, then index 0 is the first byte of payload.
// Python logic: hex_representation is hex string of the payload.
// hex[2:34] means skipping first 2 chars = 1 byte.
// So we skip index 0 of payload, and take next 16 bytes (index 1 to 16).
// This matches the previous logic: `manufacturerData.data() + 1`.
// BUT, we must be absolutely certain what getManufacturerData returns.
// If using standard NimBLE-Arduino: it returns payload.
// Let's double check logging.
static std::string extractSerial(const std::string& manufacturerData) {
    // Debug print
    if (manufacturerData.length() > 0) {
        char hex_debug[manufacturerData.length() * 2 + 1];
        for(size_t i=0; i<manufacturerData.length(); i++) {
            sprintf(&hex_debug[i*2], "%02X", (uint8_t)manufacturerData[i]);
        }
        hex_debug[manufacturerData.length() * 2] = 0;
        ESP_LOGD("DeviceManager", "Manufacturer Data: %s", hex_debug);
    }

    // Manufacturer Data structure for Ecoflow:
    // Bytes 0-1: Company ID (0xB5B5 or similar)
    // Byte 2:    Length/Type (e.g., 0x13 = 19)
    // Bytes 3+:  Serial Number (16 bytes)
    if (manufacturerData.length() < 19) return "";

    // Bytes 3 to 18 (length 16)
    char serialBuf[17];
    memcpy(serialBuf, manufacturerData.data() + 3, 16);
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
    NimBLEDevice::init(""); // Initialize BLE once here
    prefs.begin("ecoflow", false);
    loadDevices();

    // If we have saved credentials, start the instances
    if (!slotD3.macAddress.empty()) {
        ESP_LOGI("DeviceManager", "Restoring D3: %s", slotD3.serialNumber.c_str());
        slotD3.instance->begin(ECOFLOW_USER_ID, slotD3.serialNumber, slotD3.macAddress, 3); // V3 for D3 (Delta)
    }
    if (!slotW2.macAddress.empty()) {
        ESP_LOGI("DeviceManager", "Restoring W2: %s", slotW2.serialNumber.c_str());
        slotW2.instance->begin(ECOFLOW_USER_ID, slotW2.serialNumber, slotW2.macAddress, 2); // V2 for W2 (Wave 2)
    }
}

void DeviceManager::update() {
    // Handle Pending Connection from Scan
    if (_hasPendingConnection) {
        stopScan(); // Ensure scan is stopped before connecting

        ESP_LOGI("DeviceManager", "Executing pending connection to %s", _pendingConnectSN.c_str());
        saveDevice(_targetScanType, _pendingConnectMac, _pendingConnectSN);

        EcoflowESP32* dev = getDevice(_targetScanType);
        uint8_t version = (_targetScanType == DeviceType::WAVE_2) ? 2 : 3;
        dev->begin(ECOFLOW_USER_ID, _pendingConnectSN, _pendingConnectMac, version);

        // Since we have the advertised device pointer in the callback,
        // we could optimize by passing it if we stored it, but begin() sets up basics.
        // Wait, EcoflowESP32::begin() doesn't connect anymore, it just sets creds.
        // We need to tell it to connect!
        // But we don't have the AdvertisedDevice object here easily unless we stored a copy.
        // Re-scanning to connect is inefficient if we just found it.
        // However, EcoflowESP32::begin used to start scanning for a specific address.
        // Now we want DeviceManager to find it and hand it over.

        // Issue: NimBLEAdvertisedDevice pointers from callbacks are temporary/managed by Scan.
        // We need to initiate connection via address if we don't have the object.
        // But NimBLEClient::connect() takes AdvertisedDevice* OR address.
        // EcoflowESP32 currently expects an AdvertisedDevice in connectTo().
        // We should probably let DeviceManager restart a targetted scan OR
        // since we just found it, we should have grabbed it.

        // Since we are in a loop, let's simplify:
        // We saved the MAC.
        // If any device is disconnected but has a MAC saved, we should be scanning for it.

        _hasPendingConnection = false;
    }

    slotD3.instance->update();
    slotW2.instance->update();

    slotD3.isConnected = slotD3.instance->isConnected();
    slotW2.isConnected = slotW2.instance->isConnected();

    // Auto-reconnection / Scanning Logic
    if (!_isScanning) {
        // If we have a disconnected device that is configured (has MAC), we should scan for it.
        // Priority: Target Type if user requested, otherwise cycle or scan for all?
        // NimBLE Scan finds everything. We just need to filter in onResult.

        bool d3NeedsConnect = !slotD3.isConnected && !slotD3.macAddress.empty();
        bool w2NeedsConnect = !slotW2.isConnected && !slotW2.macAddress.empty();

        if (d3NeedsConnect || w2NeedsConnect) {
             // Start generic background scan
             // We don't set _targetScanType here strictly, we can check both in onResult
             _targetScanType = (d3NeedsConnect) ? DeviceType::DELTA_2 : DeviceType::WAVE_2; // Just for logging context
             startScan(_targetScanType);
        }
    }

    if (_isScanning) {
        if (millis() - _scanStartTime > 10000) { // 10s scan timeout
            stopScan();
            ESP_LOGI("DeviceManager", "Scan timeout/cycle");
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
    _hasPendingConnection = false;

    if (!pScan) {
        pScan = NimBLEDevice::getScan();
        pScan->setAdvertisedDeviceCallbacks(new ManagerScanCallbacks(this), true);
        pScan->setActiveScan(true);
        pScan->setInterval(100);
        pScan->setWindow(99);
    }
    // Non-blocking scan (duration 0 = forever until stopped manually or timeout logic in update)
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
    if (_hasPendingConnection) return; // Already found one
    if (!device->haveManufacturerData()) return;

    std::string data = device->getManufacturerData();
    std::string sn = extractSerial(data);

    if (sn.length() > 0) {
        ESP_LOGD("DeviceManager", "Found SN: %s", sn.c_str());

        // Check matches for D3 slot
        if (!slotD3.isConnected && !d3.isConnecting()) {
            bool isD3 = isTargetDevice(sn, DeviceType::DELTA_2);
            // If we have a saved MAC, only connect if it matches. If no MAC (new pairing), connect if type matches.
            bool match = false;
            if (!slotD3.macAddress.empty()) {
                if (device->getAddress().toString() == slotD3.macAddress) match = true;
            } else if (isD3 && _targetScanType == DeviceType::DELTA_2) {
                match = true; // New pairing discovery
            }

            if (match) {
                ESP_LOGI("DeviceManager", "Connecting D3 (%s)", sn.c_str());
                d3.connectTo(device); // Pass the device object directly!
                // Save if it was a new pairing
                if (slotD3.macAddress.empty()) {
                     saveDevice(DeviceType::DELTA_2, device->getAddress().toString(), sn);
                     // Ensure begin is called with correct SN for auth if this was a fresh discovery
                     d3.begin(ECOFLOW_USER_ID, sn, device->getAddress().toString(), 3);
                }
                return; // One connection per scan cycle to be safe? NimBLE is single threaded mostly.
            }
        }

        // Check matches for W2 slot
        if (!slotW2.isConnected && !w2.isConnecting()) {
            bool isW2 = isTargetDevice(sn, DeviceType::WAVE_2);
            bool match = false;
            if (!slotW2.macAddress.empty()) {
                if (device->getAddress().toString() == slotW2.macAddress) match = true;
            } else if (isW2 && _targetScanType == DeviceType::WAVE_2) {
                match = true; // New pairing discovery
            }

            if (match) {
                ESP_LOGI("DeviceManager", "Connecting W2 (%s)", sn.c_str());
                w2.connectTo(device);
                 if (slotW2.macAddress.empty()) {
                     saveDevice(DeviceType::WAVE_2, device->getAddress().toString(), sn);
                     w2.begin(ECOFLOW_USER_ID, sn, device->getAddress().toString(), 2);
                }
                return;
            }
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
