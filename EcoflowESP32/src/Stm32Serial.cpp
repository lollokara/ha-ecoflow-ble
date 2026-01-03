/**
 * @file Stm32Serial.cpp
 * @author Lollokara
 * @brief Implementation of the UART communication layer between ESP32 and STM32F4.
 *
 * This file handles the serialization and deserialization of the custom protocol
 * packets, CRC verification, and dispatching of received commands to the appropriate
 * handlers (DeviceManager, LightSensor, etc.).
 */

#include "Stm32Serial.h"
#include "DeviceManager.h"
#include "LightSensor.h"
#include "EcoflowESP32.h"
#include <WiFi.h>
#include <LittleFS.h>
#include "OtaManager.h"
#include "EcoflowProtocol.h"

#define RX_PIN 18
#define TX_PIN 17
#define POWER_LATCH_PIN 16

static const char* TAG = "Stm32Serial";

Stm32Serial::Stm32Serial() : _serial(&Serial1), _ota(NULL) {
}

void Stm32Serial::begin() {
    _serial->begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
    ESP_LOGI(TAG, "STM32 Serial Initialized");
}

void Stm32Serial::startOta(const String& filename) {
    if (_ota) {
        _ota->startStm32Update(filename.c_str());
    } else {
        ESP_LOGE(TAG, "OTA Manager not initialized!");
    }
}

uint8_t Stm32Serial::crc8(uint8_t *data, int len) {
    uint8_t crc = 0;
    for (int i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) crc = (crc << 1) ^ 0x31; else crc <<= 1;
        }
    }
    return crc;
}

void Stm32Serial::sendPacket(uint8_t cmd, uint8_t* payload, uint8_t len) {
    uint8_t buf[300];
    if (len + 4 > sizeof(buf)) {
        ESP_LOGE(TAG, "Packet too large");
        return;
    }
    buf[0] = 0xAA;
    buf[1] = cmd;
    buf[2] = len;
    if (len > 0 && payload != NULL) {
        memcpy(&buf[3], payload, len);
    }

    buf[3+len] = crc8(&buf[1], len + 2);
    _serial->write(buf, len + 4);
}

void Stm32Serial::handle() {
    static uint8_t rx_buf[1024];
    static uint16_t rx_idx = 0;
    static uint8_t expected_len = 0;
    static bool collecting = false;

    while (_serial->available()) {
        uint8_t b = _serial->read();

        if (!collecting) {
            if (b == 0xAA) {
                collecting = true;
                rx_idx = 0;
                rx_buf[rx_idx++] = b;
            }
        } else {
            rx_buf[rx_idx++] = b;

            if (rx_idx == 3) {
                expected_len = rx_buf[2];
                if (expected_len > 250) {
                    collecting = false;
                    rx_idx = 0;
                }
            } else if (rx_idx > 3) {
                if (rx_idx == (4 + expected_len)) {
                    // Packet Complete
                    uint8_t received_crc = rx_buf[rx_idx - 1];
                    uint8_t calculated_crc = crc8(&rx_buf[1], 2 + expected_len);

                    if (received_crc == calculated_crc) {
                        processPacket(rx_buf, rx_idx);
                    }
                    collecting = false;
                    rx_idx = 0;
                }
            }

            if (rx_idx >= sizeof(rx_buf)) {
                collecting = false;
                rx_idx = 0;
            }
        }
    }
}

