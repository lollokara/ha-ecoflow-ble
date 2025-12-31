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
     * @brief Helper to calculate brightness (Not currently implemented/used).
     * @return uint8_t
     */
    uint8_t calculateBrightness();
};

#endif
