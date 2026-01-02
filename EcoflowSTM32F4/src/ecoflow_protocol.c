/**
 * @file ecoflow_protocol.c
 * @author Lollokara
 * @brief Implementation of the shared protocol serialization/deserialization functions.
 *
 * This file contains the logic to pack and unpack the C structures defined in
 * ecoflow_protocol.h into raw byte buffers for UART transmission.
 * It also implements the CRC8 checksum calculation.
 */

#include "ecoflow_protocol.h"
#include <string.h>

/**
 * @brief Calculates CRC8 checksum using the MAXIM polynomial (0x31).
 *
 * Used to verify packet integrity.
 *
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

/**
 * @brief Packs a Handshake packet.
 * @param buffer Output buffer (must be >= 4 bytes).
 * @return Length of the packed message.
 */
int pack_handshake_message(uint8_t *buffer) {
    buffer[0] = START_BYTE;
    buffer[1] = CMD_HANDSHAKE;
    buffer[2] = 0;
    buffer[3] = calculate_crc8(&buffer[1], 2);
    return 4;
}

/**
 * @brief Packs a Handshake Acknowledgment packet.
 * @param buffer Output buffer.
 * @return Length of the packed message.
 */
int pack_handshake_ack_message(uint8_t *buffer) {
    buffer[0] = START_BYTE;
    buffer[1] = CMD_HANDSHAKE_ACK;
    buffer[2] = 0;
    buffer[3] = calculate_crc8(&buffer[1], 2);
    return 4;
}

/**
 * @brief Packs the Device List packet.
 * @param buffer Output buffer.
 * @param list Pointer to the DeviceList structure.
 * @return Length of the packed message.
 */
int pack_device_list_message(uint8_t *buffer, const DeviceList *list) {
    uint8_t len = sizeof(DeviceList);
    buffer[0] = START_BYTE;
    buffer[1] = CMD_DEVICE_LIST;
    buffer[2] = len;
    memcpy(&buffer[3], list, len);
    buffer[3 + len] = calculate_crc8(&buffer[1], 2 + len);
    return 4 + len;
}

/**
 * @brief Unpacks a Device List packet.
 * @param buffer Input buffer (starting at START_BYTE).
 * @param list Pointer to DeviceList structure to fill.
 * @return 0 on success, -1 on CRC error, -2 on length error.
 */
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

/**
 * @brief Packs a Power Off command.
 * @param buffer Output buffer.
 * @return Length of the packed message.
 */
int pack_power_off_message(uint8_t *buffer) {
    buffer[0] = START_BYTE;
    buffer[1] = CMD_POWER_OFF;
    buffer[2] = 0; // Length
    buffer[3] = calculate_crc8(&buffer[1], 2);
    return 4;
}

/**
 * @brief Packs a Device List Acknowledgment packet.
 * @param buffer Output buffer.
 * @return Length of the packed message.
 */
int pack_device_list_ack_message(uint8_t *buffer) {
    buffer[0] = START_BYTE;
    buffer[1] = CMD_DEVICE_LIST_ACK;
    buffer[2] = 0;
    buffer[3] = calculate_crc8(&buffer[1], 2);
    return 4;
}

/**
 * @brief Packs a Request Device Status packet.
 * @param buffer Output buffer.
 * @param device_id ID of the device to query.
 * @return Length of the packed message.
 */
int pack_get_device_status_message(uint8_t *buffer, uint8_t device_id) {
    uint8_t len = 1;
    buffer[0] = START_BYTE;
    buffer[1] = CMD_GET_DEVICE_STATUS;
    buffer[2] = len;
    buffer[3] = device_id;
    buffer[3 + len] = calculate_crc8(&buffer[1], 2 + len);
    return 4 + len;
}

/**
 * @brief Unpacks a Request Device Status packet.
 * @param buffer Input buffer.
 * @param device_id Pointer to store the requested ID.
 * @return 0 on success, error code otherwise.
 */
int unpack_get_device_status_message(const uint8_t *buffer, uint8_t *device_id) {
    uint8_t len = buffer[2];
    if (len != 1) return -2;

    uint8_t received_crc = buffer[3 + len];
    uint8_t calculated_crc = calculate_crc8(&buffer[1], 2 + len);
    if (received_crc != calculated_crc) return -1;

    *device_id = buffer[3];
    return 0;
}

