#include "EcoflowProtocol.h"
#include <cstdint>

// CRC8 implementation (from HA component)
uint8_t crc8(const uint8_t* data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

// Helper struct for encoding a single key-value pair for SetDeviceData
struct SetDataKv {
    const char* key;
    int32_t value;
};

// Nanopb callback for encoding the map field in SetDeviceData
static bool encode_setdata_map_callback(pb_ostream_t *stream, const pb_field_t *field, void * const *arg) {
    SetDataKv* data = (SetDataKv*)(*arg);

    if (!stream || !field || !data || !data->key) {
        return false; // Nothing to encode
    }

    // Create the map entry submessage
    SetDeviceData_ParamsEntry entry = SetDeviceData_ParamsEntry_init_zero;

    // Use a string callback to encode the key
    entry.key.funcs.encode = [](pb_ostream_t *stream, const pb_field_t *field, void * const *arg) -> bool {
        const char* str = (const char*)(*arg);
        if (!pb_encode_tag_for_field(stream, field)) return false;
        return pb_encode_string(stream, (uint8_t*)str, strlen(str));
    };
    entry.key.arg = (void*)data->key;
    entry.value = data->value;

    // Encode the submessage field tag
    if (!pb_encode_tag_for_field(stream, field)) {
        return false;
    }

    // Encode the submessage
    return pb_encode_submessage(stream, SetDeviceData_ParamsEntry_fields, &entry);
}


// Encodes a protobuf message into a buffer
static bool encode_message(pb_ostream_t* stream, const pb_msgdesc_t* fields, const void* src_struct) {
    return pb_encode(stream, fields, src_struct);
}

// Creates a complete command frame
size_t create_command_frame(uint8_t* buffer, size_t buffer_size, const ToDevice& message) {
    if (buffer_size < 11) return 0; // Header(9) + CRC(1) + Tail(1)

    // Create a temporary buffer for the protobuf payload
    uint8_t proto_buffer[256];
    pb_ostream_t stream = pb_ostream_from_buffer(proto_buffer, sizeof(proto_buffer));

    // Encode the protobuf message
    if (!encode_message(&stream, ToDevice_fields, &message)) {
        return 0;
    }
    size_t proto_len = stream.bytes_written;
    if (10 + proto_len > buffer_size) return 0; // Check if it fits

    // Frame Header
    buffer[0] = 0xAA; // Magic
    buffer[1] = 0x02; // Protocol version
    buffer[2] = (uint8_t)proto_len;
    buffer[3] = 0x40; // From App
    buffer[4] = 0;    // Reserved
    buffer[5] = 0;    // Reserved

    // Checksum for header
    uint16_t header_checksum = 0;
    for(int i=0; i<6; i++) header_checksum += buffer[i];
    buffer[6] = header_checksum & 0xFF;
    buffer[7] = (header_checksum >> 8) & 0xFF;

    // Copy protobuf payload
    memcpy(buffer + 8, proto_buffer, proto_len);

    // CRC8 for payload
    buffer[8 + proto_len] = crc8(proto_buffer, proto_len);

    // Frame Tail
    buffer[9 + proto_len] = 0xBB;

    return 10 + proto_len;
}


ToDevice create_heartbeat_message(uint32_t seq) {
    ToDevice msg = ToDevice_init_zero;
    msg.seq = seq;
    msg.which_pdata = ToDevice_heartbeat_tag;
    msg.pdata.heartbeat = Heartbeat_init_zero;
    return msg;
}

ToDevice create_request_data_message(uint32_t seq) {
    ToDevice msg = ToDevice_init_zero;
    msg.seq = seq;
    msg.which_pdata = ToDevice_get_all_data_tag;
    msg.pdata.get_all_data = GetDeviceAllData_init_zero;
    return msg;
}

// This global is not ideal, but it's a simple way to pass the data to the callback
// without dynamic allocation.
static SetDataKv g_setDataKv;

ToDevice create_set_data_message(uint32_t seq, const char* key, int32_t value) {
    ToDevice msg = ToDevice_init_zero;
    msg.seq = seq;
    msg.which_pdata = ToDevice_set_data_tag;
    msg.pdata.set_data = SetDeviceData_init_zero;

    // Set up the data for the callback
    g_setDataKv.key = key;
    g_setDataKv.value = value;

    // Set up the callback for the 'params' map field
    msg.pdata.set_data.params.funcs.encode = encode_setdata_map_callback;
    msg.pdata.set_data.params.arg = &g_setDataKv;

    return msg;
}