void Stm32Serial::processPacket(uint8_t* rx_buf, uint8_t len) {
    uint8_t cmd = rx_buf[1];
    uint8_t* payload = &rx_buf[3];

    if (cmd == CMD_OTA_ACK && _ota) _ota->onAck(lastSentCmd);
    else if (cmd == CMD_OTA_NACK && _ota) _ota->onNack(lastSentCmd);
    else if (cmd == CMD_HANDSHAKE) {
        sendPacket(CMD_HANDSHAKE_ACK, NULL, 0);
        sendDeviceList();
    }
    else if (cmd == CMD_GET_DEVICE_STATUS) {
         uint8_t dev_id = 0;
         if (unpack_get_device_status_message(rx_buf, &dev_id) == 0) {
            sendDeviceStatus(dev_id);
         }
    }
    else if (cmd == CMD_SET_WAVE2) {
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
    }
    else if (cmd == CMD_SET_AC) {
        uint8_t enable;
        if (unpack_set_ac_message(rx_buf, &enable) == 0) {
            EcoflowESP32* d3 = DeviceManager::getInstance().getDevice(DeviceType::DELTA_3);
            if (d3 && d3->isAuthenticated()) d3->setAC(enable);
            EcoflowESP32* d3p = DeviceManager::getInstance().getDevice(DeviceType::DELTA_PRO_3);
            if (d3p && d3p->isAuthenticated()) d3p->setAC(enable);
        }
    }
    else if (cmd == CMD_SET_DC) {
        uint8_t enable;
        if (unpack_set_dc_message(rx_buf, &enable) == 0) {
            EcoflowESP32* d3 = DeviceManager::getInstance().getDevice(DeviceType::DELTA_3);
            if (d3 && d3->isAuthenticated()) d3->setDC(enable);
            EcoflowESP32* d3p = DeviceManager::getInstance().getDevice(DeviceType::DELTA_PRO_3);
            if (d3p && d3p->isAuthenticated()) d3p->setDC(enable);
            EcoflowESP32* alt = DeviceManager::getInstance().getDevice(DeviceType::ALTERNATOR_CHARGER);
            if (alt && alt->isAuthenticated()) alt->setChargerOpen(enable);
        }
    }
    else if (cmd == CMD_SET_VALUE) {
        uint8_t type;
        int value;
        if (unpack_set_value_message(rx_buf, &type, &value) == 0) {
             EcoflowESP32* d3 = DeviceManager::getInstance().getDevice(DeviceType::DELTA_3);
             if (d3 && d3->isAuthenticated()) {
                switch(type) {
                    case SET_VAL_AC_LIMIT: d3->setAcChargingLimit(value); break;
                    case SET_VAL_MAX_SOC: d3->setBatterySOCLimits(value, -1); break;
                    case SET_VAL_MIN_SOC: d3->setBatterySOCLimits(101, value); break;
                }
             }
             EcoflowESP32* d3p = DeviceManager::getInstance().getDevice(DeviceType::DELTA_PRO_3);
             if (d3p && d3p->isAuthenticated()) {
                 switch(type) {
                    case SET_VAL_AC_LIMIT: d3p->setAcChargingLimit(value); break;
                    case SET_VAL_MAX_SOC: d3p->setBatterySOCLimits(value, -1); break;
                    case SET_VAL_MIN_SOC: d3p->setBatterySOCLimits(101, value); break;
                 }
             }
             EcoflowESP32* alt = DeviceManager::getInstance().getDevice(DeviceType::ALTERNATOR_CHARGER);
             if (alt && alt->isAuthenticated()) {
                 switch(type) {
                    case SET_VAL_ALT_START_VOLTAGE: alt->setBatteryVoltage((float)value / 10.0f); break;
                    case SET_VAL_ALT_MODE: alt->setChargerMode(value); break;
                    case SET_VAL_ALT_PROD_LIMIT: alt->setPowerLimit(value); break;
                    case SET_VAL_ALT_REV_LIMIT: alt->setCarBatteryChargeLimit((float)value); break;
                    case SET_VAL_ALT_CHG_LIMIT: alt->setDeviceBatteryChargeLimit((float)value); break;
                 }
             }
        }
    }
    else if (cmd == CMD_POWER_OFF) {
        ESP_LOGI(TAG, "Received Power OFF Command.");
        _serial->end();
        pinMode(POWER_LATCH_PIN, OUTPUT);
        digitalWrite(POWER_LATCH_PIN, HIGH);
        delay(3000);
        ESP.restart();
    }
    else if (cmd == CMD_GET_DEBUG_INFO) {
        DebugInfo info = {0};
        if(WiFi.status() == WL_CONNECTED) strncpy(info.ip, WiFi.localIP().toString().c_str(), 15);
        else strncpy(info.ip, "Disconnected", 15);

        DeviceType types[] = {DeviceType::DELTA_3, DeviceType::WAVE_2, DeviceType::DELTA_PRO_3, DeviceType::ALTERNATOR_CHARGER};
        for(int i=0; i<4; i++) {
             DeviceSlot* s = DeviceManager::getInstance().getSlot(types[i]);
             if(s) {
                 if(s->isConnected) info.devices_connected++;
                 if(!s->macAddress.empty()) info.devices_paired++;
             }
        }
        uint8_t buffer[sizeof(DebugInfo) + 4];
        int len = pack_debug_info_message(buffer, &info);
        _serial->write(buffer, len);
    }
    else if (cmd == CMD_CONNECT_DEVICE) {
        uint8_t type;
        if (unpack_connect_device_message(rx_buf, &type) == 0) {
            DeviceManager::getInstance().scanAndConnect((DeviceType)type);
        }
    }
    else if (cmd == CMD_FORGET_DEVICE) {
        uint8_t type;
        if (unpack_forget_device_message(rx_buf, &type) == 0) {
            DeviceManager::getInstance().forget((DeviceType)type);
        }
    }
}

