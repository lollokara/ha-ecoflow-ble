
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
static const char* TAG = "Main";

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

    // User reported wiring: F4 PG14 (TX) -> ESP 16, F4 PG9 (RX) -> ESP 17
    // Standard ESP32 Serial1: RX=16, TX=17.
    // If connected directly (TX->TX, RX->RX), we need to swap pins in software.
    // RX Pin = 17 (connected to F4 TX)
    // TX Pin = 16 (connected to F4 RX)
    Serial1.begin(115200, SERIAL_8N1, 16, 17);
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
    status.connected = 1;

    if (type == DeviceType::DELTA_3) {
        strncpy(status.name, "Delta 3", 15);
        const Delta3Data& src = dev->getData().delta3;
        Delta3DataStruct& dst = status.data.d3;

        dst.batteryLevel = src.batteryLevel;
        dst.acInputPower = src.acInputPower;
        dst.acOutputPower = src.acOutputPower;
        dst.inputPower = src.inputPower;
        dst.outputPower = src.outputPower;
        dst.dc12vOutputPower = src.dc12vOutputPower;
        dst.dcPortInputPower = src.dcPortInputPower;
        dst.dcPortState = src.dcPortState;
        dst.usbcOutputPower = src.usbcOutputPower;
        dst.usbc2OutputPower = src.usbc2OutputPower;
        dst.usbaOutputPower = src.usbaOutputPower;
        dst.usba2OutputPower = src.usba2OutputPower;
        dst.pluggedInAc = src.pluggedInAc;
        dst.energyBackup = src.energyBackup;
        dst.energyBackupBatteryLevel = src.energyBackupBatteryLevel;
        dst.batteryInputPower = src.batteryInputPower;
        dst.batteryOutputPower = src.batteryOutputPower;
        dst.batteryChargeLimitMin = src.batteryChargeLimitMin;
        dst.batteryChargeLimitMax = src.batteryChargeLimitMax;
        dst.cellTemperature = src.cellTemperature;
        dst.dc12vPort = src.dc12vPort;
        dst.acPorts = src.acPorts;
        dst.solarInputPower = src.solarInputPower;
        dst.acChargingSpeed = src.acChargingSpeed;
        dst.maxAcChargingPower = src.maxAcChargingPower;
        dst.acOn = src.acOn;
        dst.dcOn = src.dcOn;
        dst.usbOn = src.usbOn;

    } else if (type == DeviceType::WAVE_2) {
        strncpy(status.name, "Wave 2", 15);
        const Wave2Data& src = dev->getData().wave2;
        Wave2DataStruct& dst = status.data.w2;

        dst.mode = src.mode;
        dst.subMode = src.subMode;
        dst.setTemp = src.setTemp;
        dst.fanValue = src.fanValue;
        dst.envTemp = src.envTemp;
        dst.tempSys = src.tempSys;
        dst.batSoc = src.batSoc;
        dst.remainingTime = src.remainingTime;
        dst.powerMode = src.powerMode;
        dst.batPwrWatt = src.batPwrWatt;

    } else if (type == DeviceType::DELTA_PRO_3) {
        strncpy(status.name, "Delta Pro 3", 15);
        const DeltaPro3Data& src = dev->getData().deltaPro3;
        DeltaPro3DataStruct& dst = status.data.d3p;

        dst.batteryLevel = src.batteryLevel;
        dst.acInputPower = src.acInputPower;
        dst.acLvOutputPower = src.acLvOutputPower;
        dst.acHvOutputPower = src.acHvOutputPower;
        dst.inputPower = src.inputPower;
        dst.outputPower = src.outputPower;
        dst.dc12vOutputPower = src.dc12vOutputPower;
        dst.dcLvInputPower = src.dcLvInputPower;
        dst.dcHvInputPower = src.dcHvInputPower;
        dst.dcLvInputState = src.dcLvInputState;
        dst.dcHvInputState = src.dcHvInputState;
        dst.usbcOutputPower = src.usbcOutputPower;
        dst.usbc2OutputPower = src.usbc2OutputPower;
        dst.usbaOutputPower = src.usbaOutputPower;
        dst.usba2OutputPower = src.usba2OutputPower;
        dst.acChargingSpeed = src.acChargingSpeed;
        dst.maxAcChargingPower = src.maxAcChargingPower;
        dst.pluggedInAc = src.pluggedInAc;
        dst.energyBackup = src.energyBackup;
        dst.energyBackupBatteryLevel = src.energyBackupBatteryLevel;
        dst.batteryChargeLimitMin = src.batteryChargeLimitMin;
        dst.batteryChargeLimitMax = src.batteryChargeLimitMax;
        dst.cellTemperature = src.cellTemperature;
        dst.dc12vPort = src.dc12vPort;
        dst.acLvPort = src.acLvPort;
        dst.acHvPort = src.acHvPort;
        dst.solarLvPower = src.solarLvPower;
        dst.solarHvPower = src.solarHvPower;
        dst.gfiMode = src.gfiMode;

    } else if (type == DeviceType::ALTERNATOR_CHARGER) {
        strncpy(status.name, "Alt Charger", 15);
        const AlternatorChargerData& src = dev->getData().alternatorCharger;
        AlternatorChargerDataStruct& dst = status.data.ac;

        dst.batteryLevel = src.batteryLevel;
        dst.dcPower = src.dcPower;
        dst.chargerMode = src.chargerMode;
        dst.chargerOpen = src.chargerOpen;
        dst.carBatteryVoltage = src.carBatteryVoltage;
    }

    uint8_t buffer[sizeof(DeviceStatus) + 4];
    int len = pack_device_status_message(buffer, &status);
    Serial1.write(buffer, len);
}

