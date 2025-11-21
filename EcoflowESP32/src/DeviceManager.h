#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include "EcoflowESP32.h"
#include <vector>
#include <Preferences.h>

enum class DeviceType {
    DELTA_2, // Matches P2 serial prefix (User said Delta 3 but serial starts with P2, code for Delta 2 starts with R33? User said: "for delta 3 use D3 and for Wave 2 use W2", "my wave 2 serial is KT...", "P231ZE...").
             // Wait, memory says R33 is Delta 2. P2 might be Delta Pro or Delta 2 Max or Delta 3? User said "D3".
             // User provided serial P231ZEBAPH342310.
    WAVE_2
};

struct DeviceSlot {
    EcoflowESP32* instance;
    std::string macAddress;
    std::string serialNumber;
    std::string name; // "D3" or "W2"
    DeviceType type;
    bool isConnected;
};

class DeviceManager {
public:
    static DeviceManager& getInstance();

    void initialize();
    void update();

    // Connect to a device type in the specified slot
    void scanAndConnect(DeviceType type);

    // Disconnect and forget
    void disconnect(DeviceType type);

    EcoflowESP32* getDevice(DeviceType type);
    DeviceSlot* getSlot(DeviceType type);

    // Scanning state
    bool isScanning();
    bool isAnyConnecting();

private:
    DeviceManager();

    EcoflowESP32 d3;
    EcoflowESP32 w2;

    DeviceSlot slotD3;
    DeviceSlot slotW2;

    Preferences prefs;

    NimBLEScan* pScan = nullptr;
    bool _isScanning = false;
    uint32_t _scanStartTime = 0;
    DeviceType _targetScanType;

    class ManagerScanCallbacks : public NimBLEAdvertisedDeviceCallbacks {
        DeviceManager* _parent;
    public:
        ManagerScanCallbacks(DeviceManager* parent) : _parent(parent) {}
        void onResult(NimBLEAdvertisedDevice* advertisedDevice) override;
    };

    void startScan(DeviceType type);
    void stopScan();
    void onDeviceFound(NimBLEAdvertisedDevice* device);
    bool isTargetDevice(const std::string& sn, DeviceType type);
    void saveDevice(DeviceType type, const std::string& mac, const std::string& sn);
    void loadDevices();

    // Connection queue
    bool _hasPendingConnection = false;
    std::string _pendingConnectMac;
    std::string _pendingConnectSN;
    NimBLEAdvertisedDevice* _pendingDevice = nullptr;

    SemaphoreHandle_t _scanMutex;
};

#endif
