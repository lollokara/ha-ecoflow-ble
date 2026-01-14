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

// Log API

int pack_log_list_req_message(uint8_t *buffer) {
    buffer[0] = START_BYTE;
    buffer[1] = CMD_LOG_LIST_REQ;
    buffer[2] = 0;
    buffer[3] = calculate_crc8(&buffer[1], 2);
    return 4;
}

int pack_log_list_resp_message(uint8_t *buffer, uint16_t total, uint16_t index, uint32_t size, const char* name) {
    // Structure: [Total:2][Index:2][Size:4][Name:32] = 40 bytes
    LogListEntryMsg msg;
    msg.total_files = total;
    msg.index = index;
    msg.size = size;
    strncpy(msg.name, name, 31);
    msg.name[31] = 0;

    uint8_t len = sizeof(LogListEntryMsg);
    buffer[0] = START_BYTE;
    buffer[1] = CMD_LOG_LIST_RESP;
    buffer[2] = len;
    memcpy(&buffer[3], &msg, len);
    buffer[3 + len] = calculate_crc8(&buffer[1], 2 + len);
    return 4 + len;
}

int unpack_log_list_resp_message(const uint8_t *buffer, uint16_t *total, uint16_t *index, uint32_t *size, char* name) {
    uint8_t len = buffer[2];
    if (len != sizeof(LogListEntryMsg)) return -2;

    uint8_t received_crc = buffer[3 + len];
    uint8_t calculated_crc = calculate_crc8(&buffer[1], 2 + len);
    if (received_crc != calculated_crc) return -1;

    LogListEntryMsg msg;
    memcpy(&msg, &buffer[3], len);

    *total = msg.total_files;
    *index = msg.index;
    *size = msg.size;
    strncpy(name, msg.name, 32);
    return 0;
}

int pack_log_download_req_message(uint8_t *buffer, const char* name) {
    LogDownloadReqMsg msg;
    strncpy(msg.name, name, 31);
    msg.name[31] = 0;

    uint8_t len = sizeof(LogDownloadReqMsg);
    buffer[0] = START_BYTE;
    buffer[1] = CMD_LOG_DOWNLOAD_REQ;
    buffer[2] = len;
    memcpy(&buffer[3], &msg, len);
    buffer[3 + len] = calculate_crc8(&buffer[1], 2 + len);
    return 4 + len;
}

int unpack_log_download_req_message(const uint8_t *buffer, char* name) {
    uint8_t len = buffer[2];
    if (len != sizeof(LogDownloadReqMsg)) return -2;
    uint8_t received_crc = buffer[3 + len];
    uint8_t calculated_crc = calculate_crc8(&buffer[1], 2 + len);
    if (received_crc != calculated_crc) return -1;

    LogDownloadReqMsg msg;
    memcpy(&msg, &buffer[3], len);
    strncpy(name, msg.name, 32);
    return 0;
}

int pack_log_data_chunk_message(uint8_t *buffer, uint32_t offset, const uint8_t* data, uint16_t len) {
    // [Offset:4][Len:2][Data:...]
    // Header size = 6.
    // Payload max 255. So Data max 249.
    if (len > 249) return -3; // Too large

    uint8_t payload_len = 6 + len;
    buffer[0] = START_BYTE;
    buffer[1] = CMD_LOG_DATA_CHUNK;
    buffer[2] = payload_len;

    memcpy(&buffer[3], &offset, 4);
    memcpy(&buffer[7], &len, 2);
    if (len > 0 && data) {
        memcpy(&buffer[9], data, len);
    }

    buffer[3 + payload_len] = calculate_crc8(&buffer[1], 2 + payload_len);
    return 4 + payload_len;
}

int pack_log_delete_req_message(uint8_t *buffer, const char* name) {
    LogDeleteReqMsg msg;
    strncpy(msg.name, name, 31);
    msg.name[31] = 0;

    uint8_t len = sizeof(LogDeleteReqMsg);
    buffer[0] = START_BYTE;
    buffer[1] = CMD_LOG_DELETE_REQ;
    buffer[2] = len;
    memcpy(&buffer[3], &msg, len);
    buffer[3 + len] = calculate_crc8(&buffer[1], 2 + len);
    return 4 + len;
}

int unpack_log_delete_req_message(const uint8_t *buffer, char* name) {
    uint8_t len = buffer[2];
    if (len != sizeof(LogDeleteReqMsg)) return -2;
    uint8_t received_crc = buffer[3 + len];
    uint8_t calculated_crc = calculate_crc8(&buffer[1], 2 + len);
    if (received_crc != calculated_crc) return -1;

    LogDeleteReqMsg msg;
    memcpy(&msg, &buffer[3], len);
    strncpy(name, msg.name, 32);
    return 0;
}

