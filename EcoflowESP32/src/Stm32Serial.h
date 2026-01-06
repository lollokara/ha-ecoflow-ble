#ifndef STM32_SERIAL_H
#define STM32_SERIAL_H

/**
 * @file Stm32Serial.h
 * @author Lollokara
 * @brief Header for the Stm32Serial class.
 *
 * Defines the singleton class responsible for managing the UART connection
 * to the STM32F4 user interface controller.
 */

#include <Arduino.h>
#include "ecoflow_protocol.h"
#include <freertos/semphr.h>
#include <vector>

/**
 * @class Stm32Serial
 * @brief Singleton class for ESP32-STM32 UART communication.
 *
 * This class handles:
 * - Initialization of the hardware serial port.
 * - Processing incoming packets (parsing, CRC validation).
 * - Sending outgoing packets (Handshakes, Status Updates).
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
     * Must be called frequently in the main loop to process incoming data.
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
     * @brief Starts the background OTA task.
     * @param filename Path to the firmware file in LittleFS.
     */
    void startOta(const String& filename);

    bool isOtaInProgress() const { return _otaRunning; }

    // Helper to send raw data safely
    void sendData(const uint8_t* data, size_t len);

    // New API for Logging
    bool requestLogList(String& output, uint32_t timeoutMs = 2000);
    void requestFile(const String& filename);
    bool hasFileData();
    size_t getFileData(uint8_t* buffer, size_t maxLen);
    bool isFileTransferComplete();

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

    static void otaTask(void* parameter);

    bool _otaRunning = false;
    SemaphoreHandle_t _txMutex = NULL;

    // Log List Handling
    SemaphoreHandle_t _logListReady = NULL;
    String _logListBuffer;

    // File Transfer Handling
    SemaphoreHandle_t _fileDataMutex = NULL;
    std::vector<uint8_t> _fileDataQueue;
    volatile bool _fileTransferComplete = false;
};

#endif
