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

// Helpers for binary parsing (little-endian)
static uint16_t get_uint16_le(const uint8_t* data) {
    return data[0] | (data[1] << 8);
}

static uint32_t get_uint32_le(const uint8_t* data) {
    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

static int16_t get_int16_le(const uint8_t* data) {
    return (int16_t)(data[0] | (data[1] << 8));
}

static int16_t swap_endian_and_parse_signed_int(const uint8_t* data) {
    return (int16_t)(data[0] | (data[1] << 8));
}


static float get_float_le(const uint8_t* data) {
    uint32_t i = get_uint32_le(data);
    float f;
    memcpy(&f, &i, sizeof(f));
    return f;
}

namespace EcoflowDataParser {

void parsePacket(const Packet& pkt, EcoflowData& data) {

    // V3 Protobuf Packet (Delta 2 / D3)
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

            // SOC Limits
            if (display_msg.has_cms_max_chg_soc) data.maxChgSoc = (int)display_msg.cms_max_chg_soc;
            if (display_msg.has_cms_min_dsg_soc) data.minDsgSoc = (int)display_msg.cms_min_dsg_soc;

            // AC Charging Limit
            if (display_msg.has_plug_in_info_ac_in_chg_pow_max) data.acChgLimit = (int)display_msg.plug_in_info_ac_in_chg_pow_max;

        } else {
             // If failed, try parsing as RuntimePropertyUpload (which contains more temp fields)
             stream = pb_istream_from_buffer(pkt.getPayload().data(), pkt.getPayload().size());
             pd335_sys_RuntimePropertyUpload runtime_msg = pd335_sys_RuntimePropertyUpload_init_zero;
             if (pb_decode(&stream, pd335_sys_RuntimePropertyUpload_fields, &runtime_msg)) {
                 ESP_LOGD(TAG, "Successfully decoded RuntimePropertyUpload");

                 // Log Temps for Wave 2 Debugging
                 if (runtime_msg.has_temp_pcs_dc) ESP_LOGI(TAG, "Temp PCS DC: %f", runtime_msg.temp_pcs_dc);
                 if (runtime_msg.has_temp_pcs_ac) ESP_LOGI(TAG, "Temp PCS AC: %f", runtime_msg.temp_pcs_ac);
                 if (runtime_msg.has_temp_pv) ESP_LOGI(TAG, "Temp PV: %f", runtime_msg.temp_pv);
                 if (runtime_msg.has_temp_pv2) ESP_LOGI(TAG, "Temp PV2: %f", runtime_msg.temp_pv2);
             } else {
                 ESP_LOGE(TAG, "Failed to decode protobuf message as Display or Runtime Property");
             }
        }
    }

    // V2 Binary Packet (Wave 2 / KT210)
    else if (pkt.getCmdSet() == 0x42 && pkt.getCmdId() == 0x50) {
        ESP_LOGD(TAG, "Parsing Wave 2 (V2) Status packet");
        const std::vector<uint8_t>& payload = pkt.getPayload();

        if (payload.size() >= 108) {
            const uint8_t* p = payload.data();

            int16_t psdr_pwr = swap_endian_and_parse_signed_int(p + 61);
            int16_t bat_pwr = swap_endian_and_parse_signed_int(p + 63);
            int16_t mppt_pwr = swap_endian_and_parse_signed_int(p + 65);
            uint8_t bat_soc = p[75];
            float outlet_temp = get_float_le(p + 77);

            data.batteryLevel = bat_soc;
            data.solarInputPower = mppt_pwr;

            data.mode = p[0];
            data.subMode = p[1];
            data.setTemp = p[2];
            data.fanSpeed = p[3];
            data.currentTemp = (int)get_float_le(p + 4); // Env temp

            uint32_t charge_rem = get_uint32_le(p + 71);
            uint32_t discharge_rem = get_uint32_le(p + 67);
            if (charge_rem > 0 && charge_rem < 6000) data.remainingTime = charge_rem;
            else if (discharge_rem > 0 && discharge_rem < 6000) data.remainingTime = discharge_rem;
            else data.remainingTime = 0;

            if (bat_pwr > 0) {
                data.inputPower = bat_pwr;
                data.outputPower = 0;
            } else {
                data.inputPower = 0;
                data.outputPower = abs(bat_pwr);
            }

            data.dcOutputPower = psdr_pwr;
            data.cellTemperature = (int)outlet_temp;

            data.acOn = (p[0] != 0);
            data.dcOn = (p[59] != 0);

            ESP_LOGI(TAG, "Wave 2: SOC=%d%% BatPwr=%dW Solar=%dW Temp=%.1f Mode=%d Fan=%d", data.batteryLevel, bat_pwr, mppt_pwr, outlet_temp, data.mode, data.fanSpeed);
        }
    }
}

} // namespace EcoflowDataParser
