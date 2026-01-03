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

// Hardware Serial pin definition (moved from main.cpp)
// RX Pin = 18 (connected to F4 TX)
// TX Pin = 17 (connected to F4 RX)
#define RX_PIN 18
#define TX_PIN 17

#define POWER_LATCH_PIN 39

static const char* TAG = "Stm32Serial";

// Variables for OTA (from WebServer.cpp)
extern int ota_state;
extern int ota_progress;
extern String ota_msg;

// Variables for OTA
static volatile bool otaAckReceived = false;
static volatile bool otaNackReceived = false;

/**
 * @brief Initializes the UART interface.
 *
 * Configures Serial1 with 115200 baud, 8N1, on defined pins.
 */
void Stm32Serial::begin() {
    Serial1.begin(115200, SERIAL_8N1, RX_PIN, TX_PIN);
    if (_txMutex == NULL) {
        _txMutex = xSemaphoreCreateMutex();
    }
}

void Stm32Serial::sendData(const uint8_t* data, size_t len) {
    if (_txMutex != NULL) {
        if (xSemaphoreTake(_txMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            Serial1.write(data, len);
            xSemaphoreGive(_txMutex);
        } else {
            ESP_LOGE(TAG, "Failed to take TX mutex");
        }
    } else {
        Serial1.write(data, len);
    }
}

/**
 * @brief Main update loop for processing incoming UART data.
 *
 * Reads bytes from the serial buffer, assembles packets based on the protocol
 * (START_BYTE, Header, Length, Payload, CRC), and calls processPacket() when
 * a valid packet is received.
 */
void Stm32Serial::update() {
    // If OTA is running, we might still receive packets (ACK/NACK)
    // The OTA task needs to check flags set here.

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
 * @brief Dispatches valid packets to their specific handlers.
 *
 * @param rx_buf The buffer containing the full packet.
 * @param len The total length of the packet.
 */
void Stm32Serial::processPacket(uint8_t* rx_buf, uint8_t len) {
    uint8_t cmd = rx_buf[1];

    if (cmd == CMD_HANDSHAKE) {
        // Reply with ACK and then send the device list
        uint8_t ack[4];
        int l = pack_handshake_ack_message(ack);
        sendData(ack, l);
        sendDeviceList();
    } else if (cmd == CMD_OTA_ACK) {
        otaAckReceived = true;
    } else if (cmd == CMD_OTA_NACK) {
        otaNackReceived = true;
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
        sendData(buffer, len);
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
    sendData(buffer, len);
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
    sendData(buffer, len);
}

static String otaFilename;

void Stm32Serial::startOta(const String& filename) {
    if (_otaRunning) return;
    otaFilename = filename;
    _otaRunning = true;
    xTaskCreate(otaTask, "OtaTask", 4096, this, 1, NULL);
}

void Stm32Serial::otaTask(void* parameter) {
    Stm32Serial* self = (Stm32Serial*)parameter;

    // 1. Open File
    File f = LittleFS.open(otaFilename, "r");
    if (!f) {
        ESP_LOGE(TAG, "Failed to open firmware file");
        ota_state = 4; ota_msg = "FS Error";
        self->_otaRunning = false;
        vTaskDelete(NULL);
        return;
    }

    uint32_t totalSize = f.size();
    ESP_LOGI(TAG, "Starting OTA. Size: %d", totalSize);

    // 2. Send Start Command
    // Try sending Start Command up to 3 times
    bool startSuccess = false;
    uint8_t buf[256];
    int len = pack_ota_start_message(buf, totalSize);

    for(int attempt=0; attempt<3; attempt++) {
        ESP_LOGI(TAG, "Sending OTA Start (Attempt %d)", attempt+1);
        self->sendData(buf, len);

        // Wait for ACK (longer timeout for erase: 30s)
        otaAckReceived = false;
        otaNackReceived = false;
        uint32_t startWait = millis();
        // Mass erase or large sector erase can take time.
        // Recovery OTA erases 10 sectors (Bank 1).
        while(!otaAckReceived && !otaNackReceived && (millis() - startWait < 30000)) {
            vTaskDelay(100);
        }

        if (otaAckReceived) {
            startSuccess = true;
            // Give STM32 time to loop back to RX state after sending ACK
            vTaskDelay(pdMS_TO_TICKS(100));
            break;
        }

        if (otaNackReceived) {
            ESP_LOGE(TAG, "OTA Start NACK received");
            // If NACK, maybe it's busy or invalid state. Wait before retry.
            vTaskDelay(1000);
        }
    }

    if (!startSuccess) {
        ESP_LOGE(TAG, "OTA Start Failed (Timeout/NACK)");
        ota_state = 4; ota_msg = "Start Timeout";
        f.close();
        self->_otaRunning = false;
        vTaskDelete(NULL);
        return;
    }

    // 3. Send Chunks
    uint32_t offset = 0;
    uint8_t chunk[200]; // Keep under 255 payload limit

    while (f.available()) {
        int bytesRead = f.read(chunk, sizeof(chunk));

        len = pack_ota_chunk_message(buf, offset, chunk, bytesRead);
        self->sendData(buf, len);

        // Wait for ACK
        otaAckReceived = false;
        otaNackReceived = false;
        uint32_t startWait = millis();
        while(!otaAckReceived && !otaNackReceived && (millis() - startWait < 2000)) { // 2s timeout per chunk
            vTaskDelay(5);
        }

        if (otaNackReceived) {
             ESP_LOGE(TAG, "OTA Chunk NACK at %d", offset);
             ota_state = 4; ota_msg = "Chunk NACK";
             break;
        }

        if (!otaAckReceived) {
            ESP_LOGE(TAG, "OTA Chunk Timeout at %d", offset);
            ota_state = 4; ota_msg = "Chunk Timeout";
            break;
        }

        offset += bytesRead;
        int new_progress = (offset * 100) / totalSize;
        if (new_progress > ota_progress + 9) { // Log every ~10%
             ESP_LOGI(TAG, "OTA Progress: %d%% (%d/%d bytes)", new_progress, offset, totalSize);
        }
        ota_progress = new_progress;
    }

    f.close();

    if (offset == totalSize) {
        ESP_LOGI(TAG, "OTA Upload Complete. Sending End...");
        len = pack_ota_end_message(buf);
        self->sendData(buf, len);
        vTaskDelay(500);

        ESP_LOGI(TAG, "Sending Apply...");
        len = pack_ota_apply_message(buf);
        self->sendData(buf, len);
        ota_state = 3; ota_msg = "STM32 Rebooting...";
    } else {
        if (ota_state != 4) { ota_state = 4; ota_msg = "Incomplete"; }
    }

    self->_otaRunning = false;
    vTaskDelete(NULL);
}
