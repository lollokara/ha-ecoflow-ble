/**
 * @file main.cpp
 * @author Lollokara
 * @brief Main application file for the ESP32 EcoFlow Controller.
 *
 * This file contains the primary application logic, including:
 * - Initialization of hardware (Serial, Display, Buttons).
 * - Management of the user interface, including button handling and display updates.
 * - Coordination of the DeviceManager to handle BLE connections and data.
 */

#include <Arduino.h>
#include "EcoflowESP32.h"
#include "Credentials.h"
#include "Display.h"
#include "DeviceManager.h"
#include "CmdUtils.h"
#include "WebServer.h"
#include "LightSensor.h"
#include "ecoflow_protocol.h"

// Define GPIO pins for the buttons
#define BTN_UP_PIN    4
#define BTN_DOWN_PIN  5
#define BTN_ENTER_PIN 6
#define POWER_LATCH_PIN 12

// Timings for button press detection (in milliseconds)
#define DEBOUNCE_DELAY 50
#define HOLD_PRESS_TIME 1000

/**
 * @class Button
 * @brief A simple class to handle button inputs with debouncing and multiple press types.
 *
 * This class detects short (<1s) and hold (>1s) presses for a single button.
 */
class Button {
    int pin;
    int state;
    int lastReading;
    unsigned long lastDebounceTime;
    unsigned long pressedTime;
    bool isPressedState;
    bool holdHandled;

public:
    Button(int p) : pin(p), state(HIGH), lastReading(HIGH), lastDebounceTime(0),
                    pressedTime(0), isPressedState(false), holdHandled(false) {}
    
    /**
     * @brief Checks the button state and returns the type of press event.
     * @return 0 for no event, 1 for a short press, 2 for a hold.
     */
    int check() {
        int reading = digitalRead(pin);
        int event = 0;

        if (reading != lastReading) {
            lastDebounceTime = millis();
        }

        if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
            if (reading != state) {
                state = reading;
                if (state == LOW) { // Button pressed
                    pressedTime = millis();
                    isPressedState = true;
                    holdHandled = false;
                } else { // Button released
                    if (isPressedState) {
                        isPressedState = false;
                        if (!holdHandled) {
                             event = 1; // Short press
                        }
                    }
                }
            }
        }

        // Check for hold while the button is held down
        if (isPressedState) {
            unsigned long duration = millis() - pressedTime;
            if (!holdHandled && duration >= HOLD_PRESS_TIME) {
                event = 2; // Hold
                holdHandled = true;
            }
        }

        lastReading = reading;
        return event;
    }
};

Button btnUp(BTN_UP_PIN);
Button btnDown(BTN_DOWN_PIN);
Button btnEnter(BTN_ENTER_PIN);

DeviceType currentViewDevice = DeviceType::DELTA_3;

void handleAction(DisplayAction action);

/**
 * @brief Checks the Serial buffer for new commands.
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
 * @brief Standard Arduino setup function. Initializes all components.
 */
void setup() {
    // Hold Power
    pinMode(POWER_LATCH_PIN, OUTPUT);
    digitalWrite(POWER_LATCH_PIN, HIGH);

    Serial.begin(115200);
    Serial.println("Starting Ecoflow Controller...");

    setupDisplay();
    pinMode(BTN_UP_PIN, INPUT_PULLUP);
    pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
    pinMode(BTN_ENTER_PIN, INPUT_PULLUP);

    LightSensor::getInstance().begin();
    DeviceManager::getInstance().initialize();

    WebServer::begin();

    // Initialize UART1 for communication with STM32F4
    // RX on GPIO16, TX on GPIO17
    Serial1.begin(460800, SERIAL_8N1, 16, 17);
}

void sendBatteryStatus() {
    EcoflowESP32* d3 = DeviceManager::getInstance().getDevice(DeviceType::DELTA_3);
    if (!d3->isAuthenticated()) {
        return; // Don't send if not connected
    }

    BatteryStatus status = {0};
    status.soc = d3->getData().delta3.batteryLevel;
    status.power_w = d3->getData().delta3.inputPower - d3->getData().delta3.outputPower;
    status.voltage_v = 0; // This data is not available in the current struct
    status.connected = 1;
    strncpy(status.device_name, "Delta 3", sizeof(status.device_name) - 1);

    uint8_t buffer[sizeof(BatteryStatus) + 4];
    int len = pack_battery_status_message(buffer, &status);
    Serial1.write(buffer, len);
}

void checkUart() {
    while (Serial1.available() >= 4) { // Wait for a complete header + CRC
        if (Serial1.read() == START_BYTE) {
            uint8_t buffer[3];
            buffer[0] = Serial1.read(); // CMD
            buffer[1] = Serial1.read(); // LEN
            uint8_t received_crc = Serial1.read();

            uint8_t calculated_crc = calculate_crc8(buffer, 2);

            if (received_crc == calculated_crc) {
                if (buffer[0] == CMD_REQUEST_STATUS_UPDATE) {
                    sendBatteryStatus();
                } else {
                    // If we get an unexpected command, read the payload to clear the buffer
                    for(int i = 0; i < buffer[1]; i++) {
                        if(Serial1.available()) Serial1.read();
                    }
                }
            }
        }
    }
}


/**
 * @brief Standard Arduino loop function. Runs the main application logic.
 */
