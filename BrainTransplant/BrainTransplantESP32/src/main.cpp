/**
 * @file main.cpp
 * @author Lollokara
 * @brief Main entry point for the BrainTransplantESP32 application.
 *
 * This file handles the initialization of the ESP32, including:
 * - Setting up serial communication (Debug and UART to STM32).
 * - Starting the WebServer for OTA updates.
 * - Initializing the Stm32Serial for OTA protocol.
 */

#include <Arduino.h>
#include <esp_task_wdt.h>
#include "Credentials.h"
#include "WebServer.h"
#include "Stm32Serial.h"
#include "DeviceManager.h"

// Hardware Pin Definitions
#define POWER_LATCH_PIN 16 ///< GPIO pin to control the power latch.

// Logging Tag
static const char* TAG = "Main";

/**
 * @brief Arduino Setup function.
 */
void setup() {
    // Initialize Task Watchdog Timer (TWDT) for 10 seconds, panic on timeout
    esp_task_wdt_init(10, true);
    esp_task_wdt_add(NULL); // Add current task (loop task)

    // Initialize Power Latch
    pinMode(POWER_LATCH_PIN, OUTPUT);
    digitalWrite(POWER_LATCH_PIN, LOW);

    Serial.begin(115200);
    Serial.println("Starting BrainTransplant Controller...");

    // Initialize the Device Manager (for BLE scanning shown in UI)
    DeviceManager::getInstance().initialize();

    // Start the Web Server
    WebServer::begin();

    // Initialize the UART communication with the STM32F4
    Stm32Serial::getInstance().begin();
}

/**
 * @brief Arduino Main Loop.
 */
void loop() {
    esp_task_wdt_reset();

    // Process BLE events
    DeviceManager::getInstance().update();

    // Process incoming UART packets from STM32F4 (OTA ACKs etc)
    Stm32Serial::getInstance().update();

    vTaskDelay(10 / portTICK_PERIOD_MS);
}
