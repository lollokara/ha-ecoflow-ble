#include "ecoflow_protocol.h"
#include <string.h>

// CRC-8/ROHC (x^8 + x^2 + x + 1) -> 0x07
uint8_t calculate_crc8(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0;
    while (len--) {
        crc ^= *data++;
        for (uint8_t i = 0; i < 8; i++) {
            if (crc & 0x80) crc = (crc << 1) ^ 0x07;
            else crc <<= 1;
        }
    }
    return crc;
}

static int pack_header(uint8_t *buffer, uint8_t cmd, uint8_t len) {
    buffer[0] = START_BYTE;
    buffer[1] = cmd;
    buffer[2] = len;
    return 3;
}

static int finalize_packet(uint8_t *buffer, int len) {
    buffer[len] = calculate_crc8(&buffer[1], len - 1);
    return len + 1;
}

int pack_handshake_message(uint8_t *buffer) {
    pack_header(buffer, CMD_HANDSHAKE, 0);
    return finalize_packet(buffer, 3);
}

int pack_handshake_ack_message(uint8_t *buffer) {
    pack_header(buffer, CMD_HANDSHAKE_ACK, 0);
    return finalize_packet(buffer, 3);
}

int pack_device_list_message(uint8_t *buffer, const DeviceList *list) {
    int size = sizeof(DeviceList);
    pack_header(buffer, CMD_DEVICE_LIST, size);
    memcpy(&buffer[3], list, size);
    return finalize_packet(buffer, 3 + size);
}

int unpack_device_list_message(const uint8_t *buffer, DeviceList *list) {
    if (buffer[2] != sizeof(DeviceList)) return -1;
    memcpy(list, &buffer[3], sizeof(DeviceList));
    return 0;
}

int pack_device_list_ack_message(uint8_t *buffer) {
    pack_header(buffer, CMD_DEVICE_LIST_ACK, 0);
    return finalize_packet(buffer, 3);
}

int pack_get_device_status_message(uint8_t *buffer, uint8_t device_id) {
    pack_header(buffer, CMD_GET_DEVICE_STATUS, 1);
    buffer[3] = device_id;
    return finalize_packet(buffer, 4);
}

int unpack_get_device_status_message(const uint8_t *buffer, uint8_t *device_id) {
    if (buffer[2] != 1) return -1;
    *device_id = buffer[3];
    return 0;
}

int pack_device_status_message(uint8_t *buffer, const DeviceStatus *status) {
    int size = sizeof(DeviceStatus);
    pack_header(buffer, CMD_DEVICE_STATUS, size);
    memcpy(&buffer[3], status, size);
    return finalize_packet(buffer, 3 + size);
}

int unpack_device_status_message(const uint8_t *buffer, DeviceStatus *status) {
    if (buffer[2] != sizeof(DeviceStatus)) return -1;
    memcpy(status, &buffer[3], sizeof(DeviceStatus));
    return 0;
}

int pack_set_wave2_message(uint8_t *buffer, uint8_t type, uint8_t value) {
    pack_header(buffer, CMD_SET_WAVE2, 2);
    buffer[3] = type;
    buffer[4] = value;
    return finalize_packet(buffer, 5);
}

int unpack_set_wave2_message(const uint8_t *buffer, uint8_t *type, uint8_t *value) {
    if (buffer[2] != 2) return -1;
    *type = buffer[3];
    *value = buffer[4];
    return 0;
}

int pack_set_ac_message(uint8_t *buffer, uint8_t enable) {
    pack_header(buffer, CMD_SET_AC, 1);
    buffer[3] = enable;
    return finalize_packet(buffer, 4);
}

int unpack_set_ac_message(const uint8_t *buffer, uint8_t *enable) {
    if (buffer[2] != 1) return -1;
    *enable = buffer[3];
    return 0;
}

int pack_set_dc_message(uint8_t *buffer, uint8_t enable) {
    pack_header(buffer, CMD_SET_DC, 1);
    buffer[3] = enable;
    return finalize_packet(buffer, 4);
}