void Stm32Serial::sendOtaStart(uint32_t size) {
    uint8_t payload[4];
    memcpy(payload, &size, 4);
    sendPacket(0xA0, payload, 4);
    lastSentCmd = 0xA0;
}

void Stm32Serial::sendOtaChunk(uint32_t offset, uint8_t* data, uint8_t len) {
    uint8_t payload[250];
    memcpy(payload, &offset, 4);
    memcpy(&payload[4], data, len);
    sendPacket(0xA1, payload, len + 4);
    lastSentCmd = 0xA1;
}

void Stm32Serial::sendOtaEnd() {
    sendPacket(0xA2, NULL, 0);
    lastSentCmd = 0xA2;
}

void Stm32Serial::sendOtaApply() {
    sendPacket(0xA3, NULL, 0);
    lastSentCmd = 0xA3;
}

void Stm32Serial::sendDeviceList() {
    DeviceList list = {0};
    DeviceSlot* slots[] = {
        DeviceManager::getInstance().getSlot(DeviceType::DELTA_3),
        DeviceManager::getInstance().getSlot(DeviceType::WAVE_2),
        DeviceManager::getInstance().getSlot(DeviceType::DELTA_PRO_3),
        DeviceManager::getInstance().getSlot(DeviceType::ALTERNATOR_CHARGER)
    };

    uint8_t count = 0;
    for (int i = 0; i < 4; i++) {
        if (slots[i]) {
            list.devices[count].id = (uint8_t)slots[i]->type;
            strncpy(list.devices[count].name, slots[i]->name.c_str(), sizeof(list.devices[count].name) - 1);
            list.devices[count].connected = slots[i]->isConnected ? 1 : 0;
            list.devices[count].paired = !slots[i]->macAddress.empty() ? 1 : 0;
            count++;
        }
    }
    list.count = count;

    uint8_t buffer[sizeof(DeviceList) + 4];
    int len = pack_device_list_message(buffer, &list);
    _serial->write(buffer, len);
}

void Stm32Serial::sendDeviceStatus(uint8_t device_id) {
    DeviceType type = (DeviceType)device_id;
    EcoflowESP32* dev = DeviceManager::getInstance().getDevice(type);

    if (!dev || !dev->isAuthenticated()) return;

    DeviceStatus status = {0};
    status.id = device_id;
    status.connected = 1;
    status.brightness = LightSensor::getInstance().getBrightnessPercent();

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
        dst.batteryLevelMain = src.batteryLevelMain;
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
        dst.startVoltage = src.startVoltage;
        dst.powerLimit = src.powerLimit;
        dst.reverseChargingCurrentLimit = src.reverseChargingCurrentLimit;
        dst.chargingCurrentLimit = src.chargingCurrentLimit;
        dst.startVoltageMin = src.startVoltageMin;
        dst.startVoltageMax = src.startVoltageMax;
        dst.powerMax = src.powerMax;
        dst.reverseChargingCurrentMax = src.reverseChargingCurrentMax;
        dst.chargingCurrentMax = src.chargingCurrentMax;
    }

    uint8_t buffer[sizeof(DeviceStatus) + 4];
    int len = pack_device_status_message(buffer, &status);
    _serial->write(buffer, len);
}
