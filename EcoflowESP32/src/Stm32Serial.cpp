/**
 * @file Stm32Serial.cpp
 * @author Lollokara
 * @brief Implementation of the UART communication layer between ESP32 and STM32F4.
 */

#include "Stm32Serial.h"
#include "DeviceManager.h"
#include "LightSensor.h"
#include "EcoflowESP32.h"
#include "EcoflowDataParser.h"
#include <WiFi.h>
#include <LittleFS.h>
#include <esp_rom_crc.h>
#include <vector>

// Hardware Serial pin definition
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

// Log Globals
static std::vector<Stm32Serial::LogEntry> _cachedLogList;
static bool _logListReady = false;
static SemaphoreHandle_t _logListMutex = NULL;

static std::vector<uint8_t> _downloadBuffer;
static bool _downloadComplete = false;
static SemaphoreHandle_t _downloadMutex = NULL;

// CRC32 Table
static const uint32_t crc32_table[] = {
    0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,
    0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61, 0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
    0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
    0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3, 0x709f7b7a, 0x745e66cd,
    0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039, 0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5,
    0xbe2b5b58, 0xbaea46ef, 0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
    0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
    0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1, 0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d,
    0x34867077, 0x30476dc0, 0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
    0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca,
    0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde, 0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02,
    0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
    0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc, 0xb6238b25, 0xb2e29692,
    0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6, 0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a,
    0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
    0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34, 0xdc3abded, 0xd8fba05a,
    0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637, 0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
    0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
    0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5, 0x3f9b762c, 0x3b5a6b9b,
    0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff, 0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623,
    0xf12f560e, 0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
    0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
    0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7, 0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b,
    0x9b3660c6, 0x9ff77d71, 0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
    0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c,
    0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8, 0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24,
    0x119b4be9, 0x155a565e, 0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
    0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a, 0x2d15ebe3, 0x29d4f654,
    0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0, 0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c,
    0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
    0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662, 0x933eb0bb, 0x97ffad0c,
    0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668, 0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

static uint32_t calculate_crc32(uint32_t crc, const uint8_t *buf, size_t len) {
    crc = ~crc;
    while (len--) {
        crc = (crc << 8) ^ crc32_table[((crc >> 24) ^ *buf++) & 0xFF];
    }
    return ~crc;
}

void Stm32Serial::begin() {
    Serial1.setRxBufferSize(16384); // Increase buffer for Log List bursts
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

void Stm32Serial::sendEspLog(uint8_t level, const char* tag, const char* msg) {
    if (_txMutex != NULL) {
        if (xSemaphoreTake(_txMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            uint8_t buf[256];
            int len = pack_esp_log_message(buf, level, tag, msg);
            Serial1.write(buf, len);
            xSemaphoreGive(_txMutex);
        }
    }
}

void Stm32Serial::update() {
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

            if (rx_idx == 3) {
                expected_len = rx_buf[2];
                if (expected_len > 250) {
                    collecting = false;
                    rx_idx = 0;
                }
            } else if (rx_idx > 3) {
                 if (rx_idx == (4 + expected_len)) {
                    uint8_t received_crc = rx_buf[rx_idx - 1];
                    uint8_t calculated_crc = calculate_crc8(&rx_buf[1], 2 + expected_len);

                    if (received_crc == calculated_crc) {
                        processPacket(rx_buf, rx_idx);
                    } else {
                        ESP_LOGE(TAG, "CRC Fail: Rx %02X != Calc %02X", received_crc, calculated_crc);
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

    if (cmd == CMD_HANDSHAKE) {
        uint8_t ack[4];
        int l = pack_handshake_ack_message(ack);
        sendData(ack, l);
        sendDeviceList();
    } else if (cmd == CMD_OTA_ACK) {
        otaAckReceived = true;
    } else if (cmd == CMD_OTA_NACK) {
        otaNackReceived = true;
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
            EcoflowESP32* d3 = DeviceManager::getInstance().getDevice(DeviceType::DELTA_3);
            if (d3 && d3->isAuthenticated()) d3->setAC(enable);

            EcoflowESP32* d3p = DeviceManager::getInstance().getDevice(DeviceType::DELTA_PRO_3);
            if (d3p && d3p->isAuthenticated()) d3p->setAC(enable);
        }
    } else if (cmd == CMD_SET_DC) {
        uint8_t enable;
        if (unpack_set_dc_message(rx_buf, &enable) == 0) {
            EcoflowESP32* d3 = DeviceManager::getInstance().getDevice(DeviceType::DELTA_3);
            if (d3 && d3->isAuthenticated()) d3->setDC(enable);

            EcoflowESP32* d3p = DeviceManager::getInstance().getDevice(DeviceType::DELTA_PRO_3);
            if (d3p && d3p->isAuthenticated()) d3p->setDC(enable);
        }
    } else if (cmd == CMD_SET_VALUE) {
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
                    case SET_VAL_ALT_ENABLE: alt->setChargerOpen(value ? true : false); break;
                 }
             }
        }
    } else if (cmd == CMD_POWER_OFF) {
        ESP_LOGI(TAG, "Received Power OFF Command. Shutting down...");
        Serial1.end();
        pinMode(POWER_LATCH_PIN, OUTPUT);
        digitalWrite(POWER_LATCH_PIN, HIGH);
        delay(3000);
        ESP.restart();
    } else if (cmd == CMD_GET_DEBUG_INFO) {
        DebugInfo info = {0};
        if(WiFi.status() == WL_CONNECTED) {
            strncpy(info.ip, WiFi.localIP().toString().c_str(), 15);
        } else {
             strncpy(info.ip, "Disconnected", 15);
        }
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
        uint8_t type;
        if (unpack_connect_device_message(rx_buf, &type) == 0) {
            DeviceManager::getInstance().scanAndConnect((DeviceType)type);
        }
    } else if (cmd == CMD_FORGET_DEVICE) {
        uint8_t type;
        if (unpack_forget_device_message(rx_buf, &type) == 0) {
            DeviceManager::getInstance().forget((DeviceType)type);
        }
    } else if (cmd == CMD_GET_FULL_CONFIG) {
        sendEspLog(ESP_LOG_INFO, "CFG", "--- ESP32 Config ---");
        char buf[128];
        snprintf(buf, sizeof(buf), "Version: %s %s", __DATE__, __TIME__);
        sendEspLog(ESP_LOG_INFO, "CFG", buf);
        snprintf(buf, sizeof(buf), "MAC: %s", WiFi.macAddress().c_str());
        sendEspLog(ESP_LOG_INFO, "CFG", buf);
        snprintf(buf, sizeof(buf), "IP: %s", WiFi.localIP().toString().c_str());
        sendEspLog(ESP_LOG_INFO, "CFG", buf);

        // Connected Devices Dump
        sendEspLog(ESP_LOG_INFO, "CFG", "--- Connected Devices ---");
        DeviceType types[] = {DeviceType::DELTA_3, DeviceType::WAVE_2, DeviceType::DELTA_PRO_3, DeviceType::ALTERNATOR_CHARGER};
        const char* names[] = {"Delta 3", "Wave 2", "Delta Pro 3", "Alt Charger"};

        for(int i=0; i<4; i++) {
             DeviceSlot* s = DeviceManager::getInstance().getSlot(types[i]);
             if(s && (s->isConnected || !s->macAddress.empty())) {
                 snprintf(buf, sizeof(buf), "%s: SN=%s, MAC=%s, Conn=%d",
                    names[i],
                    s->serialNumber.empty() ? "N/A" : s->serialNumber.c_str(),
                    s->macAddress.empty() ? "N/A" : s->macAddress.c_str(),
                    s->isConnected);
                 sendEspLog(ESP_LOG_INFO, "CFG", buf);
             }
        }
    } else if (cmd == CMD_GET_DEBUG_DUMP) {
        EcoflowDataParser::triggerDebugDump();
    } else if (cmd == CMD_LOG_LIST_RESP) {
        uint16_t total, idx;
        uint32_t size;
        char name[32];
        if (unpack_log_list_resp_message(rx_buf, &total, &idx, &size, name) == 0) {
            if (!_logListMutex) _logListMutex = xSemaphoreCreateMutex();
            xSemaphoreTake(_logListMutex, portMAX_DELAY);
            if (idx == 0) _cachedLogList.clear();
            if (total > 0 && name[0]) {
                _cachedLogList.push_back({String(name), size});
            }
            if (idx == total - 1 || total == 0) {
                _logListReady = true;
            }
            xSemaphoreGive(_logListMutex);
        }
    } else if (cmd == CMD_LOG_DATA_CHUNK) {
        if (len >= 9) {
            uint16_t dataLen;
            memcpy(&dataLen, &rx_buf[7], 2);
            if (!_downloadMutex) _downloadMutex = xSemaphoreCreateMutex();
            xSemaphoreTake(_downloadMutex, portMAX_DELAY);
            if (dataLen > 0) {
                if (len >= 9 + dataLen) {
                    size_t current = _downloadBuffer.size();
                    _downloadBuffer.resize(current + dataLen);
                    memcpy(&_downloadBuffer[current], &rx_buf[9], dataLen);
                }
            } else {
                _downloadComplete = true;
                ESP_LOGI(TAG, "Log Download Complete (EOF Received)");
            }
            xSemaphoreGive(_downloadMutex);
        }
    }
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
    sendData(buffer, len);
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
        float power = src.mpptPwrWatt;
        if (power == 0) power = src.psdrPwrWatt;
        if (power == 0) power = abs(src.batPwrWatt);
        dst.batPwrWatt = (int)power;
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
        dst.expansion1Power = src.expansion1Power;
        dst.expansion2Power = src.expansion2Power;
        dst.acInputStatus = src.acInputStatus;
        dst.soh = src.soh;
        dst.dischargeRemainingTime = src.dischargeRemainingTime;
        dst.chargeRemainingTime = src.chargeRemainingTime;
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

void Stm32Serial::requestLogList() {
    if(!_logListMutex) _logListMutex = xSemaphoreCreateMutex();
    _logListReady = false;
    uint8_t buf[8];
    int l = pack_log_list_req_message(buf);
    sendData(buf, l);
}

std::vector<Stm32Serial::LogEntry> Stm32Serial::getLogList() {
    uint32_t start = millis();
    while(!_logListReady && millis() - start < 20000) {
        if (_logListReady) break;
        vTaskDelay(10);
    }
    xSemaphoreTake(_logListMutex, portMAX_DELAY);
    std::vector<LogEntry> copy = _cachedLogList;
    xSemaphoreGive(_logListMutex);
    return copy;
}

void Stm32Serial::deleteLog(const String& name) {
    uint8_t buf[64];
    int len = pack_log_delete_req_message(buf, name.c_str());
    sendData(buf, len);
}

void Stm32Serial::startLogDownload(const String& name) {
    if(!_downloadMutex) _downloadMutex = xSemaphoreCreateMutex();
    xSemaphoreTake(_downloadMutex, portMAX_DELAY);
    _downloadBuffer.clear();
    _downloadBuffer.reserve(4096);
    _downloadComplete = false;
    xSemaphoreGive(_downloadMutex);

    uint8_t buf[64];
    int len = pack_log_download_req_message(buf, name.c_str());
    sendData(buf, len);
}

size_t Stm32Serial::readLogChunk(uint8_t* buffer, size_t maxLen) {
    size_t read = 0;
    if(!_downloadMutex) return 0;
    xSemaphoreTake(_downloadMutex, portMAX_DELAY);
    if (!_downloadBuffer.empty()) {
        read = std::min(maxLen, _downloadBuffer.size());
        memcpy(buffer, _downloadBuffer.data(), read);
        _downloadBuffer.erase(_downloadBuffer.begin(), _downloadBuffer.begin() + read);
    }
    xSemaphoreGive(_downloadMutex);
    return read;
}

bool Stm32Serial::isLogDownloadComplete() {
    if(!_downloadMutex) return true;
    xSemaphoreTake(_downloadMutex, portMAX_DELAY);
    bool c = _downloadComplete && _downloadBuffer.empty();
    xSemaphoreGive(_downloadMutex);
    return c;
}

size_t Stm32Serial::getDownloadBufferSize() {
    if(!_downloadMutex) return 0;
    xSemaphoreTake(_downloadMutex, portMAX_DELAY);
    size_t s = _downloadBuffer.size();
    xSemaphoreGive(_downloadMutex);
    return s;
}

void Stm32Serial::abortLogDownload() {
    // Placeholder
}

static String otaFilename;

void Stm32Serial::startOta(const String& filename) {
    if (_otaRunning) return;
    otaFilename = filename;
    _otaRunning = true;
    xTaskCreate(otaTask, "OtaTask", 8192, this, 1, NULL);
}

void Stm32Serial::otaTask(void* parameter) {
    Stm32Serial* self = (Stm32Serial*)parameter;

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

    ESP_LOGI(TAG, "Calculating Checksum...");
    uint32_t crc = 0;
    uint8_t buf[256];
    size_t readLen;
    while(f.available()) {
        readLen = f.read(buf, sizeof(buf));
        crc = calculate_crc32(crc, buf, readLen);
    }
    f.close();
    ESP_LOGI(TAG, "CRC32: 0x%08X", crc);

    f = LittleFS.open(otaFilename, "r");

    bool startSuccess = false;
    int len = pack_ota_start_message(buf, totalSize);

    for(int attempt=0; attempt<3; attempt++) {
        ESP_LOGI(TAG, "Sending OTA Start (Attempt %d)", attempt+1);
        self->sendData(buf, len);

        otaAckReceived = false;
        otaNackReceived = false;
        uint32_t startWait = millis();
        while(!otaAckReceived && !otaNackReceived && (millis() - startWait < 30000)) {
            vTaskDelay(100);
        }

        if (otaAckReceived) {
            startSuccess = true;
            break;
        }

        if (otaNackReceived) {
            ESP_LOGE(TAG, "OTA Start NACK received");
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

    uint32_t offset = 0;
    uint8_t chunk[200];
    int last_log_progress = -1;
    bool transferFailed = false;

    while (f.available()) {
        int bytesRead = f.read(chunk, sizeof(chunk));

        len = pack_ota_chunk_message(buf, offset, chunk, bytesRead);

        bool chunkSuccess = false;
        for(int retries=0; retries<3; retries++) {
            self->sendData(buf, len);

            otaAckReceived = false;
            otaNackReceived = false;
            uint32_t startWait = millis();
            while(!otaAckReceived && !otaNackReceived && (millis() - startWait < 2000)) {
                vTaskDelay(5);
            }

            if (otaAckReceived) {
                chunkSuccess = true;
                break;
            }
            if (otaNackReceived) {
                 ESP_LOGW(TAG, "Chunk NACK at %d, retry %d", offset, retries);
                 vTaskDelay(50);
            } else {
                 ESP_LOGW(TAG, "Chunk Timeout at %d, retry %d", offset, retries);
            }
        }

        if (!chunkSuccess) {
            ESP_LOGE(TAG, "OTA Chunk Failed at %d", offset);
            ota_state = 4; ota_msg = "Chunk Fail";
            transferFailed = true;
            break;
        }

        offset += bytesRead;
        ota_progress = (offset * 100) / totalSize;
        if (ota_progress != last_log_progress && ota_progress % 10 == 0) {
            ESP_LOGI(TAG, "OTA Progress: %d%% (%d/%d)", ota_progress, offset, totalSize);
            last_log_progress = ota_progress;
        }
    }

    f.close();

    if (!transferFailed && offset == totalSize) {
        ESP_LOGI(TAG, "OTA Upload Complete. Sending End (CRC: 0x%08X)...", crc);
        len = pack_ota_end_message(buf, crc);

        bool endSuccess = false;
        for(int retries=0; retries<3; retries++) {
            self->sendData(buf, len);

            otaAckReceived = false;
            otaNackReceived = false;
             uint32_t startWait = millis();
            while(!otaAckReceived && !otaNackReceived && (millis() - startWait < 5000)) {
                vTaskDelay(50);
            }
            if (otaAckReceived) { endSuccess = true; break; }
            if (otaNackReceived) { ESP_LOGE(TAG, "OTA End NACK (Checksum Mismatch?)"); break; }
        }

        if (endSuccess) {
            vTaskDelay(500);
            ESP_LOGI(TAG, "Sending Apply...");
            len = pack_ota_apply_message(buf);
            self->sendData(buf, len);
            ota_state = 3; ota_msg = "STM32 Rebooting...";
        } else {
             ota_state = 4; ota_msg = "Checksum Fail";
        }
    } else {
        if (ota_state != 4) { ota_state = 4; ota_msg = "Incomplete"; }
    }

    self->_otaRunning = false;
    vTaskDelete(NULL);
}
