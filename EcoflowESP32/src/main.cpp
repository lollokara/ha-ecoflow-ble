/**
 * @file main.cpp
 * @author Lollokara
 * @brief Main entry point for the EcoflowESP32 application.
 *
 * This file handles the initialization of the ESP32, including:
 * - Setting up serial communication (Debug and UART to STM32).
 * - Initializing the DeviceManager for BLE connections.
 * - Starting the WebServer for OTA updates and status.
 * - Managing the main application loop, including sensor updates,
 *   device data polling, and UART communication handling.
 */

#include <Arduino.h>
#include <esp_task_wdt.h>
#include "EcoflowESP32.h"
#include "Credentials.h"
#include "DeviceManager.h"
#include "CmdUtils.h"
#include "WebServer.h"
#include "LightSensor.h"
#include "ecoflow_protocol.h"
#include "Stm32Serial.h"
#include "RemoteLogger.h"

// Hardware Pin Definitions
#define POWER_LATCH_PIN 16 ///< GPIO pin to control the power latch (keeps device on).

// Logging Tag
static const char* TAG = "Main";

/**
 * @brief Processes incoming data from the Debug Serial interface.
 *
 * Reads characters from Serial, builds a command string, and passes it
 * to CmdUtils::processInput when a newline is received.
 */
void checkSerial() {
    static String inputBuffer = "";
    while (Serial.available()) {
        char c = (char)Serial.read();
        if (c == '\n') {
            CmdUtils::processInput(inputBuffer);
            inputBuffer = "";   
        } else if (c >= 32 && c <= 126) {
            inputBuffer += c;
        }
    }
}

/**
 * @brief Arduino Setup function.
 *
 * Runs once at startup. Initializes pins, serial ports, singletons,
 * and starts background tasks (Web Server, BLE scanning).
 */
void setup() {
    // Initialize Task Watchdog Timer (TWDT) for 10 seconds, panic on timeout
    esp_task_wdt_init(10, true);
    esp_task_wdt_add(NULL); // Add current task (loop task)

    // Initialize Power Latch to keep the device powered on
    pinMode(POWER_LATCH_PIN, OUTPUT);
    digitalWrite(POWER_LATCH_PIN, LOW); // Active Low/High depends on hardware, assuming Low keeps it ON based on previous context or defaulting.
                                       // Wait, memory says "switch to OUTPUT LOW only when executing the power-off sequence."
                                       // But here it sets it LOW at startup.
                                       // The memory said: "ESP32 GPIO 16 is the Power Latch control pin; it must be initialized as INPUT_PULLUP in setup() and switched to OUTPUT LOW only when executing the power-off sequence."
                                       // The existing code sets it OUTPUT LOW. I must NOT alter code functionality, so I will document it as is.

    // Serial.begin(115200); // Disabled to prevent LightSensor interference
    // Serial.println("Starting Ecoflow Controller...");

    RemoteLogger_Init();

    // Initialize Light Sensor for ambient brightness detection
    LightSensor::getInstance().begin();

    // Initialize the Device Manager to handle BLE connections
    DeviceManager::getInstance().initialize();

    // Start the Web Server
    WebServer::begin();

    // Initialize the UART communication with the STM32F4
    Stm32Serial::getInstance().begin();
}

/**
 * @brief Arduino Main Loop.
 *
 * Runs repeatedly. Handles:
 * - Light sensor updates.
 * - BLE device state updates.
 * - Serial command processing.
 * - STM32 UART packet processing.
 * - Periodic data polling (every 2s).
 * - Periodic device list broadcasting (every 5s).
 */
void loop() {
    esp_task_wdt_reset();

    static uint32_t last_data_refresh = 0;
    static uint32_t last_device_list_update = 0;

    // Process WebServer delayed tasks (e.g. log downloads)
    WebServer::update();

    // Update light sensor readings
    LightSensor::getInstance().update();

    // Process BLE events and connection states
    DeviceManager::getInstance().update();

    // Check for debug commands
    checkSerial();

    // Process incoming UART packets from STM32F4
    Stm32Serial::getInstance().update();

    // Poll connected devices for data every 2 seconds
    if (millis() - last_data_refresh > 2000) {
        last_data_refresh = millis();

        EcoflowESP32* d3 = DeviceManager::getInstance().getDevice(DeviceType::DELTA_3);
        if (d3 && d3->isAuthenticated()) d3->requestData();

        EcoflowESP32* w2 = DeviceManager::getInstance().getDevice(DeviceType::WAVE_2);
        if (w2 && w2->isAuthenticated()) w2->requestData();

        EcoflowESP32* d3p = DeviceManager::getInstance().getDevice(DeviceType::DELTA_PRO_3);
        if (d3p && d3p->isAuthenticated()) d3p->requestData();
    }

    // Periodically broadcast the device list (every 5 seconds)
    // This ensures the STM32F4 stays in sync even if packets are lost or it restarts.
    if (millis() - last_device_list_update > 5000) {
        last_device_list_update = millis();
        Stm32Serial::getInstance().sendDeviceList();
    }
}
