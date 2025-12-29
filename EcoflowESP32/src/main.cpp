
// ... (includes)
#include <Arduino.h>
#include "EcoflowESP32.h"
#include "Credentials.h"
#include "Display.h"
#include "DeviceManager.h"
#include "CmdUtils.h"
#include "WebServer.h"
#include "LightSensor.h"
#include "ecoflow_protocol.h"

// ... (Button class and definitions)

// Define GPIO pins for the buttons
#define BTN_UP_PIN    4
#define BTN_DOWN_PIN  5
#define BTN_ENTER_PIN 6
#define POWER_LATCH_PIN 12

// Timings for button press detection (in milliseconds)
#define DEBOUNCE_DELAY 50
#define HOLD_PRESS_TIME 1000

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

void setup() {
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

    Serial1.begin(460800, SERIAL_8N1, 16, 17);
}

// ... (sendBatteryStatus deprecated but kept for compatibility if needed, replaced by new functions)

void sendDeviceList() {
    DeviceList list = {0};
    DeviceSlot* slots[] = {
        DeviceManager::getInstance().getSlot(DeviceType::DELTA_3),
        DeviceManager::getInstance().getSlot(DeviceType::WAVE_2),
        DeviceManager::getInstance().getSlot(DeviceType::DELTA_PRO_3),
        DeviceManager::getInstance().getSlot(DeviceType::ALTERNATOR_CHARGER)
    };

    uint8_t count = 0;
    for (int i = 0; i < 4; i++) {
        if (slots[i] && slots[i]->isConnected) {
            list.devices[count].id = (uint8_t)slots[i]->type;
            strncpy(list.devices[count].name, slots[i]->name.c_str(), sizeof(list.devices[count].name) - 1);
            list.devices[count].connected = 1;
            count++;
        }
    }
    list.count = count;

    uint8_t buffer[sizeof(DeviceList) + 4];
    int len = pack_device_list_message(buffer, &list);
    Serial1.write(buffer, len);
}

void sendDeviceStatus(uint8_t device_id) {
    DeviceType type = (DeviceType)device_id;
    EcoflowESP32* dev = DeviceManager::getInstance().getDevice(type);

    if (!dev || !dev->isAuthenticated()) return;

    DeviceStatus status = {0};
    status.id = device_id;

    // Fill BatteryStatus part
    if (type == DeviceType::DELTA_3) {
        status.status.soc = dev->getData().delta3.batteryLevel;
        status.status.power_w = dev->getData().delta3.inputPower - dev->getData().delta3.outputPower;
        status.status.voltage_v = 0;
        status.status.connected = 1;
        strncpy(status.status.device_name, "Delta 3", 15);
    } else if (type == DeviceType::WAVE_2) {
        // Wave 2 data mapping (simplified for now)
        status.status.soc = 0; // Wave 2 might not have battery
        status.status.power_w = 0;
        status.status.voltage_v = 0;
        status.status.connected = 1;
        strncpy(status.status.device_name, "Wave 2", 15);
    }

    uint8_t buffer[sizeof(DeviceStatus) + 4];
    int len = pack_device_status_message(buffer, &status);
    Serial1.write(buffer, len);
}

void checkUart() {
    static uint8_t rx_buf[260];
    static uint8_t rx_idx = 0;
    static uint8_t expected_len = 0;
    static bool collecting = false;

    while (Serial1.available()) {
        uint8_t b = Serial1.peek();

        if (!collecting) {
            if (b == START_BYTE) {
                collecting = true;
                rx_idx = 0;
                rx_buf[rx_idx++] = Serial1.read();
            } else {
                Serial1.read(); // Discard garbage
            }
        } else {
            // We are collecting, just read everything
            rx_buf[rx_idx++] = Serial1.read();

            if (rx_idx == 3) { // We have START, CMD, LEN
                expected_len = rx_buf[2];
            }

            if (rx_idx >= 3 && rx_idx == (4 + expected_len)) {
                // Packet complete
                uint8_t received_crc = rx_buf[rx_idx - 1];
                uint8_t calculated_crc = calculate_crc8(&rx_buf[1], 2 + expected_len);

                if (received_crc == calculated_crc) {
                    uint8_t cmd = rx_buf[1];
                    if (cmd == CMD_HANDSHAKE) {
                            uint8_t ack[4];
                            int len = pack_handshake_ack_message(ack);
                            Serial1.write(ack, len);
                            sendDeviceList();
                    } else if (cmd == CMD_GET_DEVICE_STATUS) {
                            uint8_t dev_id;
                            if (unpack_get_device_status_message(rx_buf, &dev_id) == 0) {
                                sendDeviceStatus(dev_id);
                            }
                    }
                }
                collecting = false; // Reset for next packet
            }

            if (rx_idx >= sizeof(rx_buf)) {
                collecting = false; // Overflow protection, reset
            }
        }
    }
}

