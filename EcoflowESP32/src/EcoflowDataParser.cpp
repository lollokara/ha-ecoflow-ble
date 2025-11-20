#include "EcoflowDataParser.h"
#include "pb_utils.h"
#include "pd335_sys.pb.h"
#include <Arduino.h>
#include "esp_log.h"

static const char* TAG = "EcoflowDataParser";

bool pb_decode_string(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    // We don't need to actually decode strings for this application
    return pb_read(stream, NULL, stream->bytes_left);
}

bool pb_decode_to_vector(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    std::vector<uint8_t> *vec = (std::vector<uint8_t> *)*arg;
    vec->resize(stream->bytes_left);
    return pb_read(stream, vec->data(), stream->bytes_left);
}

namespace EcoflowDataParser {

void parsePacket(const Packet& pkt, EcoflowData& data) {
    if (pkt.getSrc() == 0x02 && pkt.getCmdSet() == 0xFE && pkt.getCmdId() == 0x11) {
        pd335_sys_DisplayPropertyUpload proto_msg = pd335_sys_DisplayPropertyUpload_init_zero;
        pb_istream_t stream = pb_istream_from_buffer(pkt.getPayload().data(), pkt.getPayload().size());
        if (pb_decode(&stream, pd335_sys_DisplayPropertyUpload_fields, &proto_msg)) {
            ESP_LOGI(TAG, "Successfully decoded protobuf message");
            data.batteryLevel = proto_msg.cms_batt_soc;
            ESP_LOGI(TAG, "Battery level: %d%%", data.batteryLevel);
            data.inputPower = proto_msg.pow_in_sum_w;
            data.outputPower = proto_msg.pow_out_sum_w;
            data.acOn = (proto_msg.flow_info_ac_out & 0b11) >= 0b10;
            data.dcOn = (proto_msg.flow_info_12v & 0b11) >= 0b10;
        } else {
            ESP_LOGE(TAG, "Failed to decode protobuf message");
        }
    }
}

} // namespace EcoflowDataParser
