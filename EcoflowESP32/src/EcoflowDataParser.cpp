#include "EcoflowDataParser.h"
#include "pb_utils.h"
#include "pd335_sys.pb.h"
#include <Arduino.h>
#include "esp_log.h"

static const char* TAG = "EcoflowDataParser";

bool pb_decode_string(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    return pb_read(stream, NULL, stream->bytes_left);
}

bool pb_decode_to_vector(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    std::vector<uint8_t> *vec = (std::vector<uint8_t> *)*arg;
    vec->resize(stream->bytes_left);
    return pb_read(stream, vec->data(), stream->bytes_left);
}

namespace EcoflowDataParser {

void parsePacket(const Packet& pkt, EcoflowData& data) {
    if (pkt.getSrc() == 0x02 && pkt.getCmdSet() == 0xFE && (pkt.getCmdId() == 0x11 || pkt.getCmdId() == 0x15)) {
        ESP_LOGD(TAG, "Parsing data packet with cmdId=0x%02x", pkt.getCmdId());
        pd335_sys_DisplayPropertyUpload proto_msg = pd335_sys_DisplayPropertyUpload_init_zero;
        pb_istream_t stream = pb_istream_from_buffer(pkt.getPayload().data(), pkt.getPayload().size());
        if (pb_decode(&stream, pd335_sys_DisplayPropertyUpload_fields, &proto_msg)) {
            ESP_LOGD(TAG, "Successfully decoded protobuf message");

            // Reliable Battery Percentage
            if (proto_msg.has_bms_batt_soc && proto_msg.bms_batt_soc > 0) {
                 data.batteryLevel = (int)proto_msg.bms_batt_soc;
                 ESP_LOGD(TAG, "Battery level (BMS): %d%%", data.batteryLevel);
            } else if (proto_msg.has_cms_batt_soc) {
                data.batteryLevel = (int)proto_msg.cms_batt_soc;
                ESP_LOGD(TAG, "Battery level (CMS): %d%%", data.batteryLevel);
            }

            // Power readings
            if (proto_msg.has_pow_in_sum_w) {
                data.inputPower = (int)proto_msg.pow_in_sum_w;
            }
            if (proto_msg.has_pow_out_sum_w) {
                data.outputPower = (int)proto_msg.pow_out_sum_w;
            }

            // Solar Input Power (W)
            if (proto_msg.has_pow_get_pv) {
                data.solarInputPower = (int)proto_msg.pow_get_pv;
            }

            // AC Output Power (W)
            if (proto_msg.has_pow_get_ac_out) {
                data.acOutputPower = (int)proto_msg.pow_get_ac_out;
            }

            // DC Output Power (W)
            if (proto_msg.has_pow_get_12v) {
                data.dcOutputPower = (int)proto_msg.pow_get_12v;
            }

            // Cell Temperature (C)
            if (proto_msg.has_bms_max_cell_temp) {
                data.cellTemperature = (int)proto_msg.bms_max_cell_temp;
            }

            // Limits
            if (proto_msg.has_cms_max_chg_soc) {
                data.maxChargeLevel = (int)proto_msg.cms_max_chg_soc;
            }
            if (proto_msg.has_cms_min_dsg_soc) {
                data.minDischargeLevel = (int)proto_msg.cms_min_dsg_soc;
            }

            // Status flags
            if (proto_msg.has_flow_info_ac_out) {
                data.acOn = (proto_msg.flow_info_ac_out & 0b11) >= 0b10;
            }
            if (proto_msg.has_flow_info_12v) {
                data.dcOn = (proto_msg.flow_info_12v & 0b11) >= 0b10;
            }
            if (proto_msg.has_flow_info_qcusb1) {
                 data.usbOn = (proto_msg.flow_info_qcusb1 & 0b11) >= 0b10;
            }

        } else {
            ESP_LOGE(TAG, "Failed to decode protobuf message");
        }
    }
}

} // namespace EcoflowDataParser