int pack_esp_log_message(uint8_t *buffer, uint8_t level, const char* tag, const char* msg) {
    // [Level:1][TagLen:1][Tag:...][Msg...]
    // Max payload 255.
    uint8_t tagLen = strlen(tag);
    uint8_t msgLen = strlen(msg);
    if (1 + 1 + tagLen + msgLen > 255) {
        // Truncate message
        int avail = 255 - (1 + 1 + tagLen);
        if (avail < 0) return -3;
        msgLen = avail;
    }

    uint8_t payload_len = 1 + 1 + tagLen + msgLen;
    buffer[0] = START_BYTE;
    buffer[1] = CMD_ESP_LOG_DATA;
    buffer[2] = payload_len;

    buffer[3] = level;
    buffer[4] = tagLen;
    memcpy(&buffer[5], tag, tagLen);
    memcpy(&buffer[5+tagLen], msg, msgLen);

    buffer[3 + payload_len] = calculate_crc8(&buffer[1], 2 + payload_len);
    return 4 + payload_len;
}

int pack_simple_cmd_message(uint8_t *buffer, uint8_t cmd) {
    buffer[0] = START_BYTE;
    buffer[1] = cmd;
    buffer[2] = 0;
    buffer[3] = calculate_crc8(&buffer[1], 2);
    return 4;
}

int pack_log_manager_op_message(uint8_t *buffer, uint8_t op) {
    uint8_t len = 1;
    buffer[0] = START_BYTE;
    buffer[1] = CMD_LOG_MANAGER_OP;
    buffer[2] = len;
    buffer[3] = op;
    buffer[3 + len] = calculate_crc8(&buffer[1], 2 + len);
    return 4 + len;
}

int unpack_log_manager_op_message(const uint8_t *buffer, uint8_t *op) {
    uint8_t len = buffer[2];
    if (len != 1) return -2;
    uint8_t received_crc = buffer[3 + len];
    uint8_t calculated_crc = calculate_crc8(&buffer[1], 2 + len);
    if (received_crc != calculated_crc) return -1;
    *op = buffer[3];
    return 0;
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

int pack_connect_device_message(uint8_t *buffer, uint8_t device_type) {
    uint8_t len = 1;
    buffer[0] = START_BYTE;
    buffer[1] = CMD_CONNECT_DEVICE;
    buffer[2] = len;
    buffer[3] = device_type;
    buffer[3 + len] = calculate_crc8(&buffer[1], 2 + len);
    return 4 + len;
}

int unpack_connect_device_message(const uint8_t *buffer, uint8_t *device_type) {
    uint8_t len = buffer[2];
    if (len != 1) return -2;
    uint8_t received_crc = buffer[3 + len];
    uint8_t calculated_crc = calculate_crc8(&buffer[1], 2 + len);
    if (received_crc != calculated_crc) return -1;
    *device_type = buffer[3];
    return 0;
}

int pack_forget_device_message(uint8_t *buffer, uint8_t device_type) {
    uint8_t len = 1;
    buffer[0] = START_BYTE;
    buffer[1] = CMD_FORGET_DEVICE;
    buffer[2] = len;
    buffer[3] = device_type;
    buffer[3 + len] = calculate_crc8(&buffer[1], 2 + len);
    return 4 + len;
}

int unpack_forget_device_message(const uint8_t *buffer, uint8_t *device_type) {
    uint8_t len = buffer[2];
    if (len != 1) return -2;
    uint8_t received_crc = buffer[3 + len];
    uint8_t calculated_crc = calculate_crc8(&buffer[1], 2 + len);
    if (received_crc != calculated_crc) return -1;
    *device_type = buffer[3];
    return 0;
}

int pack_ota_start_message(uint8_t *buffer, uint32_t total_size) {
    uint8_t len = sizeof(OtaStartMsg);
    buffer[0] = START_BYTE;
    buffer[1] = CMD_OTA_START;
    buffer[2] = len;
    OtaStartMsg msg = { total_size };
    memcpy(&buffer[3], &msg, len);
    buffer[3 + len] = calculate_crc8(&buffer[1], 2 + len);
    return 4 + len;
}

int pack_ota_chunk_message(uint8_t *buffer, uint32_t offset, const uint8_t *data, uint8_t len) {
    // [OFFSET(4)][DATA(len)]
    // Payload length = 4 + len. Max 255.
    // So len must be <= 251.
    uint8_t payload_len = 4 + len;
    buffer[0] = START_BYTE;
    buffer[1] = CMD_OTA_CHUNK;
    buffer[2] = payload_len;
    memcpy(&buffer[3], &offset, 4);
    memcpy(&buffer[7], data, len);
    buffer[3 + payload_len] = calculate_crc8(&buffer[1], 2 + payload_len);
    return 4 + payload_len;
}

int pack_ota_end_message(uint8_t *buffer, uint32_t crc32) {
    uint8_t len = 4; // 4 bytes CRC
    buffer[0] = START_BYTE;
    buffer[1] = CMD_OTA_END;
    buffer[2] = len;
    memcpy(&buffer[3], &crc32, 4);
    buffer[3 + len] = calculate_crc8(&buffer[1], 2 + len);
    return 4 + len;
}

int pack_ota_apply_message(uint8_t *buffer) {
    buffer[0] = START_BYTE;
    buffer[1] = CMD_OTA_APPLY;
    buffer[2] = 0;
    buffer[3] = calculate_crc8(&buffer[1], 2);
    return 4;
}
