#include "Stm32Serial.h"
#include <Arduino.h>
#include "ecoflow_protocol.h"
#include "DeviceManager.h"
#include "CmdUtils.h"
#include "OtaManager.h"

#define START_BYTE 0xAA

// Protocol constants
#define CMD_HANDSHAKE 0x20
#define CMD_HANDSHAKE_ACK 0x21
#define CMD_DEVICE_LIST 0x22
#define CMD_DEVICE_LIST_ACK 0x23
#define CMD_DEVICE_STATUS 0x24
#define CMD_GET_DEVICE_STATUS 0x25
#define CMD_GET_DEVICE_LIST 0x26

// Control Commands from STM32
#define CMD_SET_WAVE2 0x30
#define CMD_SET_AC 0x31
#define CMD_SET_DC 0x32
#define CMD_SET_VALUE 0x40
#define CMD_POWER_OFF 0x50
#define CMD_CONNECT_DEVICE 0x62
#define CMD_FORGET_DEVICE 0x63

// OTA Commands (Standardized)
#define CMD_OTA_START 0xA0
#define CMD_OTA_CHUNK 0xA1
#define CMD_OTA_END   0xA2
#define CMD_OTA_APPLY 0xA3
#define CMD_OTA_ACK   0x06
#define CMD_OTA_NACK  0x15

Stm32Serial::Stm32Serial(HardwareSerial* serial, OtaManager* ota) : _serial(serial), _ota(ota) {
}

void Stm32Serial::begin(unsigned long baud) {
    _serial->begin(baud, SERIAL_8N1, 18, 17); // RX, TX
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
    buf[0] = START_BYTE;
    buf[1] = cmd;
    buf[2] = len;
    if (len > 0 && payload != NULL) {
        memcpy(&buf[3], payload, len);
    }

    // Calculate CRC over CMD, LEN, PAYLOAD
    buf[3+len] = crc8(&buf[1], len + 2);

    _serial->write(buf, len + 4);
}

void Stm32Serial::handle() {
    while (_serial->available()) {
        uint8_t b = _serial->read();

        static int idx = 0;
        static uint8_t buf[1024];
        static uint8_t expected_len = 0;

        if (idx == 0 && b != START_BYTE) continue;
        buf[idx++] = b;

        // Header is [START][CMD][LEN]
        if (idx == 3) {
            expected_len = buf[2];
        }

        if (idx >= 3 && idx >= (expected_len + 4)) {
             uint8_t cmd = buf[1];
             uint8_t len = buf[2];

             // Check CRC
             uint8_t crc = crc8(&buf[1], len + 2);
             if (crc == buf[len + 3]) {
                  // Valid Packet

                  // OTA Handling
                  bool handled = false;
                  if (_ota) {
                      if (cmd == CMD_OTA_ACK) { _ota->onAck(lastSentCmd); handled = true; }
                      else if (cmd == CMD_OTA_NACK) { _ota->onNack(lastSentCmd); handled = true; }
                  }

                  // Application Handling
                  if (!handled) {
                      uint8_t* payload = &buf[3];

                      if (cmd == CMD_GET_DEVICE_LIST) {
                          sendDeviceList();
                      }
                      else if (cmd == CMD_GET_DEVICE_STATUS) {
                          if (len > 0) sendDeviceStatus(payload[0]);
                      }
                      else if (cmd == CMD_HANDSHAKE) {
                           sendPacket(CMD_HANDSHAKE_ACK, NULL, 0);
                      }
                      // --- Control Commands ---
                      else if (cmd == CMD_SET_AC) {
                          if (len > 0) {
                              // Assuming active device or default logic
                              EcoflowESP32* dev = DeviceManager::getInstance().getDevice(DeviceType::DELTA_3);
                              if (dev) dev->setAC(payload[0] ? true : false);
                          }
                      }
                      else if (cmd == CMD_SET_DC) {
                          if (len > 0) {
                              EcoflowESP32* dev = DeviceManager::getInstance().getDevice(DeviceType::DELTA_3);
                              if (dev) dev->setDC(payload[0] ? true : false);
                          }
                      }
                      else if (cmd == CMD_SET_WAVE2) {
                          // [Type][Value]
                          if (len >= 2) {
                              EcoflowESP32* dev = DeviceManager::getInstance().getDevice(DeviceType::WAVE_2);
                              if (dev) {
                                  uint8_t type = payload[0];
                                  uint8_t val = payload[1];
                                  // Map types based on Wave2 UI Logic
                                  if (type == 0) dev->setMainMode(val);
                                  else if (type == 1) dev->setSubMode(val);
                                  else if (type == 2) dev->setTemperature(val);
                                  else if (type == 4) dev->setFanSpeed(val);
                                  else if (type == 5) dev->setPowerState(val);
                              }
                          }
                      }
                      else if (cmd == CMD_CONNECT_DEVICE) {
                          if (len > 0) DeviceManager::getInstance().scanAndConnect((DeviceType)payload[0]);
                      }
                      else if (cmd == CMD_FORGET_DEVICE) {
                          if (len > 0) DeviceManager::getInstance().forget((DeviceType)payload[0]);
                      }
                      else if (cmd == CMD_POWER_OFF) {
                          // Handle Power Off Sequence if needed
                      }
                  }
             }
             idx = 0;
        }
        if (idx >= sizeof(buf)) idx = 0; // Safety reset
    }
}