void loop() {
    static uint32_t last_display_update = 0;
    static uint32_t last_data_refresh = 0;

    LightSensor::getInstance().update();
    DeviceManager::getInstance().update();
    checkSerial();
    checkUart();

    DisplayAction action = DisplayAction::NONE;
    int upEvent = btnUp.check();
    int downEvent = btnDown.check();
    int enterEvent = btnEnter.check();

    if (upEvent == 1) action = handleDisplayInput(ButtonInput::BTN_UP);
    if (downEvent == 1) action = handleDisplayInput(ButtonInput::BTN_DOWN);
    if (enterEvent == 1) action = handleDisplayInput(ButtonInput::BTN_ENTER_SHORT);
    if (enterEvent == 2) action = handleDisplayInput(ButtonInput::BTN_ENTER_HOLD);

    handleAction(action);
    handleAction(getPendingAction());

    if (millis() - last_data_refresh > 2000) {
        last_data_refresh = millis();

        // sendBatteryStatus(); // Replaced by request/response from F4

        EcoflowESP32* d3 = DeviceManager::getInstance().getDevice(DeviceType::DELTA_3);
        if (d3 && d3->isAuthenticated()) d3->requestData();
        EcoflowESP32* w2 = DeviceManager::getInstance().getDevice(DeviceType::WAVE_2);
        if (w2 && w2->isAuthenticated()) w2->requestData();
    }

    if (millis() - last_display_update > 20) {
        last_display_update = millis();
        currentViewDevice = getActiveDeviceType();

        EcoflowESP32* activeDev = DeviceManager::getInstance().getDevice(currentViewDevice);
        DeviceSlot* activeSlot = DeviceManager::getInstance().getSlot(currentViewDevice);
        bool scanning = DeviceManager::getInstance().isScanning();
        updateDisplay(activeDev ? activeDev->_data : EcoflowData{}, activeSlot, scanning);
    }
}

void handleAction(DisplayAction action) {
    if (action == DisplayAction::NONE) return;

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

    if (action == DisplayAction::SYSTEM_OFF) {
        Serial.println("Power Off Requested. Releasing latch...");
        digitalWrite(POWER_LATCH_PIN, LOW);
        delay(1000);
        ESP.restart();
        return;
    }

    EcoflowESP32* dev = DeviceManager::getInstance().getDevice(currentViewDevice);
    EcoflowESP32* w2 = DeviceManager::getInstance().getDevice(DeviceType::WAVE_2);

    if (dev && dev->isAuthenticated()) {
        switch(action) {
            case DisplayAction::TOGGLE_AC: dev->setAC(!dev->isAcOn()); break;
            case DisplayAction::TOGGLE_DC: dev->setDC(!dev->isDcOn()); break;
            case DisplayAction::TOGGLE_USB: dev->setUSB(!dev->isUsbOn()); break;
            case DisplayAction::SET_AC_LIMIT: dev->setAcChargingLimit(getSetAcLimit()); break;
            case DisplayAction::SET_SOC_LIMITS: dev->setBatterySOCLimits(getSetMaxChgSoc(), getSetMinDsgSoc()); break;
            default: break;
        }
    }

    if (w2 && w2->isAuthenticated()) {
        switch(action) {
            case DisplayAction::W2_TOGGLE_PWR: w2->setPowerState(w2->getData().wave2.powerMode == 1 ? 2 : 1); break;
            case DisplayAction::W2_SET_PWR: w2->setPowerState((uint8_t)getSetW2Val()); break;
            case DisplayAction::W2_SET_MODE:
                if (w2->getData().wave2.powerMode != 1) {
                    w2->setPowerState(1);
                    delay(200);
                }
                w2->setMainMode((uint8_t)getSetW2Val());
                break;
            case DisplayAction::W2_SET_FAN: w2->setFanSpeed((uint8_t)getSetW2Val()); break;
            case DisplayAction::W2_SET_SUB_MODE: w2->setSubMode((uint8_t)getSetW2Val()); break;
            default: break;
        }
    }
}
