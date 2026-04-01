#include "EcoflowDataParser.h"
#include "pb_utils.h"
#include "utc_sys.pb.h"
#include <Arduino.h>

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

bool parsePacket(const Packet& pkt, EcoflowData& data) {
    if (pkt.getSrc() == 0x02 && pkt.getCmdSet() == 0xFE && pkt.getCmdId() == 0x15) {
        pd335_sys_DisplayPropertyUpload proto_msg = pd335_sys_DisplayPropertyUpload_init_zero;
        pb_istream_t stream = pb_istream_from_buffer(pkt.getPayload().data(), pkt.getPayload().size());

        // Custom decoders for variable length fields
        // proto_msg.plug_in_info_pv_chg_max_list.pv_chg_max_item.funcs.decode = &pb_decode_to_vector;
        // proto_msg.pv_dc_chg_setting_list.list_info.funcs.decode = &pb_decode_to_vector;

        if (pb_decode(&stream, pd335_sys_DisplayPropertyUpload_fields, &proto_msg)) {
            data.batteryLevel = proto_msg.cms_batt_soc;
            data.inputPower = proto_msg.pow_in_sum_w;
            data.outputPower = proto_msg.pow_out_sum_w;
            data.acOn = (proto_msg.flow_info_ac_out & 0b11) >= 0b10;
            data.dcOn = (proto_msg.flow_info_12v & 0b11) >= 0b10;
            return true;
        }
    }
    return false;
}

} // namespace EcoflowDataParser
