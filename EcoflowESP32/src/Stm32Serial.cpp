/**
 * @file Stm32Serial.cpp
 * @author Lollokara
 * @brief Implementation of the UART communication layer between ESP32 and STM32F4.
 *
 * This file handles the serialization and deserialization of the custom protocol
 * packets, CRC verification, and dispatching of received commands to the appropriate
 * handlers (DeviceManager, LightSensor, etc.). It also manages the OTA update process.
 */

#include "Stm32Serial.h"
#include "DeviceManager.h"
#include "LightSensor.h"
#include "EcoflowESP32.h"
#include <WiFi.h>
#include <LittleFS.h>
#include <esp_log.h>

// Hardware Serial pin definition (moved from main.cpp)
// RX Pin = 18 (connected to F4 TX)
// TX Pin = 17 (connected to F4 RX)
#define RX_PIN 18
#define TX_PIN 17

#define POWER_LATCH_PIN 39

// OTA Commands
#define CMD_OTA_START       0xF0
#define CMD_OTA_CHUNK       0xF1
#define CMD_OTA_END         0xF2
#define CMD_ACK             0x21
#define CMD_NACK            0x22

#define OTA_CHUNK_SIZE      240
#define OTA_TIMEOUT_MS      5000  // Timeout for ACK
#define OTA_ERASE_TIMEOUT_MS 15000 // Extended timeout for Start/Erase

static const char* TAG = "Stm32Serial";

/**
 * @brief Initializes the UART interface.
 *
 * Configures Serial1 with 115200 baud, 8N1, on defined pins.
 */
void Stm32Serial::begin() {
    Serial1.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
}

/**
 * @brief Main update loop for processing incoming UART data.
 *
 * Reads bytes from the serial buffer, assembles packets based on the protocol
 * (START_BYTE, Header, Length, Payload, CRC), and calls processPacket() when
 * a valid packet is received. Also runs the OTA task.
 */
void Stm32Serial::update() {
    // Run OTA Task
    otaTask();

    static uint8_t rx_buf[1024];
    static uint16_t rx_idx = 0;
    static uint8_t expected_len = 0;
    static bool collecting = false;

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
                        processPacket(rx_buf, rx_idx);
                    } else {
                        ESP_LOGE(TAG, "CRC Fail: Rx %02X != Calc %02X", received_crc, calculated_crc);
                    }
                    collecting = false; // Reset for next packet
                    rx_idx = 0;
                }
            }

            if (rx_idx >= sizeof(rx_buf)) {
                collecting = false; // Overflow protection
                rx_idx = 0;
            }
        }
    }
}

/**
 * @brief Starts an OTA update for the STM32 using the provided file.
 */
bool Stm32Serial::startOta(const String& path) {
    if (_otaState != OTA_IDLE && _otaState != OTA_ERROR && _otaState != OTA_DONE) {
        ESP_LOGE(TAG, "OTA already in progress");
        return false;
    }

    if (!LittleFS.exists(path)) {
        ESP_LOGE(TAG, "Firmware file not found: %s", path.c_str());
        return false;
    }

    _otaFile = LittleFS.open(path, "r");
    if (!_otaFile) {
        ESP_LOGE(TAG, "Failed to open firmware file");
        return false;
    }

    _otaTotalSize = _otaFile.size();
    _otaOffset = 0;
    _otaRetries = 0;
    _otaState = OTA_STARTING;
    _ackReceived = false;
    _nackReceived = false;
    _otaLastMsgTime = millis();

    ESP_LOGI(TAG, "Starting STM32 OTA. File: %s, Size: %u bytes", path.c_str(), _otaTotalSize);
    return true;
}

/**
 * @brief Manages the OTA state machine.
 */