int unpack_set_dc_message(const uint8_t *buffer, uint8_t *enable) {
    if (buffer[2] != 1) return -1;
    *enable = buffer[3];
    return 0;
}

int pack_set_value_message(uint8_t *buffer, uint8_t type, int value) {
    pack_header(buffer, CMD_SET_VALUE, 5);
    buffer[3] = type;
    memcpy(&buffer[4], &value, 4);
    return finalize_packet(buffer, 9);
}

int unpack_set_value_message(const uint8_t *buffer, uint8_t *type, int *value) {
    if (buffer[2] != 5) return -1;
    *type = buffer[3];
    memcpy(value, &buffer[4], 4);
    return 0;
}

int pack_power_off_message(uint8_t *buffer) {
    pack_header(buffer, CMD_POWER_OFF, 0);
    return finalize_packet(buffer, 3);
}

int pack_get_debug_info_message(uint8_t *buffer) {
    pack_header(buffer, CMD_GET_DEBUG_INFO, 0);
    return finalize_packet(buffer, 3);
}

int pack_debug_info_message(uint8_t *buffer, const DebugInfo *info) {
    int size = sizeof(DebugInfo);
    pack_header(buffer, CMD_DEBUG_INFO, size);
    memcpy(&buffer[3], info, size);
    return finalize_packet(buffer, 3 + size);
}

int unpack_debug_info_message(const uint8_t *buffer, DebugInfo *info) {
    if (buffer[2] != sizeof(DebugInfo)) return -1;
    memcpy(info, &buffer[3], sizeof(DebugInfo));
    return 0;
}

int pack_connect_device_message(uint8_t *buffer, uint8_t device_type) {
    pack_header(buffer, CMD_CONNECT_DEVICE, 1);
    buffer[3] = device_type;
    return finalize_packet(buffer, 4);
}

int unpack_connect_device_message(const uint8_t *buffer, uint8_t *device_type) {
    if (buffer[2] != 1) return -1;
    *device_type = buffer[3];
    return 0;
}

int pack_forget_device_message(uint8_t *buffer, uint8_t device_type) {
    pack_header(buffer, CMD_FORGET_DEVICE, 1);
    buffer[3] = device_type;
    return finalize_packet(buffer, 4);
}

int unpack_forget_device_message(const uint8_t *buffer, uint8_t *device_type) {
    if (buffer[2] != 1) return -1;
    *device_type = buffer[3];
    return 0;
}

int pack_ota_start_message(uint8_t *buffer, uint32_t total_size) {
    pack_header(buffer, CMD_OTA_START, 4);
    memcpy(&buffer[3], &total_size, 4);
    return finalize_packet(buffer, 7);
}

int pack_ota_chunk_message(uint8_t *buffer, uint32_t offset, const uint8_t *data, uint8_t len) {
    pack_header(buffer, CMD_OTA_CHUNK, 4 + len);
    memcpy(&buffer[3], &offset, 4);
    memcpy(&buffer[7], data, len);
    return finalize_packet(buffer, 3 + 4 + len);
}

int pack_ota_end_message(uint8_t *buffer, uint32_t crc32) {
    pack_header(buffer, CMD_OTA_END, 4);
    memcpy(&buffer[3], &crc32, 4);
    return finalize_packet(buffer, 7);
}

int pack_ota_apply_message(uint8_t *buffer) {
    pack_header(buffer, CMD_OTA_APPLY, 0);
    return finalize_packet(buffer, 3);
}

// Log API
int pack_log_list_req_message(uint8_t *buffer) {
    pack_header(buffer, CMD_LOG_LIST_REQ, 0);
    return finalize_packet(buffer, 3);
}

int pack_log_list_resp_message(uint8_t *buffer, uint8_t total, uint8_t index, uint32_t size, const char* name) {
    uint8_t nameLen = strlen(name);
    if(nameLen > 31) nameLen = 31;

    // total(1) + index(1) + size(4) + nameLen(1) + name(N)
    uint8_t plLen = 1 + 1 + 4 + 1 + nameLen;
    pack_header(buffer, CMD_LOG_LIST_RESP, plLen);

    buffer[3] = total;
    buffer[4] = index;
    memcpy(&buffer[5], &size, 4);
    buffer[9] = nameLen;
    memcpy(&buffer[10], name, nameLen);

    return finalize_packet(buffer, 3 + plLen);
}

