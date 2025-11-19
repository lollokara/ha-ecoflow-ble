#ifndef EcoflowESP32_h
#define EcoflowESP32_h

#include "Arduino.h"
#include "NimBLEDevice.h"
#include "EcoflowData.h"

class EcoflowESP32; // Forward declaration

class EcoflowScanCallbacks : public NimBLEScanCallbacks {
public:
    EcoflowScanCallbacks(EcoflowESP32* pEcoflowESP32);
    void onDiscovered(const NimBLEAdvertisedDevice* advertisedDevice) override;
private:
    EcoflowESP32* _pEcoflowESP32;
};

/**
 * @file EcoflowESP32.h
 * @brief This file contains the declaration of the EcoflowESP32 class, which provides an interface
 * to communicate with Ecoflow devices over BLE.
 */
class EcoflowESP32 : public NimBLEClientCallbacks
{
    friend class EcoflowScanCallbacks;
public:
    /**
     * @brief Construct a new EcoflowESP32 object.
     */
    EcoflowESP32();

    /**
     * @brief Destroy the EcoflowESP32 object.
     */
    ~EcoflowESP32();

    /**
     * @brief Initialize the BLE stack.
     * @return true if initialization was successful, false otherwise.
     */
    bool begin();

    /**
     * @brief Scan for Ecoflow devices.
     * @param scanTime The duration of the scan in seconds.
     * @return true if a device was found, false otherwise.
     */
    bool scan(uint32_t scanTime = 5);

    /**
     * @brief Connect to the first found Ecoflow device.
     * @return true if the connection was successful, false otherwise.
     */
    bool connectToServer();

    /**
     * @brief Turn the AC output on or off.
     * @param on true to turn on, false to turn off.
     */
    void setAC(bool on);

    /**
     * @brief Turn the DC (12V) output on or off.
     * @param on true to turn on, false to turn off.
     */
    void setDC(bool on);

    /**
     * @brief Turn the USB outputs on or off.
     * @param on true to turn on, false to turn off.
     */
    void setUSB(bool on);

    /**
     * @brief Get the current battery level.
     * @return The battery level in percent, or -1 if not available.
     */
    int getBatteryLevel();

    /**
     * @brief Get the current input power.
     * @return The input power in watts, or -1 if not available.
     */
    int getInputPower();

    /**
     * @brief Get the current output power.
     * @return The output power in watts, or -1 if not available.
     */
    int getOutputPower();

    /**
     * @brief Check if the AC output is on.
     * @return true if the AC output is on, false otherwise.
     */
    bool isAcOn();

    /**
     * @brief Check if the DC (12V) output is on.
     * @return true if the DC output is on, false otherwise.
     */
    bool isDcOn();

    /**
     * @brief Check if the USB outputs are on.
     * @return true if the USB outputs are on, false otherwise.
     */
    bool isUsbOn();

    /**
     * @brief Check if the client is connected to a device.
     * @return true if connected, false otherwise.
     */
    bool isConnected();

    /**
     * @brief Manually set the advertised device to connect to.
     * @param device A pointer to the advertised device.
     */
    void setAdvertisedDevice(NimBLEAdvertisedDevice* device);

    /**
     * @brief Request the latest data from the device.
     */
    void requestData();

    /**
     * @brief Send a raw command to the device.
     * @param command A pointer to the command byte array.
     * @param size The size of the command array.
     * @return true if the command was sent successfully, false otherwise.
     */
    bool sendCommand(const uint8_t* command, size_t size);

private:
    void onConnect(NimBLEClient* pclient);
    void onDisconnect(NimBLEClient* pclient, int reason);
    void onAuthenticationComplete(ble_gap_conn_desc* desc);
    static void notifyCallback(NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
    void parse(uint8_t* pData, size_t length);

    NimBLEClient* pClient;
    NimBLERemoteCharacteristic* pWriteChr;
    NimBLERemoteCharacteristic* pReadChr;
    EcoflowData _data;
    static EcoflowESP32* _instance;

    EcoflowScanCallbacks* _scanCallbacks;
    NimBLEAdvertisedDevice* m_pAdvertisedDevice = nullptr;
};

#endif