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
#include <esp_task_wdt.h>
#include "ecoflow_protocol.h"

// Hardware Serial pin definition
#define RX_PIN 18
#define TX_PIN 17

#define POWER_LATCH_PIN 39

static const char* TAG = "Stm32Serial";

// Global flag to indicate OTA handshake success
static volatile bool ota_ack_received = false;

void Stm32Serial::begin() {
    Serial1.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
    _txMutex = xSemaphoreCreateMutex();
}

void Stm32Serial::update() {
    while (Serial1.available()) {
        uint8_t b = Serial1.read();
        if (!_collecting) {
            if (b == PROTOCOL_START_BYTE) {
                _collecting = true;
                _rx_idx = 0;
                _rx_buf[_rx_idx++] = b;
            }
        } else {
            _rx_buf[_rx_idx++] = b;

            if (_rx_idx == 3) {
                _expected_len = _rx_buf[2];
                if (_expected_len > 250) {
                    _collecting = false;
                    _rx_idx = 0;
                }
            } else if (_rx_idx > 3) {
                 if (_rx_idx == (4 + _expected_len)) {
                    uint8_t received_crc = _rx_buf[_rx_idx - 1];

                    uint8_t calculated_crc = 0;
                    auto update_crc = [&](uint8_t val) {
                        uint8_t crc = calculated_crc;
                        uint8_t extract = val;
                        for (uint8_t tempI = 8; tempI; tempI--) {
                            uint8_t sum = (crc ^ extract) & 0x01;
                            crc >>= 1;
                            if (sum) {
                                crc ^= 0x8C;
                            }
                            extract >>= 1;
                        }
                        calculated_crc = crc;
                    };

                    update_crc(_rx_buf[1]);
                    update_crc(_rx_buf[2]);
                    for(int i=0; i<_expected_len; i++) update_crc(_rx_buf[3+i]);

                    if (received_crc == calculated_crc) {
                        processPacket(_rx_buf, _rx_idx);
                    } else {
                        ESP_LOGE(TAG, "CRC Fail: Rx %02X != Calc %02X", received_crc, calculated_crc);
                    }
                    _collecting = false;
                    _rx_idx = 0;
                }
            }

            if (_rx_idx >= sizeof(_rx_buf)) {
                _collecting = false;
                _rx_idx = 0;
            }
        }
    }
}

void Stm32Serial::sendPacket(uint8_t cmd, const uint8_t* payload, size_t len) {
    uint8_t header[3];
    header[0] = PROTOCOL_START_BYTE;
    header[1] = cmd;
    header[2] = (uint8_t)len;

    uint8_t crc = 0;
    auto update_crc = [&](uint8_t b) {
        uint8_t extract = b;
        for (uint8_t tempI = 8; tempI; tempI--) {
            uint8_t sum = (crc ^ extract) & 0x01;
            crc >>= 1;
            if (sum) {
                crc ^= 0x8C;
            }
            extract >>= 1;
        }
    };

    update_crc(cmd);
    update_crc((uint8_t)len);
    for(size_t i=0; i<len; i++) {
        update_crc(payload[i]);
    }

    if (_txMutex) xSemaphoreTake(_txMutex, portMAX_DELAY);
    Serial1.write(header, 3);
    if(len > 0) Serial1.write(payload, len);
    Serial1.write(crc);
    if (_txMutex) xSemaphoreGive(_txMutex);
}