/**
 * @brief Packs a Device Status (Telemetry) packet.
 * @param buffer Output buffer.
 * @param status Pointer to the DeviceStatus data.
 * @return Length of the packed message.
 */
int pack_device_status_message(uint8_t *buffer, const DeviceStatus *status) {
    uint8_t len = sizeof(DeviceStatus);
    buffer[0] = START_BYTE;
    buffer[1] = CMD_DEVICE_STATUS;
    buffer[2] = len;
    memcpy(&buffer[3], status, len);
    buffer[3 + len] = calculate_crc8(&buffer[1], 2 + len);
    return 4 + len;
}

/**
 * @brief Unpacks a Device Status packet.
 * @param buffer Input buffer.
 * @param status Pointer to store the data.
 * @return 0 on success, error code otherwise.
 */
int unpack_device_status_message(const uint8_t *buffer, DeviceStatus *status) {
    uint8_t len = buffer[2];
    if (len != sizeof(DeviceStatus)) return -2;

    uint8_t received_crc = buffer[3 + len];
    uint8_t calculated_crc = calculate_crc8(&buffer[1], 2 + len);
    if (received_crc != calculated_crc) return -1;

    memcpy(status, &buffer[3], len);
    return 0;
}

/**
 * @brief Packs a Wave 2 Control packet.
 * @param buffer Output buffer.
 * @param type Control type (Mode, Temp, etc).
 * @param value New value.
 * @return Length of the packed message.
 */
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

/**
 * @brief Unpacks a Wave 2 Control packet.
 * @param buffer Input buffer.
 * @param type Pointer to store type.
 * @param value Pointer to store value.
 * @return 0 on success, error code otherwise.
 */
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

/**
 * @brief Packs an AC Control packet.
 * @param buffer Output buffer.
 * @param enable 1 for ON, 0 for OFF.
 * @return Length of the packed message.
 */
int pack_set_ac_message(uint8_t *buffer, uint8_t enable) {
    uint8_t len = 1;
    buffer[0] = START_BYTE;
    buffer[1] = CMD_SET_AC;
    buffer[2] = len;
    buffer[3] = enable;
    buffer[3 + len] = calculate_crc8(&buffer[1], 2 + len);
    return 4 + len;
}

/**
 * @brief Unpacks an AC Control packet.
 * @param buffer Input buffer.
 * @param enable Pointer to store the value.
 * @return 0 on success, error code otherwise.
 */
int unpack_set_ac_message(const uint8_t *buffer, uint8_t *enable) {
    uint8_t len = buffer[2];
    if (len != 1) return -2;
    uint8_t received_crc = buffer[3 + len];
    uint8_t calculated_crc = calculate_crc8(&buffer[1], 2 + len);
    if (received_crc != calculated_crc) return -1;
    *enable = buffer[3];
    return 0;
}

/**
 * @brief Packs a DC Control packet.
 * @param buffer Output buffer.
 * @param enable 1 for ON, 0 for OFF.
 * @return Length of the packed message.
 */
int pack_set_dc_message(uint8_t *buffer, uint8_t enable) {
    uint8_t len = 1;
    buffer[0] = START_BYTE;
    buffer[1] = CMD_SET_DC;
    buffer[2] = len;
    buffer[3] = enable;
    buffer[3 + len] = calculate_crc8(&buffer[1], 2 + len);
    return 4 + len;
}

/**
 * @brief Unpacks a DC Control packet.
 * @param buffer Input buffer.
 * @param enable Pointer to store the value.
 * @return 0 on success, error code otherwise.
 */
int unpack_set_dc_message(const uint8_t *buffer, uint8_t *enable) {
    uint8_t len = buffer[2];
    if (len != 1) return -2;
    uint8_t received_crc = buffer[3 + len];
    uint8_t calculated_crc = calculate_crc8(&buffer[1], 2 + len);
    if (received_crc != calculated_crc) return -1;
    *enable = buffer[3];
    return 0;
}

/**
 * @brief Packs a Set Value packet (Numeric setting).
 * @param buffer Output buffer.
 * @param type Value type (e.g., SET_VAL_AC_LIMIT).
 * @param value The 32-bit integer value.
 * @return Length of the packed message.
 */
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

