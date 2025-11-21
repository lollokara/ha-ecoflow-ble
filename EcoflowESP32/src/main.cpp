#include <Arduino.h>
#include "EcoflowESP32.h"
#include "Credentials.h"
#include "Display.h"

EcoflowESP32 ecoflow;

const std::string ble_address = "7c:2c:67:44:a4:3e";

#define BTN_UP_PIN    4
#define BTN_DOWN_PIN  5
#define BTN_ENTER_PIN 6

#define DEBOUNCE_DELAY 50
#define LONG_PRESS_TIME 1000
#define VERY_LONG_PRESS_TIME 3000

class Button {
    int pin;
    int state;
    int lastReading;
    unsigned long lastDebounceTime;
    unsigned long pressedTime;
    bool isPressedState;
    bool longPressHandled;
    bool veryLongPressHandled;

public:
    Button(int p) : pin(p), state(HIGH), lastReading(HIGH), lastDebounceTime(0),
                    pressedTime(0), isPressedState(false), longPressHandled(false), veryLongPressHandled(false) {}
    
    // Returns: 0=None, 1=Short, 2=Long, 3=VeryLong (Immediate)
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
                    veryLongPressHandled = false;
                } else { // Released
                    if (isPressedState) {
                        if (!veryLongPressHandled && !longPressHandled) {
                            event = 1; // Short Press
                        }
                        // If long press handled but not very long, maybe trigger release event?
                        // For now, standard short press logic.
                        isPressedState = false;
                    }
                }
            }
        }

        // Check Held Logic
        if (isPressedState) {
            unsigned long duration = millis() - pressedTime;

            if (duration > VERY_LONG_PRESS_TIME) {
                if (!veryLongPressHandled) {
                    event = 3; // Very Long Press (Immediate)
                    veryLongPressHandled = true;
                    longPressHandled = true; // Suppress Long
                }
            } else if (duration > LONG_PRESS_TIME) {
                // Long Press usually waits for release or is handled once?
                // Previous code: checked on release or hold?
                // "Clicking the central button ... holding it for 1s ... will toggle".
                // Usually toggle on release of long press, or immediate?
                // User said "as soon as it reaches 3s" for the new one.
                // For 1s, let's keep it on release or hold?
                // To avoid Short press, we must handle it.
                // Let's trigger Long Press event only if released?
                // Actually, if we wait for release, we can distinguish.
                // But 3s is "as soon as".
                // So 3s overrides everything.

                // Strategy:
                // < 1s release: Short
                // > 1s release: Long
                // > 3s hold: Very Long (Immediate)

                // So we don't return '2' here immediately. We return '2' on release if !veryLongPressHandled.
                // Wait, previously I implemented Long Press Logic?
                // Let's check previous implementation.
                /*
                if (millis() - pressedTime > LONG_PRESS_TIME) {
                    event = 2;
                    longPressHandled = true;
                }
                */
                // It was immediate. This conflicts with 3s immediate.
                // If we return 2 at 1s, we trigger toggle. Then at 3s we trigger back.
                // This might be annoying (Toggle then Back).
                // Solution: Long Press (1s) should effectively be "Medium Press".
                // If we want 3s to be exclusive, 1s must wait for release OR we accept overlap.
                // "pressing the central button for 3s ... should get back as soon as it reaches 3s".
                // "holding it for 1s ... will toggle".
                // If I hold for 3s, I pass 1s mark.
                // If 1s is immediate, I will toggle, then 2s later go back.
                // This seems acceptable? Or should 3s cancel 1s?
                // To cancel, 1s must wait. But 1s immediate feels snappier.
                // Let's assume overlap is OK or user releases before 3s for toggle.
                // If user wants "Back", they press 3s. If they accidentally toggle, it's a side effect.
                // Ideally: Detect intent. But "Immediate" requirements conflict.
                // Let's stick to: 1s Immediate (Toggle), 3s Immediate (Back).
                // Maybe 1s is NOT immediate?
                // "Clicking the central button here (maybe holding it for 1s is better here) will toggle"
                // Let's make 1s wait for release? No, display feedback is nice.
                // Let's implement:
                // 1s hold -> Event 2.
                // 3s hold -> Event 3.
                // Application handles it.

                if (!longPressHandled) {
                    event = 2;
                    longPressHandled = true;
                }
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
    Serial.println("Starting Ecoflow Controller...");

    setupDisplay();

    pinMode(BTN_UP_PIN, INPUT_PULLUP);
    pinMode(BTN_DOWN_PIN, INPUT_PULLUP);
    pinMode(BTN_ENTER_PIN, INPUT_PULLUP);

    ecoflow.begin(ECOFLOW_USER_ID, ECOFLOW_DEVICE_SN, ble_address);
}

void handleAction(DisplayAction action) {
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
                case DisplayAction::SET_AC_LIMIT:
                    ESP_LOGI("main", "Setting AC Limit: %dW", getSetAcLimit());
                    ecoflow.setAcChargingLimit(getSetAcLimit());
                    break;
                case DisplayAction::SET_MAX_CHG:
                    ESP_LOGI("main", "Setting Max Charge: %d%%", getSetMaxCharge());
                    ecoflow.setMaxChargeLevel(getSetMaxCharge());
                    break;
                case DisplayAction::SET_MIN_DSG:
                    ESP_LOGI("main", "Setting Min Discharge: %d%%", getSetMinDischarge());
                    ecoflow.setMinDischargeLevel(getSetMinDischarge());
                    break;
                default: break;
            }
        } else {
            ESP_LOGI("main", "Action Ignored: Not Authenticated");
        }
    }
}

void loop() {
    static uint32_t last_display_update = 0;
    static uint32_t last_data_refresh = 0;

    ecoflow.update();

    int upEvent = btnUp.check();
    int downEvent = btnDown.check();
    int enterEvent = btnEnter.check();

    DisplayAction action = DisplayAction::NONE;

    if (upEvent == 1) action = handleDisplayInput(ButtonInput::BTN_UP);
    if (downEvent == 1) action = handleDisplayInput(ButtonInput::BTN_DOWN);
    if (enterEvent == 1) action = handleDisplayInput(ButtonInput::BTN_ENTER_SHORT);
    if (enterEvent == 2) action = handleDisplayInput(ButtonInput::BTN_ENTER_LONG);
    if (enterEvent == 3) action = handleDisplayInput(ButtonInput::BTN_ENTER_VERY_LONG);

    handleAction(action);

    DisplayAction pending = getPendingAction();
    handleAction(pending);

    if (millis() - last_data_refresh > 2000) {
        last_data_refresh = millis();
        if (ecoflow.isAuthenticated()) {
            ecoflow.requestData();
        }
    }

    if (millis() - last_display_update > 20) {
        last_display_update = millis();

        EcoflowData data;
        data.batteryLevel = ecoflow.getBatteryLevel();
        data.inputPower = ecoflow.getInputPower();
        data.outputPower = ecoflow.getOutputPower();
        data.solarInputPower = ecoflow.getSolarInputPower();
        data.acOutputPower = ecoflow.getAcOutputPower();
        data.dcOutputPower = ecoflow.getDcOutputPower();
        data.acOn = ecoflow.isAcOn();
        data.dcOn = ecoflow.isDcOn();
        data.usbOn = ecoflow.isUsbOn();
        data.isConnected = ecoflow.isConnected();
        data.maxChargeLevel = ecoflow.getMaxChargeLevel();
        data.minDischargeLevel = ecoflow.getMinDischargeLevel();

        updateDisplay(data);
    }
}