void checkUart() {
    static uint8_t rx_buf[1024];
    static uint16_t rx_idx = 0;
    static uint8_t expected_len = 0;
    static bool collecting = false;

    while (Serial1.available()) {
        uint8_t b = Serial1.read();

        // Debug only - can be verbose
        ESP_LOGI(TAG, "UART: %02X", b);

        if (!collecting) {
            if (b == START_BYTE) {
                // Serial.println("UART RX: Start Byte");
                collecting = true;
                rx_idx = 0;
                rx_buf[rx_idx++] = b;
            }
        } else {
            rx_buf[rx_idx++] = b;

            if (rx_idx == 3) { // We have START, CMD, LEN
                expected_len = rx_buf[2];
                // Sanity check length
                if (expected_len > 250) {
                    collecting = false;
                    rx_idx = 0;
                }
            } else if (rx_idx > 3) {
                 if (rx_idx == (4 + expected_len)) {
                    // Packet complete
                    uint8_t received_crc = rx_buf[rx_idx - 1];
                    uint8_t calculated_crc = calculate_crc8(&rx_buf[1], 2 + expected_len);

                    if (received_crc == calculated_crc) {
                        uint8_t cmd = rx_buf[1];
                        // Serial.printf("UART CMD: 0x%02X\n", cmd);
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
                        } else if (cmd == CMD_SET_WAVE2) {
                                uint8_t type, value;
                                if (unpack_set_wave2_message(rx_buf, &type, &value) == 0) {
                                    EcoflowESP32* w2 = DeviceManager::getInstance().getDevice(DeviceType::WAVE_2);
                                    if (w2 && w2->isAuthenticated()) {
                                        switch(type) {
                                            case W2_PARAM_TEMP: w2->setTemperature(value); break;
                                            case W2_PARAM_MODE: w2->setMainMode(value); break;
                                            case W2_PARAM_SUB_MODE: w2->setSubMode(value); break;
                                            case W2_PARAM_FAN: w2->setFanSpeed(value); break;
                                            case W2_PARAM_POWER: w2->setPowerState(value); break;
                                        }
                                    }
                                }
                        } else if (cmd == CMD_SET_AC) {
                                uint8_t enable;
                                if (unpack_set_ac_message(rx_buf, &enable) == 0) {
                                    EcoflowESP32* dev = DeviceManager::getInstance().getDevice(currentViewDevice);
                                    if (dev && dev->isAuthenticated()) {
                                        dev->setAC(enable ? true : false);
                                    }
                                }
                        } else if (cmd == CMD_SET_DC) {
                                uint8_t enable;
                                if (unpack_set_dc_message(rx_buf, &enable) == 0) {
                                    EcoflowESP32* dev = DeviceManager::getInstance().getDevice(currentViewDevice);
                                    if (dev && dev->isAuthenticated()) {
                                        dev->setDC(enable ? true : false);
                                    }
                                }
                        }
                    } else {
                        ESP_LOGE(TAG, "CRC Fail: Rx %02X != Calc %02X", received_crc, calculated_crc);
                    }
                    collecting = false; // Reset for next packet
                    rx_idx = 0;
                }
            }

            if (rx_idx >= sizeof(rx_buf)) {
                collecting = false; // Overflow protection, reset
                rx_idx = 0;
            }
        }
    }
}

void loop() {
    static uint32_t last_display_update = 0;
    static uint32_t last_data_refresh = 0;
    static uint32_t last_device_list_update = 0;

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

        EcoflowESP32* d3 = DeviceManager::getInstance().getDevice(DeviceType::DELTA_3);
        if (d3 && d3->isAuthenticated()) d3->requestData();
        EcoflowESP32* w2 = DeviceManager::getInstance().getDevice(DeviceType::WAVE_2);
        if (w2 && w2->isAuthenticated()) w2->requestData();
        EcoflowESP32* d3p = DeviceManager::getInstance().getDevice(DeviceType::DELTA_PRO_3);
        if (d3p && d3p->isAuthenticated()) d3p->requestData();
    }

    // Periodically update device list (every 5 seconds) to handle disconnects/reconnects
    if (millis() - last_device_list_update > 5000) {
        last_device_list_update = millis();
        sendDeviceList();
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