/**
 * @brief Unpacks a Set Value packet.
 * @param buffer Input buffer.
 * @param type Pointer to store type.
 * @param value Pointer to store value.
 * @return 0 on success, error code otherwise.
 */
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

/**
 * @brief Packs a Request Debug Info packet.
 * @param buffer Output buffer.
 * @return Length of the packed message.
 */
int pack_get_debug_info_message(uint8_t *buffer) {
    buffer[0] = START_BYTE;
    buffer[1] = CMD_GET_DEBUG_INFO;
    buffer[2] = 0;
    buffer[3] = calculate_crc8(&buffer[1], 2);
    return 4;
}

/**
 * @brief Packs a Debug Info packet (Response).
 * @param buffer Output buffer.
 * @param info Pointer to DebugInfo struct.
 * @return Length of the packed message.
 */
int pack_debug_info_message(uint8_t *buffer, const DebugInfo *info) {
    uint8_t len = sizeof(DebugInfo);
    buffer[0] = START_BYTE;
    buffer[1] = CMD_DEBUG_INFO;
    buffer[2] = len;
    memcpy(&buffer[3], info, len);
    buffer[3 + len] = calculate_crc8(&buffer[1], 2 + len);
    return 4 + len;
}

/**
 * @brief Unpacks a Debug Info packet.
 * @param buffer Input buffer.
 * @param info Pointer to DebugInfo struct to fill.
 * @return 0 on success, error code otherwise.
 */
int unpack_debug_info_message(const uint8_t *buffer, DebugInfo *info) {
    uint8_t len = buffer[2];
    if (len != sizeof(DebugInfo)) return -2;

    uint8_t received_crc = buffer[3 + len];
    uint8_t calculated_crc = calculate_crc8(&buffer[1], 2 + len);
    if (received_crc != calculated_crc) return -1;

    memcpy(info, &buffer[3], len);
    return 0;
}

/**
 * @brief Packs a Connect Device packet.
 * @param buffer Output buffer.
 * @param device_type Device Type to connect to.
 * @return Length of the packed message.
 */
int pack_connect_device_message(uint8_t *buffer, uint8_t device_type) {
    uint8_t len = 1;
    buffer[0] = START_BYTE;
    buffer[1] = CMD_CONNECT_DEVICE;
    buffer[2] = len;
    buffer[3] = device_type;
    buffer[3 + len] = calculate_crc8(&buffer[1], 2 + len);
    return 4 + len;
}

/**
 * @brief Unpacks a Connect Device packet.
 * @param buffer Input buffer.
 * @param device_type Pointer to store device type.
 * @return 0 on success, error code otherwise.
 */
int unpack_connect_device_message(const uint8_t *buffer, uint8_t *device_type) {
    uint8_t len = buffer[2];
    if (len != 1) return -2;
    uint8_t received_crc = buffer[3 + len];
    uint8_t calculated_crc = calculate_crc8(&buffer[1], 2 + len);
    if (received_crc != calculated_crc) return -1;
    *device_type = buffer[3];
    return 0;
}

/**
 * @brief Packs a Forget Device packet.
 * @param buffer Output buffer.
 * @param device_type Device Type to forget.
 * @return Length of the packed message.
 */
int pack_forget_device_message(uint8_t *buffer, uint8_t device_type) {
    uint8_t len = 1;
    buffer[0] = START_BYTE;
    buffer[1] = CMD_FORGET_DEVICE;
    buffer[2] = len;
    buffer[3] = device_type;
    buffer[3 + len] = calculate_crc8(&buffer[1], 2 + len);
    return 4 + len;
}

/**
 * @brief Unpacks a Forget Device packet.
 * @param buffer Input buffer.
 * @param device_type Pointer to store device type.
 * @return 0 on success, error code otherwise.
 */
int unpack_forget_device_message(const uint8_t *buffer, uint8_t *device_type) {
    uint8_t len = buffer[2];
    if (len != 1) return -2;
    uint8_t received_crc = buffer[3 + len];
    uint8_t calculated_crc = calculate_crc8(&buffer[1], 2 + len);
    if (received_crc != calculated_crc) return -1;
    *device_type = buffer[3];
    return 0;
}
