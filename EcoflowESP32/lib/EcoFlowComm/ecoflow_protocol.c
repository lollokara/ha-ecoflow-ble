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

/**
 * @brief Packs a BatteryStatus message into a buffer.
 * @param buffer The destination buffer.
 * @param status Pointer to the BatteryStatus struct.
 * @return The total length of the packed message.
 */
int pack_battery_status_message(uint8_t *buffer, const BatteryStatus *status) {
    uint8_t len = sizeof(BatteryStatus);
    buffer[0] = START_BYTE;
    buffer[1] = CMD_BATTERY_STATUS;
    buffer[2] = len;
    memcpy(&buffer[3], status, len);
    buffer[3 + len] = calculate_crc8(&buffer[1], 2 + len); // CRC over CMD, LEN, PAYLOAD
    return 4 + len;
}

/**
 * @brief Unpacks a BatteryStatus message from a buffer.
 * @param buffer The source buffer.
 * @param status Pointer to the destination BatteryStatus struct.
 * @return 0 on success, -1 on failure (CRC mismatch).
 */
int unpack_battery_status_message(const uint8_t *buffer, BatteryStatus *status) {
    uint8_t len = buffer[2];
    uint8_t received_crc = buffer[3 + len];
    uint8_t calculated_crc = calculate_crc8(&buffer[1], 2 + len);

    if (received_crc != calculated_crc) {
        return -1; // CRC error
    }

    memcpy(status, &buffer[3], len);
    return 0;
}

/**
 * @brief Packs a Request Status Update message into a buffer.
 * @param buffer The destination buffer.
 * @return The total length of the packed message.
 */
int pack_request_status_message(uint8_t *buffer) {
    buffer[0] = START_BYTE;
    buffer[1] = CMD_REQUEST_STATUS_UPDATE;
    buffer[2] = 0; // No payload
    buffer[3] = calculate_crc8(&buffer[1], 2);
    return 4;
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
    return 0;
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
