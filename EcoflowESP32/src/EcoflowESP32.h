#ifndef EcoflowESP32_h
#define EcoflowESP32_h

#include "Arduino.h"
#include "NimBLEDevice.h"
#include "EcoflowData.h"
#include "NimBLEScan.h"

class AdvertisedDeviceCallbacks;

/**
 * @brief Main class for interacting with Ecoflow devices.
 */
class EcoflowESP32 : public NimBLEClientCallbacks
{
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
     * @return A pointer to the advertised device if found, nullptr otherwise.
     */
    NimBLEAdvertisedDevice* scan(uint32_t scanTime = 5);

    /**
     * @brief Connect to an Ecoflow device.
     * @param device A pointer to the advertised device to connect to.
     * @return true if the connection was successful, false otherwise.
     */
    bool connectToDevice(NimBLEAdvertisedDevice* device);

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
     * @return The battery level in percent.
     */
    int getBatteryLevel();

    /**
     * @brief Get the current input power.
     * @return The input power in watts.
     */
    int getInputPower();

    /**
     * @brief Get the current output power.
     * @return The output power in watts.
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

  private:
    void onConnect(NimBLEClient* pclient);
    void onDisconnect(NimBLEClient* pclient);
    static void notifyCallback(NimBLERemoteCharacteristic* pBLERemoteCharacteristic, uint8_t* pData, size_t length, bool isNotify);
    void parse(uint8_t* pData, size_t length);
    bool sendCommand(const uint8_t* command, size_t size);

    NimBLEClient* pClient;
    NimBLERemoteCharacteristic* pWriteChr;
    NimBLERemoteCharacteristic* pReadChr;
    EcoflowData _data;
    static EcoflowESP32* _instance;
    AdvertisedDeviceCallbacks* _advertisedDeviceCallbacks;
};

#endif