void Stm32Serial::otaTask() {
    if (_otaState == OTA_IDLE || _otaState == OTA_DONE || _otaState == OTA_ERROR) return;

    // Timeout Check
    uint32_t timeout = (_otaState == OTA_WAITING_ACK && _otaPrevState == OTA_STARTING) ? OTA_ERASE_TIMEOUT_MS : OTA_TIMEOUT_MS;

    if (millis() - _otaLastMsgTime > timeout) {
        if (_otaState == OTA_WAITING_ACK) {
            ESP_LOGW(TAG, "OTA ACK Timeout. Retrying...");
            _otaRetries++;
            if (_otaRetries > 3) {
                ESP_LOGE(TAG, "OTA Failed: Too many timeouts");
                _otaState = OTA_ERROR;
                if (_otaFile) _otaFile.close();
            } else {
                // Go back to previous state to resend
                _otaState = _otaPrevState;
                _otaLastMsgTime = millis(); // Reset timer to allow resend immediately
            }
        }
    }

    switch (_otaState) {
        case OTA_STARTING: {
            uint8_t buffer[4];
            // [AA][CMD][LEN][CRC]
            // We just send CMD_OTA_START. Payload len 0.
            buffer[0] = START_BYTE;
            buffer[1] = CMD_OTA_START;
            buffer[2] = 0x00;
            buffer[3] = calculate_crc8(&buffer[1], 2);
            Serial1.write(buffer, 4);

            ESP_LOGI(TAG, "Sending OTA Start...");
            _otaPrevState = OTA_STARTING;
            _otaState = OTA_WAITING_ACK;
            _ackReceived = false;
            _otaLastMsgTime = millis();
            break;
        }

        case OTA_SENDING: {
            if (_otaOffset >= _otaTotalSize) {
                _otaState = OTA_ENDING;
                return;
            }

            // Read Chunk
            _otaFile.seek(_otaOffset);
            uint8_t chunkBuf[OTA_CHUNK_SIZE];
            int readBytes = _otaFile.read(chunkBuf, OTA_CHUNK_SIZE);

            if (readBytes > 0) {
                // Packet: [AA][CMD][LEN][OFFSET(4)][DATA...][CRC]
                // Total length: 1(Start) + 1(Cmd) + 1(Len) + 4(Offset) + Data + 1(CRC)
                // Payload length in protocol is LEN byte = 4 + Data
                // Max LEN is 250. 4 + 240 = 244. Fits.

                uint8_t header[7];
                header[0] = START_BYTE;
                header[1] = CMD_OTA_CHUNK;
                header[2] = (uint8_t)(4 + readBytes);
                memcpy(&header[3], &_otaOffset, 4);

                // Send Header
                Serial1.write(header, 7); // Start, Cmd, Len, Offset
                // Send Data
                Serial1.write(chunkBuf, readBytes);

                // Calculate CRC over [Cmd, Len, Offset, Data]
                // We need a temp buffer for CRC calc or incremental calc.
                // Reconstruct full buffer for CRC
                uint8_t crcBuf[255];
                crcBuf[0] = CMD_OTA_CHUNK;
                crcBuf[1] = (uint8_t)(4 + readBytes);
                memcpy(&crcBuf[2], &_otaOffset, 4);
                memcpy(&crcBuf[6], chunkBuf, readBytes);

                uint8_t crc = calculate_crc8(crcBuf, 2 + 4 + readBytes);
                Serial1.write(crc);

                if (_otaOffset % (OTA_CHUNK_SIZE * 10) == 0) {
                     ESP_LOGI(TAG, "OTA Progress: %u / %u bytes (%.1f%%)", _otaOffset, _otaTotalSize, (float)_otaOffset * 100.0 / _otaTotalSize);
                }

                _otaPrevState = OTA_SENDING;
                _otaState = OTA_WAITING_ACK;
                _ackReceived = false;
                _otaLastMsgTime = millis();
            } else {
                _otaState = OTA_ENDING;
            }
            break;
        }

        case OTA_WAITING_ACK: {
            if (_ackReceived) {
                _ackReceived = false;
                _otaRetries = 0;
                if (_otaPrevState == OTA_STARTING) {
                    _otaState = OTA_SENDING;
                    ESP_LOGI(TAG, "OTA Start ACK received. Sending firmware...");
                } else if (_otaPrevState == OTA_SENDING) {
                    _otaOffset += OTA_CHUNK_SIZE; // Should ideally use actual bytes sent
                    // If we are at end, next loop handles it
                    _otaState = OTA_SENDING;
                } else if (_otaPrevState == OTA_ENDING) {
                    ESP_LOGI(TAG, "OTA Complete. Rebooting STM32...");
                    _otaState = OTA_DONE;
                    if (_otaFile) _otaFile.close();
                }
            } else if (_nackReceived) {
                ESP_LOGE(TAG, "OTA NACK received. Retrying...");
                _nackReceived = false;
                _otaState = _otaPrevState;
                _otaRetries++;
                _otaLastMsgTime = millis(); // Reset timeout
            }
            break;
        }

        case OTA_ENDING: {
            uint8_t buffer[4];
            buffer[0] = START_BYTE;
            buffer[1] = CMD_OTA_END;
            buffer[2] = 0x00;
            buffer[3] = calculate_crc8(&buffer[1], 2);
            Serial1.write(buffer, 4);

            ESP_LOGI(TAG, "Sending OTA End...");
            _otaPrevState = OTA_ENDING;
            _otaState = OTA_WAITING_ACK;
            _ackReceived = false;
            _otaLastMsgTime = millis();
            break;
        }

        default: break;
    }
}

/**
 * @brief Dispatches valid packets to their specific handlers.
 *
 * @param rx_buf The buffer containing the full packet.
 * @param len The total length of the packet.
 */
