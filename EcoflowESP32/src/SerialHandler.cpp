#include "SerialHandler.h"
#include "EcoflowESP32.h"

static const char* TAG = "SerialHandler";

SerialHandler::SerialHandler()
    : rx_idx(0), expected_len(0), collecting(false), last_device_list_update(0), currentViewDevice(DeviceType::DELTA_3) {}

void SerialHandler::begin() {
    // User reported wiring: F4 PG14 (TX) -> ESP 16, F4 PG9 (RX) -> ESP 17
    // Standard ESP32 Serial1: RX=16, TX=17.
    // If connected directly (TX->TX, RX->RX), we need to swap pins in software.
    // RX Pin = 17 (connected to F4 TX)
    // TX Pin = 16 (connected to F4 RX)
    Serial1.begin(115200, SERIAL_8N1, 16, 17);
}

void SerialHandler::update() {
    checkUart();

    // Periodically update device list (every 5 seconds)
    if (millis() - last_device_list_update > 5000) {
        last_device_list_update = millis();
        sendDeviceList();
    }
}

void SerialHandler::sendDeviceList() {
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

void SerialHandler::sendDeviceStatus(uint8_t device_id) {
    DeviceType type = (DeviceType)device_id;
    EcoflowESP32* dev = DeviceManager::getInstance().getDevice(type);

    // If device is not authenticated, we might still want to send status if it's connected,
    // but usually we wait for auth. For now, strict check.
    if (!dev || !dev->isAuthenticated()) return;

    DeviceStatus status = {0};
    status.id = device_id;
    status.connected = 1;

    // Set Brightness from LightSensor (Global System Property)
    status.brightness = LightSensor::getInstance().getBrightness(); // Returns 0-100

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

void SerialHandler::checkUart() {
    while (Serial1.available()) {
        uint8_t b = Serial1.read();
        if (!collecting) {
            if (b == START_BYTE) {
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
                        handlePacket(rx_buf, rx_idx);
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

void SerialHandler::handlePacket(uint8_t* buffer, uint8_t len) {
    uint8_t cmd = buffer[1];

    if (cmd == CMD_HANDSHAKE) {
        uint8_t ack[4];
        int len = pack_handshake_ack_message(ack);
        Serial1.write(ack, len);
        sendDeviceList();
    } else if (cmd == CMD_GET_DEVICE_STATUS) {
        uint8_t dev_id;
        if (unpack_get_device_status_message(buffer, &dev_id) == 0) {
            // Update current view device implicitly? No, just reply.
            sendDeviceStatus(dev_id);
            // Also update local tracking if needed
            currentViewDevice = (DeviceType)dev_id;
        }
    } else if (cmd == CMD_SET_WAVE2) {
        uint8_t type, value;
        if (unpack_set_wave2_message(buffer, &type, &value) == 0) {
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
        if (unpack_set_ac_message(buffer, &enable) == 0) {
            EcoflowESP32* dev = DeviceManager::getInstance().getDevice(currentViewDevice);
            if (dev && dev->isAuthenticated()) {
                dev->setAC(enable ? true : false);
            }
        }
    } else if (cmd == CMD_SET_DC) {
        uint8_t enable;
        if (unpack_set_dc_message(buffer, &enable) == 0) {
            EcoflowESP32* dev = DeviceManager::getInstance().getDevice(currentViewDevice);
            if (dev && dev->isAuthenticated()) {
                dev->setDC(enable ? true : false);
            }
        }
    } else if (cmd == CMD_SET_VALUE) {
        uint8_t type;
        int value;
        if (unpack_set_value_message(buffer, &type, &value) == 0) {
            EcoflowESP32* dev = DeviceManager::getInstance().getDevice(currentViewDevice);
            if (dev && dev->isAuthenticated()) {
                switch(type) {
                    case SET_VAL_AC_LIMIT: dev->setAcChargingLimit(value); break;
                    case SET_VAL_MAX_SOC: dev->setBatterySOCLimits(value, -1); break;
                    case SET_VAL_MIN_SOC: dev->setBatterySOCLimits(101, value); break;
                }
            }
        }
    }
}
