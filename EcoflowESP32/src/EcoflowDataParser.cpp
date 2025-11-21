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
    if (pkt.getSrc() == 0x02 && pkt.getCmdSet() == 0xFE && (pkt.getCmdId() == 0x11 || pkt.getCmdId() == 0x15)) {
        ESP_LOGD(TAG, "Parsing data packet with cmdId=0x%02x", pkt.getCmdId());

        // Try parsing as DisplayPropertyUpload
        pd335_sys_DisplayPropertyUpload display_msg = pd335_sys_DisplayPropertyUpload_init_zero;
        pb_istream_t stream = pb_istream_from_buffer(pkt.getPayload().data(), pkt.getPayload().size());

        if (pb_decode(&stream, pd335_sys_DisplayPropertyUpload_fields, &display_msg)) {
            ESP_LOGD(TAG, "Successfully decoded DisplayPropertyUpload");

            // Reliable Battery Percentage
            if (display_msg.has_bms_batt_soc && display_msg.bms_batt_soc > 0) {
                 data.batteryLevel = (int)display_msg.bms_batt_soc;
            } else if (display_msg.has_cms_batt_soc) {
                data.batteryLevel = (int)display_msg.cms_batt_soc;
            }

            // Power readings
            if (display_msg.has_pow_in_sum_w) data.inputPower = (int)display_msg.pow_in_sum_w;
            if (display_msg.has_pow_out_sum_w) data.outputPower = (int)display_msg.pow_out_sum_w;
            if (display_msg.has_pow_get_pv) data.solarInputPower = (int)display_msg.pow_get_pv;
            if (display_msg.has_pow_get_ac_out) data.acOutputPower = (int)display_msg.pow_get_ac_out;
            if (display_msg.has_pow_get_12v) data.dcOutputPower = (int)display_msg.pow_get_12v;
            if (display_msg.has_bms_max_cell_temp) data.cellTemperature = (int)display_msg.bms_max_cell_temp;

            // Status flags
            if (display_msg.has_flow_info_ac_out) data.acOn = (display_msg.flow_info_ac_out & 0b11) >= 0b10;
            if (display_msg.has_flow_info_12v) data.dcOn = (display_msg.flow_info_12v & 0b11) >= 0b10;
            if (display_msg.has_flow_info_qcusb1) data.usbOn = (display_msg.flow_info_qcusb1 & 0b11) >= 0b10;

        } else {
             // If failed, try parsing as RuntimePropertyUpload (which contains more temp fields)
             // Reset stream or creating new one not reliable if data consumed. Recreate.
             stream = pb_istream_from_buffer(pkt.getPayload().data(), pkt.getPayload().size());
             pd335_sys_RuntimePropertyUpload runtime_msg = pd335_sys_RuntimePropertyUpload_init_zero;
             if (pb_decode(&stream, pd335_sys_RuntimePropertyUpload_fields, &runtime_msg)) {
                 ESP_LOGD(TAG, "Successfully decoded RuntimePropertyUpload");

                 // Log Temps for Wave 2 Debugging
                 if (runtime_msg.has_temp_pcs_dc) ESP_LOGI(TAG, "Temp PCS DC: %f", runtime_msg.temp_pcs_dc);
                 if (runtime_msg.has_temp_pcs_ac) ESP_LOGI(TAG, "Temp PCS AC: %f", runtime_msg.temp_pcs_ac);
                 if (runtime_msg.has_temp_pv) ESP_LOGI(TAG, "Temp PV: %f", runtime_msg.temp_pv);
                 if (runtime_msg.has_temp_pv2) ESP_LOGI(TAG, "Temp PV2: %f", runtime_msg.temp_pv2);

                 // If cellTemperature is not set by DisplayProperty, maybe we can get it here?
                 // But Runtime doesn't seem to have cell temp exposed same way usually.
             } else {
                 ESP_LOGE(TAG, "Failed to decode protobuf message as Display or Runtime Property");
             }
        }
    }
}

} // namespace EcoflowDataParser
