#include <Arduino.h>
#include "EcoflowESP32.h"
#include "Credentials.h"
#include "Display.h"

EcoflowESP32 ecoflow;

// Replace with your device's BLE address
const std::string ble_address = "7c:2c:67:44:a4:3e";

// Button Pins
#define BTN_AC_PIN 4
#define BTN_DC_PIN 5
#define BTN_USB_PIN 6

// Debounce settings
#define DEBOUNCE_DELAY 50

void setup() {
    Serial.begin(115200);
    while (!Serial) {
        ; // wait for serial port to connect
    }
    Serial.println("Starting...");

    // Initialize Display
    setupDisplay();

    // Initialize Buttons
    pinMode(BTN_AC_PIN, INPUT_PULLUP);
    pinMode(BTN_DC_PIN, INPUT_PULLUP);
    pinMode(BTN_USB_PIN, INPUT_PULLUP);

    ecoflow.begin(ECOFLOW_USER_ID, ECOFLOW_DEVICE_SN, ble_address);
}

// Simple Class for Button to clean up main loop
class Button {
    int pin;
    int state;
    int lastReading;
    unsigned long lastDebounceTime;
    
public:
    Button(int p) : pin(p), state(HIGH), lastReading(HIGH), lastDebounceTime(0) {}
    
    bool isPressed() {
        int reading = digitalRead(pin);
        bool pressed = false;

        if (reading != lastReading) {
            lastDebounceTime = millis();
        }

        if ((millis() - lastDebounceTime) > DEBOUNCE_DELAY) {
            if (reading != state) {
                state = reading;
                if (state == LOW) {
                    pressed = true;
                }
            }
        }

        lastReading = reading;
        return pressed;
    }
};

Button btnAC(BTN_AC_PIN);
Button btnDC(BTN_DC_PIN);
Button btnUSB(BTN_USB_PIN);

void loop() {
    static uint32_t last_check = 0;

    ecoflow.update();
    
    // Update Display
    // We pass the EcoflowData struct directly.
    // We need to access the raw data struct or use getters.
    // EcoflowESP32 class has getters.
    // Display needs `EcoflowData`.
    // Let's construct a temporary data object or add a method to EcoflowESP32 to get the struct.
    // Alternatively, modify Display to take individual values or the EcoflowESP32 object.
    // But `Display.h` includes `EcoflowData.h`, so let's use that.
    
    EcoflowData data;
    data.batteryLevel = ecoflow.getBatteryLevel();
    data.inputPower = ecoflow.getInputPower();
    data.outputPower = - ecoflow.getOutputPower();
    data.acOutputPower = - ecoflow.getAcOutputPower();
    data.dcOutputPower = - ecoflow.getDcOutputPower();
    data.solarInputPower = ecoflow.getSolarInputPower();
    // data.cellTemperature = ecoflow.getCellTemperature();
    // data.acOn = ecoflow.isAcOn();
    // data.dcOn = ecoflow.isDcOn();
    // data.usbOn = ecoflow.isUsbOn();
    
    updateDisplay(data);

    // Handle Buttons
    if (btnAC.isPressed()) {
        if (ecoflow.isAuthenticated()) {
            bool newState = !ecoflow.isAcOn();
            ESP_LOGI("main", "Toggling AC to %s", newState ? "ON" : "OFF");
            ecoflow.setAC(newState);
        } else {
             ESP_LOGI("main", "Cannot toggle AC: Not Authenticated");
        }
    }

    if (btnDC.isPressed()) {
        if (ecoflow.isAuthenticated()) {
            bool newState = !ecoflow.isDcOn();
            ESP_LOGI("main", "Toggling DC to %s", newState ? "ON" : "OFF");
            ecoflow.setDC(newState);
        } else {
             ESP_LOGI("main", "Cannot toggle DC: Not Authenticated");
        }
    }

    if (btnUSB.isPressed()) {
        if (ecoflow.isAuthenticated()) {
            bool newState = !ecoflow.isUsbOn();
            ESP_LOGI("main", "Toggling USB to %s", newState ? "ON" : "OFF");
            ecoflow.setUSB(newState);
        } else {
             ESP_LOGI("main", "Cannot toggle USB: Not Authenticated");
        }
    }

    if (millis() - last_check > 5000) {
        last_check = millis();
        if (ecoflow.isAuthenticated()) {
            // Print sensor data
            ESP_LOGI("main", "--- Sensor Data ---");
            ESP_LOGI("main", "Battery: %d%%", ecoflow.getBatteryLevel());
            ESP_LOGI("main", "Input: %dW, Output: %dW", ecoflow.getInputPower(), ecoflow.getOutputPower());
            ESP_LOGI("main", "Solar Input: %dW", ecoflow.getSolarInputPower());
            ESP_LOGI("main", "AC Output: %dW", ecoflow.getAcOutputPower());
            ESP_LOGI("main", "DC Output: %dW", ecoflow.getDcOutputPower());
            ESP_LOGI("main", "Cell Temp: %dC", ecoflow.getCellTemperature());
            ESP_LOGI("main", "State: AC=%d, DC=%d, USB=%d", ecoflow.isAcOn(), ecoflow.isDcOn(), ecoflow.isUsbOn());
            ESP_LOGI("main", "-------------------");
        }
    }

    // Commented out debugging loop as requested
    /*
    // Test toggling routines
    if (millis() - last_dc_toggle > 20000) {
        last_dc_toggle = millis();
        if (ecoflow.isAuthenticated()) {
            dc_on = !dc_on;
            ESP_LOGI("main", "Toggling DC to %s", dc_on ? "ON" : "OFF");
            ecoflow.setDC(dc_on);
        }
    }

    if (millis() - last_usb_toggle > 25000) {
        last_usb_toggle = millis();
        if (ecoflow.isAuthenticated()) {
            usb_on = !usb_on;
            ESP_LOGI("main", "Toggling USB to %s", usb_on ? "ON" : "OFF");
            ecoflow.setUSB(usb_on);
        }
    }

    if (millis() - last_ac_toggle > 30000) {
        last_ac_toggle = millis();
        if (ecoflow.isAuthenticated()) {
            ac_on = !ac_on;
            ESP_LOGI("main", "Toggling AC to %s", ac_on ? "ON" : "OFF");
            ecoflow.setAC(ac_on);
        }
    }

    // Dynamic AC Charging Limit Test
    if (millis() - last_chg_limit_update > 10000) {
        last_chg_limit_update = millis();
        if (ecoflow.isAuthenticated()) {
            ESP_LOGI("main", "Setting AC Charging Limit to %dW", chg_limit_watts);
            ecoflow.setAcChargingLimit(chg_limit_watts);

            chg_limit_watts += 50;
            if (chg_limit_watts > 600) {
                chg_limit_watts = 400;
            }
        }
    }
    */

    delay(50); // Reduced delay for better button responsiveness
}
