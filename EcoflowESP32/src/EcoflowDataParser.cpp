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
    // Python: swapped = bytes([data[1], data[0]])
    // Check if first hex digit < 8 (positive)
    // If negative, extend sign.
    // This seems to be big-endian signed short but with a twist?
    // Or just Big Endian?
    // Python: int.from_bytes(swapped, byteorder='big', signed=True) ? No, it converts hex.
    // Let's follow logic:
    // data[0] is LSB in the stream?
    // The function name says "swap endian", usually implies LE to BE or vice versa.
    // If input is [0x01, 0x02], swapped is [0x02, 0x01]. Hex "0201".
    // If 0x02 < 8 -> Positive. Value 0x0201 = 513.
    // This looks like the data on wire is Little Endian, but treated as Big Endian after swap?
    // Wait. If data on wire is LE: [Lo, Hi].
    // Swapped: [Hi, Lo].
    // Hex string of [Hi, Lo] is "HiLo".
    // int("HiLo", 16) is value.
    // So this effectively reads it as Big Endian from the swapped bytes...
    // WHICH MEANS: The original data [Lo, Hi] IS Little Endian representation of the value.
    // Example: Value 10 (0x000A). LE: [0x0A, 0x00].
    // Swap: [0x00, 0x0A]. Hex: "000A". Int: 10. Correct.
    // Example: -10. 16-bit signed. 0xFFF6. LE: [0xF6, 0xFF].
    // Swap: [0xFF, 0xF6]. Hex: "FFF6".
    // First digit 'F' >= 8.
    // int.from_bytes(bytes.fromhex('FFFF' + 'FFF6'), byteorder='big', signed=True).
    // 'FFFFFFF6' -> -10.

    // CONCLUSION: It is simply signed 16-bit Little Endian.
    // standard (int16_t)(data[0] | data[1]<<8) does exactly this on a LE machine (ESP32).
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
    // cmdSet=0x42 (66), cmdId=0x50 (80) - Status Data (108 bytes)
    else if (pkt.getCmdSet() == 0x42 && pkt.getCmdId() == 0x50) {
        ESP_LOGD(TAG, "Parsing Wave 2 (V2) Status packet");
        const std::vector<uint8_t>& payload = pkt.getPayload();

        if (payload.size() >= 108) {
            const uint8_t* p = payload.data();

            // Mapping based on kt210_ble_parser.py

            // 0: mode (1)
            // 1: sub_mode (1)
            // 2: set_temp (1)
            // 3: fan_value (1)
            // 4: env_temp (4, float)
            // 8: temp_sys (1)
            // 9: display_idle_time (2)
            // 11: display_idle_mode (1)
            // 12: time_en (1)
            // 13: time_set_val (2)
            // 15: time_remain_val (2)
            // 17: beep_enable (1)
            // 18: err_code (4)
            // 22: name (32, string)
            // 54: ref_en (1)
            // 55: bms_pid (2)
            // 57: wte_fth_en (1)
            // 58: temp_display (1)
            // 59: power_mode (1)
            // 60: power_src (1)

            // 61: psdr_pwr_watt (2, signed_int) -> Output Power?
            int16_t psdr_pwr = swap_endian_and_parse_signed_int(p + 61);

            // 63: bat_pwr_watt (2, signed_int) -> Battery Power (+ charge, - discharge)
            int16_t bat_pwr = swap_endian_and_parse_signed_int(p + 63);

            // 65: mptt_pwr_watt (2, signed_int) -> Solar Input
            int16_t mppt_pwr = swap_endian_and_parse_signed_int(p + 65);

            // 67: bat_dsg_remain_time (4)
            // 71: bat_chg_remain_time (4)

            // 75: bat_soc (1)
            uint8_t bat_soc = p[75];

            // 76: bat_chg_status (1)
            // 77: out_let_temp (4, float)
            float outlet_temp = get_float_le(p + 77);

            // 81: mppt_work (1)
            // 82: bms_err (1)
            // 83: rgb_state (1)
            // 84: water_value (1)
            // 85: bms_bound_flag (1)
            // 86: bms_undervoltage (1)
            // 87: ver (1)
            // 88: resv (20)

            // Populate EcoflowData
            data.batteryLevel = bat_soc;
            data.solarInputPower = mppt_pwr;

            // Wave 2 specific fields
            data.mode = p[0];
            data.subMode = p[1];
            data.setTemp = p[2];
            data.fanSpeed = p[3];
            data.currentTemp = (int)get_float_le(p + 4); // Env temp

            // Remaining Time Logic (simplified)
            uint32_t charge_rem = get_uint32_le(p + 71);
            uint32_t discharge_rem = get_uint32_le(p + 67);
            if (charge_rem > 0 && charge_rem < 6000) data.remainingTime = charge_rem; // Max ~100h check
            else if (discharge_rem > 0 && discharge_rem < 6000) data.remainingTime = discharge_rem;
            else data.remainingTime = 0;

            // Logic for Input/Output power based on battery power flow
            if (bat_pwr > 0) {
                // Charging
                data.inputPower = bat_pwr;
                data.outputPower = 0;
            } else {
                // Discharging
                data.inputPower = 0;
                data.outputPower = abs(bat_pwr);
            }

            // Use PSDR power as generic AC/DC output sum if suitable?
            // For Wave 2, it's a compressor load.
            data.dcOutputPower = psdr_pwr; // Assume compressor/device load is DC-like internally?

            data.cellTemperature = (int)outlet_temp; // Map outlet temp to cell temp for display?
            // Or use env_temp? Let's stick to outlet for now as it's "Device Temp"

            // Status Flags
            // Wave 2 doesn't have simple AC/DC toggles like Delta.
            // Map "power_mode" or "mode" to AC On?
            data.acOn = (p[0] != 0); // If mode is not 0 (OFF), assume ON?
            data.dcOn = (p[59] != 0); // Power Mode?

            ESP_LOGI(TAG, "Wave 2: SOC=%d%% BatPwr=%dW Solar=%dW Temp=%.1f Mode=%d Fan=%d", data.batteryLevel, bat_pwr, mppt_pwr, outlet_temp, data.mode, data.fanSpeed);
        }
    }
}

} // namespace EcoflowDataParser