int unpack_log_list_resp_message(const uint8_t *buffer, uint8_t *total, uint8_t *index, uint32_t *size, char* name) {
    // Basic check for min len (at least header + 7)
    if (buffer[2] < 7) return -1;

    *total = buffer[3];
    *index = buffer[4];
    memcpy(size, &buffer[5], 4);
    uint8_t nameLen = buffer[9];
    if (nameLen > 31) nameLen = 31;
    memcpy(name, &buffer[10], nameLen);
    name[nameLen] = 0;

    return 0;
}

int pack_log_download_req_message(uint8_t *buffer, const char* name) {
    uint8_t len = strlen(name);
    if (len > 31) len = 31;
    pack_header(buffer, CMD_LOG_DOWNLOAD_REQ, len);
    memcpy(&buffer[3], name, len);
    return finalize_packet(buffer, 3 + len);
}

int unpack_log_download_req_message(const uint8_t *buffer, char* name) {
    uint8_t len = buffer[2];
    if (len > 31) len = 31;
    memcpy(name, &buffer[3], len);
    name[len] = 0;
    return 0;
}

int pack_log_data_chunk_message(uint8_t *buffer, uint32_t offset, const uint8_t* data, uint16_t len) {
    // offset(4) + len(2) + data(N)
    // Packet len is byte, max 255.
    // Payload max 255. Header 4+2=6. Data max 249.
    if(len > 240) len = 240;

    pack_header(buffer, CMD_LOG_DATA_CHUNK, 6 + len);
    memcpy(&buffer[3], &offset, 4);
    memcpy(&buffer[7], &len, 2);
    if(len > 0) memcpy(&buffer[9], data, len);

    return finalize_packet(buffer, 3 + 6 + len);
}

int pack_log_delete_req_message(uint8_t *buffer, const char* name) {
    uint8_t len = strlen(name);
    if (len > 31) len = 31;
    pack_header(buffer, CMD_LOG_DELETE_REQ, len);
    memcpy(&buffer[3], name, len);
    return finalize_packet(buffer, 3 + len);
}

int unpack_log_delete_req_message(const uint8_t *buffer, char* name) {
    uint8_t len = buffer[2];
    if (len > 31) len = 31;
    memcpy(name, &buffer[3], len);
    name[len] = 0;
    return 0;
}

int pack_esp_log_message(uint8_t *buffer, uint8_t level, const char* tag, const char* msg) {
    // Deprecated by raw string approach but keeping for compatibility if needed.
    // Switching to raw string per new design.
    // [Level:1][TagLen:1][Tag][Msg]
    size_t tagLen = strlen(tag); if(tagLen > 31) tagLen = 31;
    size_t msgLen = strlen(msg);
    size_t totalLen = 1 + 1 + tagLen + msgLen;
    if(totalLen > 250) {
        msgLen = 250 - (1 + 1 + tagLen);
        totalLen = 250;
    }

    pack_header(buffer, CMD_ESP_LOG_DATA, totalLen);
    buffer[3] = level;
    buffer[4] = (uint8_t)tagLen;
    memcpy(&buffer[5], tag, tagLen);
    memcpy(&buffer[5+tagLen], msg, msgLen);

    return finalize_packet(buffer, 3 + totalLen);
}

int pack_simple_cmd_message(uint8_t *buffer, uint8_t cmd) {
    pack_header(buffer, cmd, 0);
    return finalize_packet(buffer, 3);
}

int pack_log_manager_op_message(uint8_t *buffer, uint8_t op) {
    pack_header(buffer, CMD_LOG_MANAGER_OP, 1);
    buffer[3] = op;
    return finalize_packet(buffer, 4);
}

int unpack_log_manager_op_message(const uint8_t *buffer, uint8_t *op) {
    if (buffer[2] != 1) return -1;
    *op = buffer[3];
    return 0;
}
