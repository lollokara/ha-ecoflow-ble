#ifndef STM32_SERIAL_H
#define STM32_SERIAL_H

/**
 * @file Stm32Serial.h
 * @author Lollokara
 * @brief Header for the Stm32Serial class.
 *
 * Defines the singleton class responsible for managing the UART connection
 * to the STM32F4 user interface controller, including OTA updates.
 */

#include <Arduino.h>
#include <FS.h>
#include "ecoflow_protocol.h"

/**
 * @class Stm32Serial
 * @brief Singleton class for ESP32-STM32 UART communication.
 *
 * This class handles:
 * - Initialization of the hardware serial port.
 * - Processing incoming packets (parsing, CRC validation).
 * - Sending outgoing packets (Handshakes, Status Updates).
 * - Performing UART-based OTA updates for the STM32.
 */
class Stm32Serial {
public:
    /**
     * @brief Gets the singleton instance.
     * @return Reference to the Stm32Serial instance.
     */
    static Stm32Serial& getInstance() {
        static Stm32Serial instance;
        return instance;
    }

    /**
     * @brief Initializes the serial interface.
     */
    void begin();

    /**
     * @brief Updates the serial handler.
     * Must be called frequently in the main loop to process incoming data and OTA state.
     */
    void update();

    /**
     * @brief Sends the current list of devices to the STM32.
     */
    void sendDeviceList();

    /**
     * @brief Sends the status of a specific device to the STM32.
     * @param device_id The ID of the device to report.
     */
    void sendDeviceStatus(uint8_t device_id);

    /**
     * @brief Starts an OTA update for the STM32 using the provided file.
     * @param path The path to the firmware file on the filesystem.
     * @return true if started successfully, false otherwise.
     */
    bool startOta(const String& path);

private:
    /**
     * @brief Private constructor for Singleton pattern.
     */
    Stm32Serial() {}

    /**
     * @brief Processes a fully received and validated packet.
     * @param buf Pointer to the packet buffer.
     * @param len Length of the packet.
     */
    void processPacket(uint8_t* buf, uint8_t len);

    /**
     * @brief Manages the OTA state machine.
     */
    void otaTask();

    // OTA State definitions
    enum OtaState {
        OTA_IDLE,
        OTA_STARTING,
        OTA_SENDING,
        OTA_WAITING_ACK,
        OTA_ENDING,
        OTA_DONE,
        OTA_ERROR
    };

    OtaState _otaState = OTA_IDLE;
    OtaState _otaPrevState = OTA_IDLE; // To know where to return after ACK
    File _otaFile;
    uint32_t _otaOffset = 0;
    uint32_t _otaTotalSize = 0;
    uint32_t _otaLastMsgTime = 0;
    uint8_t _otaRetries = 0;
    bool _ackReceived = false;
    bool _nackReceived = false;

    /**
     * @brief Helper to calculate brightness (Not currently implemented/used).
     * @return uint8_t
     */
    uint8_t calculateBrightness();
};

#endif
