#include "ecoflow_protocol.h"
#include <string.h>

/**
 * @brief Calculates CRC8 checksum using the MAXIM polynomial (0x31).
 * @param data Pointer to the data buffer.
 * @param len Length of the data in bytes.
 * @return The 8-bit CRC value.
 */
uint8_t calculate_crc8(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

int pack_handshake_message(uint8_t *buffer) {
    buffer[0] = START_BYTE;
    buffer[1] = CMD_HANDSHAKE;
    buffer[2] = 0;
    buffer[3] = calculate_crc8(&buffer[1], 2);
    return 4;
}

int pack_handshake_ack_message(uint8_t *buffer) {
    buffer[0] = START_BYTE;
    buffer[1] = CMD_HANDSHAKE_ACK;
    buffer[2] = 0;
    buffer[3] = calculate_crc8(&buffer[1], 2);
    return 4;
}

int pack_device_list_message(uint8_t *buffer, const DeviceList *list) {
    uint8_t len = sizeof(DeviceList);
    buffer[0] = START_BYTE;
    buffer[1] = CMD_DEVICE_LIST;
    buffer[2] = len;
    memcpy(&buffer[3], list, len);
    buffer[3 + len] = calculate_crc8(&buffer[1], 2 + len);
    return 4 + len;
}

int unpack_device_list_message(const uint8_t *buffer, DeviceList *list) {
    uint8_t len = buffer[2];
    if (len != sizeof(DeviceList)) return -2; // Size mismatch

    uint8_t received_crc = buffer[3 + len];
    uint8_t calculated_crc = calculate_crc8(&buffer[1], 2 + len);
    if (received_crc != calculated_crc) return -1;

    memcpy(list, &buffer[3], len);

    // Safety check: Clamp device count
    if (list->count > MAX_DEVICES) {
        list->count = MAX_DEVICES;
    }

    return 0;
}

int pack_power_off_message(uint8_t *buffer) {
    buffer[0] = START_BYTE;
    buffer[1] = CMD_POWER_OFF;
    buffer[2] = 0; // Length
    buffer[3] = calculate_crc8(&buffer[1], 2);
    return 4;
}

int pack_device_list_ack_message(uint8_t *buffer) {
    buffer[0] = START_BYTE;
    buffer[1] = CMD_DEVICE_LIST_ACK;
    buffer[2] = 0;
    buffer[3] = calculate_crc8(&buffer[1], 2);
    return 4;
}

int pack_get_device_status_message(uint8_t *buffer, uint8_t device_id) {
    uint8_t len = 1;
    buffer[0] = START_BYTE;
    buffer[1] = CMD_GET_DEVICE_STATUS;
    buffer[2] = len;
    buffer[3] = device_id;
    buffer[3 + len] = calculate_crc8(&buffer[1], 2 + len);
    return 4 + len;
}

int unpack_get_device_status_message(const uint8_t *buffer, uint8_t *device_id) {
    uint8_t len = buffer[2];
    if (len != 1) return -2;

    uint8_t received_crc = buffer[3 + len];
    uint8_t calculated_crc = calculate_crc8(&buffer[1], 2 + len);
    if (received_crc != calculated_crc) return -1;

    *device_id = buffer[3];
    return 0;
}

int pack_device_status_message(uint8_t *buffer, const DeviceStatus *status) {
    uint8_t len = sizeof(DeviceStatus);
    buffer[0] = START_BYTE;
    buffer[1] = CMD_DEVICE_STATUS;
    buffer[2] = len;
    memcpy(&buffer[3], status, len);
    buffer[3 + len] = calculate_crc8(&buffer[1], 2 + len);
    return 4 + len;
}

int unpack_device_status_message(const uint8_t *buffer, DeviceStatus *status) {
    uint8_t len = buffer[2];
    if (len != sizeof(DeviceStatus)) return -2;

    uint8_t received_crc = buffer[3 + len];
    uint8_t calculated_crc = calculate_crc8(&buffer[1], 2 + len);
    if (received_crc != calculated_crc) return -1;

    memcpy(status, &buffer[3], len);
    return 0;
}

int pack_set_wave2_message(uint8_t *buffer, uint8_t type, uint8_t value) {
    uint8_t len = sizeof(Wave2SetMsg);
    buffer[0] = START_BYTE;
    buffer[1] = CMD_SET_WAVE2;
    buffer[2] = len;

    Wave2SetMsg msg;
    msg.type = type;
    msg.value = value;

    memcpy(&buffer[3], &msg, len);
    buffer[3 + len] = calculate_crc8(&buffer[1], 2 + len);
    return 4 + len;
}

int unpack_set_wave2_message(const uint8_t *buffer, uint8_t *type, uint8_t *value) {
    uint8_t len = buffer[2];
    if (len != sizeof(Wave2SetMsg)) return -2;

    uint8_t received_crc = buffer[3 + len];
    uint8_t calculated_crc = calculate_crc8(&buffer[1], 2 + len);
    if (received_crc != calculated_crc) return -1;

    Wave2SetMsg msg;
    memcpy(&msg, &buffer[3], len);
    *type = msg.type;
    *value = msg.value;
    return 0;
}

int pack_set_ac_message(uint8_t *buffer, uint8_t enable) {
    uint8_t len = 1;
    buffer[0] = START_BYTE;
    buffer[1] = CMD_SET_AC;
    buffer[2] = len;
    buffer[3] = enable;
    buffer[3 + len] = calculate_crc8(&buffer[1], 2 + len);
    return 4 + len;
}

int unpack_set_ac_message(const uint8_t *buffer, uint8_t *enable) {
    uint8_t len = buffer[2];
    if (len != 1) return -2;
    uint8_t received_crc = buffer[3 + len];
    uint8_t calculated_crc = calculate_crc8(&buffer[1], 2 + len);
    if (received_crc != calculated_crc) return -1;
    *enable = buffer[3];
    return 0;
}

int pack_set_dc_message(uint8_t *buffer, uint8_t enable) {
    uint8_t len = 1;
    buffer[0] = START_BYTE;
    buffer[1] = CMD_SET_DC;
    buffer[2] = len;
    buffer[3] = enable;
    buffer[3 + len] = calculate_crc8(&buffer[1], 2 + len);
    return 4 + len;
}

int unpack_set_dc_message(const uint8_t *buffer, uint8_t *enable) {
    uint8_t len = buffer[2];
    if (len != 1) return -2;
    uint8_t received_crc = buffer[3 + len];
    uint8_t calculated_crc = calculate_crc8(&buffer[1], 2 + len);
    if (received_crc != calculated_crc) return -1;
    *enable = buffer[3];
    return 0;
}

int pack_set_value_message(uint8_t *buffer, uint8_t type, int value) {
    // [type:1][value:4]
    uint8_t len = 5;
    buffer[0] = START_BYTE;
    buffer[1] = CMD_SET_VALUE;
    buffer[2] = len;
    buffer[3] = type;
    memcpy(&buffer[4], &value, 4);
    buffer[3 + len] = calculate_crc8(&buffer[1], 2 + len);
    return 4 + len;
}

int unpack_set_value_message(const uint8_t *buffer, uint8_t *type, int *value) {
    uint8_t len = buffer[2];
    if (len != 5) return -2;
    uint8_t received_crc = buffer[3 + len];
    uint8_t calculated_crc = calculate_crc8(&buffer[1], 2 + len);
    if (received_crc != calculated_crc) return -1;

    *type = buffer[3];
    memcpy(value, &buffer[4], 4);
    return 0;
}

int pack_get_debug_info_message(uint8_t *buffer) {
    buffer[0] = START_BYTE;
    buffer[1] = CMD_GET_DEBUG_INFO;
    buffer[2] = 0;
    buffer[3] = calculate_crc8(&buffer[1], 2);
    return 4;
}

int pack_debug_info_message(uint8_t *buffer, const DebugInfo *info) {
    uint8_t len = sizeof(DebugInfo);
    buffer[0] = START_BYTE;
    buffer[1] = CMD_DEBUG_INFO;
    buffer[2] = len;
    memcpy(&buffer[3], info, len);
    buffer[3 + len] = calculate_crc8(&buffer[1], 2 + len);
    return 4 + len;
}

int unpack_debug_info_message(const uint8_t *buffer, DebugInfo *info) {
    uint8_t len = buffer[2];
    if (len != sizeof(DebugInfo)) return -2;

    uint8_t received_crc = buffer[3 + len];
    uint8_t calculated_crc = calculate_crc8(&buffer[1], 2 + len);
    if (received_crc != calculated_crc) return -1;

    memcpy(info, &buffer[3], len);
    return 0;
}
