#include <Arduino.h>
#include "EcoflowESP32.h"
#include "Credentials.h"
#include "Display.h"
#include "DeviceManager.h"

// Button Pins
#define BTN_UP_PIN    4
#define BTN_DOWN_PIN  5
#define BTN_ENTER_PIN 6

#define DEBOUNCE_DELAY 50
#define MEDIUM_PRESS_TIME 1000
#define LONG_PRESS_TIME 3000

// Simple Class for Button
class Button {
    int pin;
    int state;
    int lastReading;
    unsigned long lastDebounceTime;
    unsigned long pressedTime;
    bool isPressedState;
    bool mediumPressHandled;
    bool longPressHandled;

public:
    Button(int p) : pin(p), state(HIGH), lastReading(HIGH), lastDebounceTime(0),
                    pressedTime(0), isPressedState(false), mediumPressHandled(false), longPressHandled(false) {}
    
    // Returns: 0=None, 1=Short, 2=Medium(1s), 3=Long(3s)
    int check() {
        int reading = digitalRead(pin);
        int event = 0;

        if (reading != lastReading) {
            lastDebounceTime = millis();
        }

        if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
            if (reading != state) {
                state = reading;

                if (state == LOW) { // Pressed
                    pressedTime = millis();
                    isPressedState = true;
                    mediumPressHandled = false;
                    longPressHandled = false;
                } else { // Released
                    if (isPressedState) {
                        unsigned long duration = millis() - pressedTime;
                        isPressedState = false;

                        if (!longPressHandled) { // If we haven't already triggered the long press action
                            if (duration >= MEDIUM_PRESS_TIME) {
                                // Released between 1s and 3s -> Medium Press (Back)
                                event = 2;
                            } else {
                                // Released < 1s -> Short Press
                                event = 1;
                            }
                        }
                    }
                }
            }
        }

        // Check Long Press being held (Trigger immediately at 3s)
        if (isPressedState && !longPressHandled) {
            if (millis() - pressedTime >= LONG_PRESS_TIME) {
                event = 3; // Long Press (3s)
                longPressHandled = true; // Prevent multiple triggers and prevent release event
            }
        }

        lastReading = reading;
        return event;
    }
};

Button btnUp(BTN_UP_PIN);
Button btnDown(BTN_DOWN_PIN);
Button btnEnter(BTN_ENTER_PIN);

void setup() {
    Serial.begin(115200);
    // while (!Serial) { ; }
    Serial.println("Starting Ecoflow Controller...");

    // Initialize Display
    setupDisplay();

    // Initialize Buttons
    pinMode(BTN_UP_PIN, INPUT_PULLUP);
    pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
    pinMode(BTN_ENTER_PIN, INPUT_PULLUP);

    // Initialize Device Manager
    DeviceManager::getInstance().initialize();
}

DeviceType currentViewDevice = DeviceType::DELTA_2;

void handleAction(DisplayAction action) {
    if (action == DisplayAction::NONE) return;

    // Handle Device Management Actions
    if (action == DisplayAction::CONNECT_DEVICE) {
        DeviceType target = getTargetDeviceType();
        ESP_LOGI("main", "Requesting Connection for Device Type %d", (int)target);
        DeviceManager::getInstance().scanAndConnect(target);
        currentViewDevice = target;
        return;
    }

    if (action == DisplayAction::DISCONNECT_DEVICE) {
        DeviceType target = getTargetDeviceType();
        ESP_LOGI("main", "Disconnecting Device Type %d", (int)target);
        DeviceManager::getInstance().disconnect(target);
        return;
    }

    // Handle Control Actions (Require Authentication on the ACTIVE VIEW device)
    EcoflowESP32* dev = DeviceManager::getInstance().getDevice(currentViewDevice);

    if (dev && dev->isAuthenticated()) {
        switch(action) {
            case DisplayAction::TOGGLE_AC:
                ESP_LOGI("main", "Toggling AC on active device");
                dev->setAC(!dev->isAcOn());
                break;
            case DisplayAction::TOGGLE_DC:
                ESP_LOGI("main", "Toggling DC on active device");
                dev->setDC(!dev->isDcOn());
                break;
            case DisplayAction::TOGGLE_USB:
                ESP_LOGI("main", "Toggling USB on active device");
                dev->setUSB(!dev->isUsbOn());
                break;
            case DisplayAction::SET_AC_LIMIT:
                {
                    int limit = getSetAcLimit();
                    ESP_LOGI("main", "Setting AC Charging Limit to %dW", limit);
                    dev->setAcChargingLimit(limit);
                }
                break;
            case DisplayAction::SET_SOC_LIMITS:
                {
                    int maxC = getSetMaxChgSoc();
                    int minD = getSetMinDsgSoc();
                    ESP_LOGI("main", "Setting SOC Limits: MaxChg=%d, MinDsg=%d", maxC, minD);
                    dev->setBatterySOCLimits(maxC, minD);
                }
                break;
            default: break;
        }
    } else {
        ESP_LOGI("main", "Action Ignored: Active Device Not Authenticated");
    }
}

void loop() {
    static uint32_t last_display_update = 0;
    static uint32_t last_data_refresh = 0;

    // 1. Update Device Manager (Handles BLE updates for all instances)
    DeviceManager::getInstance().update();

    // 2. Handle Inputs
    int upEvent = btnUp.check();
    int downEvent = btnDown.check();
    int enterEvent = btnEnter.check();

    DisplayAction action = DisplayAction::NONE;

    if (upEvent == 1) action = handleDisplayInput(ButtonInput::BTN_UP);
    if (downEvent == 1) action = handleDisplayInput(ButtonInput::BTN_DOWN);

    if (enterEvent == 1) action = handleDisplayInput(ButtonInput::BTN_ENTER_SHORT);
    if (enterEvent == 2) action = handleDisplayInput(ButtonInput::BTN_ENTER_MEDIUM); // 1s Back
    if (enterEvent == 3) action = handleDisplayInput(ButtonInput::BTN_ENTER_LONG);   // 3s Action

    // 3. Process User Action
    handleAction(action);

    // 4. Process Pending Action (from Timeout)
    DisplayAction pending = getPendingAction();
    handleAction(pending);

    // 5. Refresh Data (Periodic)
    if (millis() - last_data_refresh > 2000) {
        last_data_refresh = millis();

        // Request data for ALL authenticated devices
        EcoflowESP32* d3 = DeviceManager::getInstance().getDevice(DeviceType::DELTA_2);
        if (d3->isAuthenticated()) d3->requestData();

        EcoflowESP32* w2 = DeviceManager::getInstance().getDevice(DeviceType::WAVE_2);
        if (w2->isAuthenticated()) w2->requestData();
    }

    // 6. Update Display (Animation Framerate ~30-50fps => 20-33ms)
    if (millis() - last_display_update > 20) {
        last_display_update = millis();

        EcoflowESP32* activeDev = DeviceManager::getInstance().getDevice(currentViewDevice);
        DeviceSlot* activeSlot = DeviceManager::getInstance().getSlot(currentViewDevice);
        bool scanning = DeviceManager::getInstance().isScanning();

        if (activeDev) {
             // Pass the actual internal data struct directly
             // This works because we updated EcoflowESP32 class to hold the new EcoflowData struct
             // and updated Display.cpp to handle it.
             updateDisplay(activeDev->_data, activeSlot, scanning);
        } else {
             EcoflowData empty;
             updateDisplay(empty, activeSlot, scanning);
        }
    }
}