void loop() {
    static uint32_t last_display_update = 0;
    static uint32_t last_data_refresh = 0;

    // Update hardware sensors
    LightSensor::getInstance().update();

    // 1. Update the Device Manager (handles all BLE communication)
    DeviceManager::getInstance().update();

    // Check Serial for CLI commands
    checkSerial();

    // Check UART for commands from STM32F4
    checkUart();

    // 2. Check for button inputs and translate them to display actions
    DisplayAction action = DisplayAction::NONE;
    int upEvent = btnUp.check();
    int downEvent = btnDown.check();
    int enterEvent = btnEnter.check();

    if (upEvent == 1) action = handleDisplayInput(ButtonInput::BTN_UP);
    if (downEvent == 1) action = handleDisplayInput(ButtonInput::BTN_DOWN);
    if (enterEvent == 1) action = handleDisplayInput(ButtonInput::BTN_ENTER_SHORT);
    if (enterEvent == 2) action = handleDisplayInput(ButtonInput::BTN_ENTER_HOLD);

    // 3. Process the action from the user input
    handleAction(action);

    // 4. Process any pending action that might have been triggered by a timeout in the UI
    handleAction(getPendingAction());

    // 5. Periodically request fresh data from all connected devices
    if (millis() - last_data_refresh > 2000) {
        last_data_refresh = millis();

        // Send battery status to STM32F4
        sendBatteryStatus();

        EcoflowESP32* d3 = DeviceManager::getInstance().getDevice(DeviceType::DELTA_3);
        if (d3->isAuthenticated()) d3->requestData();
        EcoflowESP32* w2 = DeviceManager::getInstance().getDevice(DeviceType::WAVE_2);
        if (w2->isAuthenticated()) w2->requestData();
    }

    // 6. Update the display at a regular framerate
    if (millis() - last_display_update > 20) {
        last_display_update = millis();

        // Sync currentViewDevice with Display's priority logic unless user is manually overriding (TODO)
        // For now, let Display logic drive the view if we are in Dashboard state
        // But Display.cpp handles state.
        // We need to know what device is *actually* being displayed to send commands to it.
        // Display::getActiveDeviceType() returns the prioritized device.
        currentViewDevice = getActiveDeviceType();

        EcoflowESP32* activeDev = DeviceManager::getInstance().getDevice(currentViewDevice);
        DeviceSlot* activeSlot = DeviceManager::getInstance().getSlot(currentViewDevice);
        bool scanning = DeviceManager::getInstance().isScanning();
        updateDisplay(activeDev ? activeDev->_data : EcoflowData{}, activeSlot, scanning);
    }
}

/**
 * @brief Executes actions based on user input from the display.
 * @param action The action to be performed.
 */
void handleAction(DisplayAction action) {
    if (action == DisplayAction::NONE) return;

    // Handle device management actions first
    if (action == DisplayAction::CONNECT_DEVICE) {
        DeviceType target = getTargetDeviceType();
        DeviceManager::getInstance().scanAndConnect(target);
        currentViewDevice = target;
        return;
    }
    if (action == DisplayAction::DISCONNECT_DEVICE) {
        DeviceManager::getInstance().disconnect(getTargetDeviceType());
        return;
    }

    // Handle System Power Off
    if (action == DisplayAction::SYSTEM_OFF) {
        Serial.println("Power Off Requested. Releasing latch...");
        digitalWrite(POWER_LATCH_PIN, LOW);
        delay(1000);
        ESP.restart(); // Should lose power before this if latch works
        return;
    }

    // For control actions, ensure the device is authenticated
    EcoflowESP32* dev = DeviceManager::getInstance().getDevice(currentViewDevice);
    EcoflowESP32* w2 = DeviceManager::getInstance().getDevice(DeviceType::WAVE_2);

    if (dev && dev->isAuthenticated()) {
        switch(action) {
            case DisplayAction::TOGGLE_AC:
                dev->setAC(!dev->isAcOn());
                break;
            case DisplayAction::TOGGLE_DC:
                dev->setDC(!dev->isDcOn());
                break;
            case DisplayAction::TOGGLE_USB:
                dev->setUSB(!dev->isUsbOn());
                break;
            case DisplayAction::SET_AC_LIMIT:
                dev->setAcChargingLimit(getSetAcLimit());
                break;
            case DisplayAction::SET_SOC_LIMITS:
                dev->setBatterySOCLimits(getSetMaxChgSoc(), getSetMinDsgSoc());
                break;
            default: break;
        }
    }

    // Handle Wave 2 Specific Actions
    if (w2 && w2->isAuthenticated()) {
        switch(action) {
            case DisplayAction::W2_TOGGLE_PWR:
                // Deprecated, use W2_SET_PWR
                w2->setPowerState(w2->getData().wave2.powerMode == 1 ? 2 : 1);
                break;
            case DisplayAction::W2_SET_PWR:
                w2->setPowerState((uint8_t)getSetW2Val());
                break;
            case DisplayAction::W2_SET_MODE:
                // If OFF, turn ON first? User said: "if off should turn on the wave 2 and then set the mode"
                if (w2->getData().wave2.powerMode != 1) {
                    w2->setPowerState(1);
                    // We might need a delay or wait for state update, but for now we send both.
                    // The device might process them sequentially.
                    delay(200);
                }
                w2->setMainMode((uint8_t)getSetW2Val());
                break;
            case DisplayAction::W2_SET_FAN:
                 w2->setFanSpeed((uint8_t)getSetW2Val());
                 break;
            case DisplayAction::W2_SET_SUB_MODE:
                 w2->setSubMode((uint8_t)getSetW2Val());
                 break;
            default: break;
        }
    }
}