void Stm32Serial::processPacket(uint8_t* rx_buf, uint8_t len) {
    uint8_t cmd = rx_buf[1];

    if (cmd == PROTOCOL_CMD_HANDSHAKE) {
        sendPacket(PROTOCOL_CMD_HANDSHAKE_ACK, NULL, 0);
        sendDeviceList();
    } else if (cmd == PROTOCOL_CMD_OTA_ACK) {
        ota_ack_received = true;
        ESP_LOGI(TAG, "OTA ACK Received");
    } else if (cmd == PROTOCOL_CMD_OTA_NACK) {
        ota_ack_received = false;
        ESP_LOGE(TAG, "OTA NACK Received");
    } else if (cmd == PROTOCOL_CMD_GET_DEVICE_STATUS) {
        uint8_t dev_id = rx_buf[3];
        sendDeviceStatus(dev_id);
    } else if (cmd == PROTOCOL_CMD_SET_WAVE2) {
        uint8_t type = rx_buf[3];
        uint8_t value = rx_buf[4];
        EcoflowESP32* w2 = DeviceManager::getInstance().getDevice(DeviceType::WAVE_2);
        if (w2 && w2->isAuthenticated()) {
            switch(type) {
                case 1: w2->setTemperature(value); break;
                case 2: w2->setMainMode(value); break;
                case 3: w2->setSubMode(value); break;
                case 4: w2->setFanSpeed(value); break;
                case 5: w2->setPowerState(value); break;
            }
        }
    } else if (cmd == PROTOCOL_CMD_SET_AC) {
        uint8_t enable = rx_buf[3];
        EcoflowESP32* d3 = DeviceManager::getInstance().getDevice(DeviceType::DELTA_3);
        if (d3 && d3->isAuthenticated()) d3->setAC(enable);
        EcoflowESP32* d3p = DeviceManager::getInstance().getDevice(DeviceType::DELTA_PRO_3);
        if (d3p && d3p->isAuthenticated()) d3p->setAC(enable);
    } else if (cmd == PROTOCOL_CMD_SET_DC) {
        uint8_t enable = rx_buf[3];
        EcoflowESP32* d3 = DeviceManager::getInstance().getDevice(DeviceType::DELTA_3);
        if (d3 && d3->isAuthenticated()) d3->setDC(enable);
        EcoflowESP32* d3p = DeviceManager::getInstance().getDevice(DeviceType::DELTA_PRO_3);
        if (d3p && d3p->isAuthenticated()) d3p->setDC(enable);
        EcoflowESP32* alt = DeviceManager::getInstance().getDevice(DeviceType::ALTERNATOR_CHARGER);
        if (alt && alt->isAuthenticated()) alt->setChargerOpen(enable);
    } else if (cmd == PROTOCOL_CMD_SET_VALUE) {
        uint8_t type = rx_buf[3];
        int value = *(int*)&rx_buf[4];
        EcoflowESP32* d3 = DeviceManager::getInstance().getDevice(DeviceType::DELTA_3);
        if (d3 && d3->isAuthenticated()) {
            if(type == 1) d3->setAcChargingLimit(value);
            else if(type == 2) d3->setBatterySOCLimits(value, -1);
            else if(type == 3) d3->setBatterySOCLimits(101, value);
        }
        EcoflowESP32* d3p = DeviceManager::getInstance().getDevice(DeviceType::DELTA_PRO_3);
        if (d3p && d3p->isAuthenticated()) {
            if(type == 1) d3p->setAcChargingLimit(value);
            else if(type == 2) d3p->setBatterySOCLimits(value, -1);
            else if(type == 3) d3p->setBatterySOCLimits(101, value);
        }
        EcoflowESP32* alt = DeviceManager::getInstance().getDevice(DeviceType::ALTERNATOR_CHARGER);
        if (alt && alt->isAuthenticated()) {
            if(type == 10) alt->setBatteryVoltage((float)value / 10.0f);
            else if(type == 11) alt->setChargerMode(value);
            else if(type == 12) alt->setPowerLimit(value);
            else if(type == 13) alt->setCarBatteryChargeLimit((float)value);
            else if(type == 14) alt->setDeviceBatteryChargeLimit((float)value);
        }
    } else if (cmd == PROTOCOL_CMD_POWER_OFF) {
        Serial1.end();
        pinMode(POWER_LATCH_PIN, OUTPUT);
        digitalWrite(POWER_LATCH_PIN, HIGH);
        delay(3000);
        ESP.restart();
    } else if (cmd == PROTOCOL_CMD_CONNECT_DEVICE) {
        uint8_t type = rx_buf[3];
        DeviceManager::getInstance().scanAndConnect((DeviceType)type);
    } else if (cmd == PROTOCOL_CMD_FORGET_DEVICE) {
        uint8_t type = rx_buf[3];
        DeviceManager::getInstance().forget((DeviceType)type);
    } else if (cmd == PROTOCOL_CMD_GET_DEBUG_INFO) {
        DebugInfo info = {0};
        if(WiFi.status() == WL_CONNECTED) {
            strncpy(info.ip, WiFi.localIP().toString().c_str(), 15);
        } else {
             strncpy(info.ip, "Disconnected", 15);
        }
        uint8_t buffer[sizeof(DebugInfo) + 4];
        int len = pack_debug_info_message(buffer, &info);
        Serial1.write(buffer, len);
    }
}

void Stm32Serial::sendDeviceList() {
    DeviceListMessage msg;
    memset(&msg, 0, sizeof(msg));

    msg.count = 0;
    for (int i = 0; i < 4; i++) {
        DeviceSlot* s = DeviceManager::getInstance().getSlotByIndex(i);
        if (s && !s->macAddress.empty()) {
            msg.devices[msg.count].slot = i;
            msg.devices[msg.count].type = (uint8_t)s->type;
            strncpy(msg.devices[msg.count].sn, s->serialNumber.c_str(), 15);
            msg.devices[msg.count].is_connected = s->isConnected ? 1 : 0;
            msg.devices[msg.count].paired = 1;
            if (s->instance) {
                msg.devices[msg.count].battery_level = (uint8_t)s->instance->getBatteryLevel();
            }
            msg.count++;
        }
    }

    uint8_t payload[sizeof(DeviceListMessage)];
    pack_device_list_message(&msg, payload);
    sendPacket(PROTOCOL_CMD_DEVICE_LIST, payload, sizeof(payload));
}

