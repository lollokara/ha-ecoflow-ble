#include <Arduino.h>
#include "EcoflowESP32.h"
#include "Credentials.h"
#include "Display.h"

EcoflowESP32 ecoflow;

// Replace with your device's BLE address
const std::string ble_address = "7c:2c:67:44:a4:3e";

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
    // while (!Serial) { ; } // Don't block if no serial connected
    Serial.println("Starting Ecoflow Controller...");

    // Initialize Display
    setupDisplay();

    // Initialize Buttons
    pinMode(BTN_UP_PIN, INPUT_PULLUP);
    pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
    pinMode(BTN_ENTER_PIN, INPUT_PULLUP);

    ecoflow.begin(ECOFLOW_USER_ID, ECOFLOW_DEVICE_SN, ble_address);
}

void loop() {
    static uint32_t last_display_update = 0;
    static uint32_t last_data_refresh = 0;

    // 1. BLE Update (Frequent)
    ecoflow.update();

    // 2. Handle Inputs
    int upEvent = btnUp.check();
    int downEvent = btnDown.check();
    int enterEvent = btnEnter.check();

    DisplayAction action = DisplayAction::NONE;

    if (upEvent == 1) action = handleDisplayInput(ButtonInput::BTN_UP);
    if (downEvent == 1) action = handleDisplayInput(ButtonInput::BTN_DOWN);
    if (enterEvent == 1) action = handleDisplayInput(ButtonInput::BTN_ENTER_SHORT);
    if (enterEvent == 2) action = handleDisplayInput(ButtonInput::BTN_ENTER_LONG);

    // 3. Process Action
    if (action != DisplayAction::NONE) {
        if (ecoflow.isAuthenticated()) {
            switch(action) {
                case DisplayAction::TOGGLE_AC:
                    ESP_LOGI("main", "Toggling AC");
                    ecoflow.setAC(!ecoflow.isAcOn());
                    break;
                case DisplayAction::TOGGLE_DC:
                    ESP_LOGI("main", "Toggling DC");
                    ecoflow.setDC(!ecoflow.isDcOn());
                    break;
                case DisplayAction::TOGGLE_USB:
                    ESP_LOGI("main", "Toggling USB");
                    ecoflow.setUSB(!ecoflow.isUsbOn());
                    break;
                default: break;
            }
            // Force a quicker refresh or wait for notify?
            // Usually wait for notify, but maybe we update display optimistically?
            // For now, let the data update naturally.
        } else {
            ESP_LOGI("main", "Action Ignored: Not Authenticated");
        }
    }

    // 4. Refresh Data (Periodic)
    // User requested every 2s
    if (millis() - last_data_refresh > 2000) {
        last_data_refresh = millis();
        if (ecoflow.isAuthenticated()) {
            ecoflow.requestData();
        }
    }

    // 5. Update Display (Animation Framerate ~30-50fps => 20-33ms)
    if (millis() - last_display_update > 20) {
        last_display_update = millis();

        EcoflowData data;
        data.batteryLevel = ecoflow.getBatteryLevel();
        data.inputPower = ecoflow.getInputPower();
        data.outputPower = ecoflow.getOutputPower();
        data.solarInputPower = ecoflow.getSolarInputPower();
        data.acOutputPower = ecoflow.getAcOutputPower();
        data.dcOutputPower = ecoflow.getDcOutputPower();
        // Update Toggle States
        data.acOn = ecoflow.isAcOn();
        data.dcOn = ecoflow.isDcOn();
        data.usbOn = ecoflow.isUsbOn();

        updateDisplay(data);
    }
}