void Stm32Serial::processPacket(uint8_t* rx_buf, uint8_t len) {
    uint8_t cmd = rx_buf[1];

    // Handle OTA ACKs
    if (_otaState != OTA_IDLE && _otaState != OTA_DONE && _otaState != OTA_ERROR) {
        if (cmd == CMD_ACK) {
            _ackReceived = true;
            return;
        } else if (cmd == CMD_NACK) {
            _nackReceived = true;
            return;
        }
    }

    if (cmd == CMD_HANDSHAKE) {
        // Reply with ACK and then send the device list
        uint8_t ack[4];
        int l = pack_handshake_ack_message(ack);
        Serial1.write(ack, l);
        sendDeviceList();
    } else if (cmd == CMD_GET_DEVICE_STATUS) {
        // STM32 is requesting status for a specific device
        uint8_t dev_id;
        if (unpack_get_device_status_message(rx_buf, &dev_id) == 0) {
            sendDeviceStatus(dev_id);
        }
    } else if (cmd == CMD_SET_WAVE2) {
        // Handle Wave 2 control commands (Temp, Mode, Fan, Power)
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
        // Toggle AC Power
        uint8_t enable;
        if (unpack_set_ac_message(rx_buf, &enable) == 0) {
            EcoflowESP32* d3 = DeviceManager::getInstance().getDevice(DeviceType::DELTA_3);
            if (d3 && d3->isAuthenticated()) d3->setAC(enable);

            EcoflowESP32* d3p = DeviceManager::getInstance().getDevice(DeviceType::DELTA_PRO_3);
            if (d3p && d3p->isAuthenticated()) d3p->setAC(enable);
        }
    } else if (cmd == CMD_SET_DC) {
        // Toggle DC Power
        uint8_t enable;
        if (unpack_set_dc_message(rx_buf, &enable) == 0) {
            EcoflowESP32* d3 = DeviceManager::getInstance().getDevice(DeviceType::DELTA_3);
            if (d3 && d3->isAuthenticated()) d3->setDC(enable);

            EcoflowESP32* d3p = DeviceManager::getInstance().getDevice(DeviceType::DELTA_PRO_3);
            if (d3p && d3p->isAuthenticated()) d3p->setDC(enable);

            EcoflowESP32* alt = DeviceManager::getInstance().getDevice(DeviceType::ALTERNATOR_CHARGER);
            if (alt && alt->isAuthenticated()) alt->setChargerOpen(enable);
        }
    } else if (cmd == CMD_SET_VALUE) {
        // Handle generic value setting (Charge limits, SOC limits)
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
    } else if (cmd == CMD_POWER_OFF) {
        // Execute power-off sequence
        ESP_LOGI(TAG, "Received Power OFF Command. Shutting down...");
        Serial1.end();
        pinMode(POWER_LATCH_PIN, OUTPUT);
        digitalWrite(POWER_LATCH_PIN, HIGH); // Assumes HIGH triggers shutdown
        delay(3000);
        ESP.restart();
    } else if (cmd == CMD_GET_DEBUG_INFO) {
        // Reply with system debug info
        DebugInfo info = {0};

        // WiFi IP
        if(WiFi.status() == WL_CONNECTED) {
            strncpy(info.ip, WiFi.localIP().toString().c_str(), 15);
        } else {
             strncpy(info.ip, "Disconnected", 15);
        }

        // Device Counts
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
        Serial1.write(buffer, len);
    } else if (cmd == CMD_CONNECT_DEVICE) {
        // Initiate BLE scan/connection for a device type
        uint8_t type;
        if (unpack_connect_device_message(rx_buf, &type) == 0) {
            DeviceManager::getInstance().scanAndConnect((DeviceType)type);
        }
    } else if (cmd == CMD_FORGET_DEVICE) {
        // Forget a bonded device
        uint8_t type;
        if (unpack_forget_device_message(rx_buf, &type) == 0) {
            DeviceManager::getInstance().forget((DeviceType)type);
        }
    }
}

/**
 * @brief Constructs and sends the Device List packet.
 *
 * Iterates through all device slots in DeviceManager and populates the
 * DeviceList structure with ID, Name, Connection Status, and Pairing Status.
 */
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
    Serial1.write(buffer, len);
}

/**
 * @brief Sends the detailed status for a specific device.
 *
 * Maps the internal device data structures (Delta3Data, Wave2Data, etc.)
 * to the protocol-defined DeviceStatus structure and transmits it via UART.
 *
 * @param device_id The ID of the device to report.
 */
void Stm32Serial::sendDeviceStatus(uint8_t device_id) {
    DeviceType type = (DeviceType)device_id;
    EcoflowESP32* dev = DeviceManager::getInstance().getDevice(type);

    if (!dev || !dev->isAuthenticated()) return;

    DeviceStatus status = {0};
    status.id = device_id;
    status.connected = 1;

    // Set Brightness based on ambient light sensor
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
    Serial1.write(buffer, len);
}