void Stm32Serial::sendDeviceStatus(uint8_t device_id) {
    DeviceType type = (DeviceType)device_id;
    EcoflowESP32* dev = DeviceManager::getInstance().getDevice(type);

    if (!dev || !dev->isAuthenticated()) return;

    DeviceStatusMessage status;
    memset(&status, 0, sizeof(status));

    status.slot = 0;
    status.type = (uint8_t)type;
    status.battery_level = (uint8_t)dev->getBatteryLevel();
    status.brightness = LightSensor::getInstance().getBrightnessPercent();

    if (type == DeviceType::DELTA_3) {
        const Delta3Data& src = dev->getData().delta3;
        Delta3DataStruct& dst = status.data.delta3;
        dst.totalInputPower = src.inputPower;
        dst.totalOutputPower = src.outputPower;
        dst.acSwitch = src.acOn;
        dst.dcSwitch = src.dcOn;
        dst.usbSwitch = src.usbOn;
        dst.acChgLimit = src.acChgLimit;
        dst.maxChgSoc = src.maxChgSoc;
        dst.minDsgSoc = src.minDsgSoc;
    } else if (type == DeviceType::WAVE_2) {
        const Wave2Data& src = dev->getData().wave2;
        Wave2DataStruct& dst = status.data.wave2;
        dst.setTemp = src.setTemp;
        dst.envTemp = src.envTemp;
        dst.mode = src.mode;
    } else if (type == DeviceType::DELTA_PRO_3) {
        const DeltaPro3Data& src = dev->getData().deltaPro3;
        DeltaPro3DataStruct& dst = status.data.deltaPro3;
        dst.acInputPower = src.acInputPower;
        dst.batteryLevel = src.batteryLevel;
    } else if (type == DeviceType::ALTERNATOR_CHARGER) {
        const AlternatorChargerData& src = dev->getData().alternatorCharger;
        AlternatorChargerDataStruct& dst = status.data.alternatorCharger;
        dst.carBatteryVoltage = src.carBatteryVoltage;
    }

    uint8_t buffer[sizeof(DeviceStatusMessage)];
    pack_device_status_message(&status, buffer);
    sendPacket(PROTOCOL_CMD_DEVICE_STATUS, buffer, sizeof(buffer));
}

// --- OTA Implementation ---

void Stm32Serial::startOTA(size_t totalSize) {
    uint32_t size = (uint32_t)totalSize;
    ESP_LOGI(TAG, "Sending OTA Start: %u bytes", size);

    ota_ack_received = false;
    _ota_error = false;
    sendPacket(PROTOCOL_CMD_OTA_START, (uint8_t*)&size, sizeof(size));

    // Wait for ACK (Erase complete) with 20s timeout
    uint32_t start = millis();
    while(!ota_ack_received && (millis() - start < 20000)) {
        // We do NOT call update() here anymore to prevent race condition.
        // The main loop() calls update(), which sets ota_ack_received.
        delay(20);
        esp_task_wdt_reset();
    }

    if (ota_ack_received) {
        ESP_LOGI(TAG, "OTA ACK Received. Starting Transfer.");
    } else {
        ESP_LOGE(TAG, "OTA ACK Timeout or NACK. Aborting.");
        _ota_error = true;
    }
}

void Stm32Serial::sendOtaChunk(uint8_t* data, size_t len, size_t index) {
    if (_ota_error) return;

    size_t pos = 0;
    while(pos < len) {
        size_t chunkSize = len - pos;
        if(chunkSize > 230) chunkSize = 230; // Safe margin

        size_t currentOffset = index + pos;

        uint8_t payload[240];
        payload[0] = (currentOffset >> 0) & 0xFF;
        payload[1] = (currentOffset >> 8) & 0xFF;
        payload[2] = (currentOffset >> 16) & 0xFF;
        payload[3] = (currentOffset >> 24) & 0xFF;

        memcpy(&payload[4], data + pos, chunkSize);

        sendPacket(PROTOCOL_CMD_OTA_CHUNK, payload, chunkSize + 4);
        pos += chunkSize;
    }
}

void Stm32Serial::endOTA() {
    if (_ota_error) return;
    ESP_LOGI(TAG, "Sending OTA End");
    uint8_t dummy = 0;
    sendPacket(PROTOCOL_CMD_OTA_END, &dummy, 1);
}
