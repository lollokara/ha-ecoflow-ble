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
#define LONG_PRESS_TIME 1000

// Simple Class for Button
class Button {
    int pin;
    int state;
    int lastReading;
    unsigned long lastDebounceTime;
    unsigned long pressedTime;
    bool isPressedState;
    bool longPressHandled;

public:
    Button(int p) : pin(p), state(HIGH), lastReading(HIGH), lastDebounceTime(0),
                    pressedTime(0), isPressedState(false), longPressHandled(false) {}
    
    // Returns: 0=None, 1=Short, 2=Long
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
                    longPressHandled = false;
                } else { // Released
                    if (isPressedState) {
                        if (!longPressHandled) {
                            event = 1; // Short Press
                        }
                        isPressedState = false;
                    }
                }
            }
        }

        // Check Long Press being held
        if (isPressedState && !longPressHandled) {
            if (millis() - pressedTime > LONG_PRESS_TIME) {
                event = 2; // Long Press
                longPressHandled = true;
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

// Current "Active View" device is based on what?
// Let's assume D3 is default, but we can switch it or display both.
// For simplicity, and based on previous code structure, let's cycle or show active.
// Since we added Device Selection menu, let's just show data for the device currently
// selected in the menu, OR default to D3 if in main menu but allow toggling?
// Actually, let's track "Active View Device" separately or just use the one
// corresponding to the last interacted Device Page?
// Let's use a simple global for now.
DeviceType currentViewDevice = DeviceType::DELTA_2;

void handleAction(DisplayAction action) {
    if (action == DisplayAction::NONE) return;

    // Handle Device Management Actions (Do not require authentication on the specific device yet)
    if (action == DisplayAction::CONNECT_DEVICE) {
        DeviceType target = getTargetDeviceType();
        ESP_LOGI("main", "Requesting Connection for Device Type %d", (int)target);
        DeviceManager::getInstance().scanAndConnect(target);
        // Switch view to this device
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
    if (enterEvent == 2) action = handleDisplayInput(ButtonInput::BTN_ENTER_LONG);

    // If user is navigating Device Menu, we might want to update currentViewDevice
    // to preview that device?
    // Let's implicitly switch view if user performs an action on it?
    // Or simpler: Toggle View with some key combo?
    // For now, let's assume if you go to DEV -> W2, the view switches to W2.
    if (action == DisplayAction::NONE) {
        // Check if menu state implies a target
        // This is a bit hacky but effective without adding more buttons
        // If we are in DEVICE_SELECT or DEVICE_ACTION, update currentViewDevice to target
        // DeviceType t = getTargetDeviceType();
        // currentViewDevice = t;
        // However, getTargetDeviceType returns D3 or W2.
        // We can update it continuously?
        // Maybe only when entering that menu?
        // Let's stick to: Main Menu shows *currentViewDevice*.
        // To switch it: User must go to DEV -> Select Device -> (maybe just by highlighting it?)
    }

    // 3. Process User Action
    handleAction(action);

    // 4. Process Pending Action (from Timeout)
    DisplayAction pending = getPendingAction();
    handleAction(pending);

    // Update View based on Menu Selection
    // If we are in a device-specific menu, ensure we view that device
    // getTargetDeviceType() returns the device currently being manipulated in the menu
    // This allows "previewing" the other device status while in the menu
    currentViewDevice = getTargetDeviceType();


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

        // Get Active Device Data
        EcoflowESP32* activeDev = DeviceManager::getInstance().getDevice(currentViewDevice);

        EcoflowData data;
        data.batteryLevel = activeDev->getBatteryLevel();
        data.inputPower = activeDev->getInputPower();
        data.outputPower = activeDev->getOutputPower();
        data.solarInputPower = activeDev->getSolarInputPower();
        data.acOutputPower = activeDev->getAcOutputPower();
        data.dcOutputPower = activeDev->getDcOutputPower();
        data.acOn = activeDev->isAcOn();
        data.dcOn = activeDev->isDcOn();
        data.usbOn = activeDev->isUsbOn();
        data.isConnected = activeDev->isConnected();

        // Log Wave 2 Temp for demonstration if active
        if (currentViewDevice == DeviceType::WAVE_2 && data.isConnected) {
            // We don't have a direct temp field in EcoflowData struct yet that maps 1:1 to Wave 2 specifics,
            // but we have getCellTemperature. Let's also print to Serial as requested.
            //ESP_LOGI("Wave2", "Temp Cell: %d, SolarInput: %d", activeDev->getCellTemperature(), activeDev->getSolarInputPower());
        }

        DeviceSlot* activeSlot = DeviceManager::getInstance().getSlot(currentViewDevice);
        bool scanning = DeviceManager::getInstance().isScanning();

        updateDisplay(data, activeSlot, scanning);
    }
}