// OTA Methods
void Stm32Serial::sendOtaStart(uint32_t size) {
    uint8_t payload[4];
    memcpy(payload, &size, 4);
    sendPacket(CMD_OTA_START, payload, 4);
    lastSentCmd = CMD_OTA_START;
}

void Stm32Serial::sendOtaChunk(uint32_t offset, uint8_t* data, uint8_t len) {
    uint8_t payload[250];
    memcpy(payload, &offset, 4);
    memcpy(&payload[4], data, len);
    sendPacket(CMD_OTA_CHUNK, payload, len + 4);
    lastSentCmd = CMD_OTA_CHUNK;
}

void Stm32Serial::sendOtaEnd() {
    sendPacket(CMD_OTA_END, NULL, 0);
    lastSentCmd = CMD_OTA_END;
}

void Stm32Serial::sendOtaApply() {
    sendPacket(CMD_OTA_APPLY, NULL, 0);
    lastSentCmd = CMD_OTA_APPLY;
}

// Device Status & List
void Stm32Serial::sendDeviceList() {
    uint8_t buffer[256];
    uint8_t offset = 0;

    // Iterate known types
    DeviceType types[] = {DeviceType::DELTA_3, DeviceType::WAVE_2, DeviceType::DELTA_PRO_3, DeviceType::ALTERNATOR_CHARGER};
    buffer[offset++] = 4; // Count

    for (int i=0; i<4; i++) {
        DeviceSlot* s = DeviceManager::getInstance().getSlot(types[i]);
        buffer[offset++] = (uint8_t)s->type; // ID
        buffer[offset++] = (uint8_t)s->type; // Type

        uint8_t nameLen = s->name.length();
        if (nameLen > 15) nameLen = 15;
        buffer[offset++] = nameLen;
        memcpy(&buffer[offset], s->name.c_str(), nameLen);
        offset += nameLen;

        buffer[offset++] = s->isConnected ? 1 : 0;
    }

    sendPacket(CMD_DEVICE_LIST, buffer, offset);
}

void Stm32Serial::sendDeviceStatus(uint8_t device_id) {
    // Basic status serialization
    // [ID(1)][Type(1)][Connected(1)][Batt(1)][In(2)][Out(2)][State(1)]

    uint8_t buf[64];
    memset(buf, 0, sizeof(buf));
    uint8_t idx = 0;

    buf[idx++] = device_id;
    buf[idx++] = device_id; // Type assumed same as ID

    EcoflowESP32* dev = NULL;
    if (device_id == (uint8_t)DeviceType::DELTA_3) dev = DeviceManager::getInstance().getDevice(DeviceType::DELTA_3);
    else if (device_id == (uint8_t)DeviceType::WAVE_2) dev = DeviceManager::getInstance().getDevice(DeviceType::WAVE_2);
    else if (device_id == (uint8_t)DeviceType::DELTA_PRO_3) dev = DeviceManager::getInstance().getDevice(DeviceType::DELTA_PRO_3);
    else if (device_id == (uint8_t)DeviceType::ALTERNATOR_CHARGER) dev = DeviceManager::getInstance().getDevice(DeviceType::ALTERNATOR_CHARGER);

    if (dev && dev->isConnected()) {
        buf[idx++] = 1; // Connected
        buf[idx++] = dev->getBatteryLevel();

        int inW = dev->getInputPower();
        memcpy(&buf[idx], &inW, 2);
        idx += 2;

        int outW = dev->getOutputPower();
        memcpy(&buf[idx], &outW, 2);
        idx += 2;

        uint8_t state = 0;
        if (dev->isAcOn()) state |= 1;
        if (dev->isDcOn()) state |= 2;
        if (dev->isUsbOn()) state |= 4;
        buf[idx++] = state;
    } else {
        buf[idx++] = 0; // Not connected
        buf[idx++] = 0; // Batt
        idx += 5; // Fill rest with 0
    }

    sendPacket(CMD_DEVICE_STATUS, buf, idx);
}
