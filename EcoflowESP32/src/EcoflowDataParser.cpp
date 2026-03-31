#include "EcoflowDataParser.h"
#include "pb_utils.h"
#include "pd335_sys.pb.h"
#include "mr521.pb.h"
#include "dc009_apl_comm.pb.h"
#include <Arduino.h>
#include "Logging.h"
#include <cmath>
#include <algorithm>

static const char* TAG = "EcoflowDataParser";
static uint32_t currentDumpId = 0;

void EcoflowDataParser::triggerDebugDump() {
    currentDumpId++;
}

// --- Helper Functions ---

bool pb_decode_string(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    // We don't need to actually decode strings for this application, just skip
    return pb_read(stream, NULL, stream->bytes_left);
}

bool pb_decode_to_vector(pb_istream_t *stream, const pb_field_t *field, void **arg) {
    std::vector<uint8_t> *vec = (std::vector<uint8_t> *)*arg;
    vec->resize(stream->bytes_left);
    return pb_read(stream, vec->data(), stream->bytes_left);
}

static uint16_t get_uint16_le(const uint8_t* data) {
    return data[0] | (data[1] << 8);
}

static uint32_t get_uint32_le(const uint8_t* data) {
    return data[0] | (data[1] << 8) | (data[2] << 16) | (data[3] << 24);
}

static int16_t swap_endian_and_parse_signed_int(const uint8_t* data) {
    // Little Endian Signed Short
    int16_t val = (int16_t)(data[0] | (data[1] << 8));
    return val;
}

static float get_float_le_safe(const uint8_t* data) {
    uint32_t i = get_uint32_le(data);
    float f;
    memcpy(&f, &i, sizeof(f));
    if (std::isnan(f) || std::isinf(f)) return 0.0f;
    return f;
}

static bool is_flow_on(uint32_t x) {
    return (x & 0b11) >= 0b10;
}

static void logFullDelta3Data(const pd335_sys_DisplayPropertyUpload& msg) {
    LOG_STM_I(TAG, "--- Full Delta 3 Dump ---");
    if (msg.has_cms_batt_soc) LOG_STM_I(TAG, "cms_batt_soc: %.2f", msg.cms_batt_soc);
    if (msg.has_bms_batt_soc) LOG_STM_I(TAG, "bms_batt_soc: %.2f", msg.bms_batt_soc);
    if (msg.has_pow_get_ac_in) LOG_STM_I(TAG, "pow_get_ac_in: %.2f", msg.pow_get_ac_in);
    if (msg.has_pow_get_ac_out) LOG_STM_I(TAG, "pow_get_ac_out: %.2f", msg.pow_get_ac_out);
    if (msg.has_pow_in_sum_w) LOG_STM_I(TAG, "pow_in_sum_w: %.2f", msg.pow_in_sum_w);
    if (msg.has_pow_out_sum_w) LOG_STM_I(TAG, "pow_out_sum_w: %.2f", msg.pow_out_sum_w);
    if (msg.has_pow_get_12v) LOG_STM_I(TAG, "pow_get_12v: %.2f", msg.pow_get_12v);
    if (msg.has_pow_get_pv) LOG_STM_I(TAG, "pow_get_pv: %.2f", msg.pow_get_pv);
    if (msg.has_plug_in_info_pv_type) LOG_STM_I(TAG, "plug_in_info_pv_type: %d", (int)msg.plug_in_info_pv_type);
    if (msg.has_pow_get_typec1) LOG_STM_I(TAG, "pow_get_typec1: %.2f", msg.pow_get_typec1);
    if (msg.has_pow_get_typec2) LOG_STM_I(TAG, "pow_get_typec2: %.2f", msg.pow_get_typec2);
    if (msg.has_pow_get_qcusb1) LOG_STM_I(TAG, "pow_get_qcusb1: %.2f", msg.pow_get_qcusb1);
    if (msg.has_pow_get_qcusb2) LOG_STM_I(TAG, "pow_get_qcusb2: %.2f", msg.pow_get_qcusb2);
    if (msg.has_plug_in_info_ac_charger_flag) LOG_STM_I(TAG, "plug_in_info_ac_charger_flag: %d", (int)msg.plug_in_info_ac_charger_flag);
    if (msg.has_energy_backup_en) LOG_STM_I(TAG, "energy_backup_en: %d", (int)msg.energy_backup_en);
    if (msg.has_energy_backup_start_soc) LOG_STM_I(TAG, "energy_backup_start_soc: %d", (int)msg.energy_backup_start_soc);
    if (msg.has_pow_get_bms) LOG_STM_I(TAG, "pow_get_bms: %.2f", msg.pow_get_bms);
    if (msg.has_cms_min_dsg_soc) LOG_STM_I(TAG, "cms_min_dsg_soc: %d", (int)msg.cms_min_dsg_soc);
    if (msg.has_cms_max_chg_soc) LOG_STM_I(TAG, "cms_max_chg_soc: %d", (int)msg.cms_max_chg_soc);
    if (msg.has_bms_max_cell_temp) LOG_STM_I(TAG, "bms_max_cell_temp: %d", (int)msg.bms_max_cell_temp);
    if (msg.has_flow_info_12v) LOG_STM_I(TAG, "flow_info_12v: %d", (int)msg.flow_info_12v);
    if (msg.has_flow_info_ac_out) LOG_STM_I(TAG, "flow_info_ac_out: %d", (int)msg.flow_info_ac_out);
    if (msg.has_plug_in_info_ac_in_chg_pow_max) LOG_STM_I(TAG, "plug_in_info_ac_in_chg_pow_max: %d", (int)msg.plug_in_info_ac_in_chg_pow_max);
    if (msg.has_flow_info_qcusb1) LOG_STM_I(TAG, "flow_info_qcusb1: %d", (int)msg.flow_info_qcusb1);
}

static void logDelta3Data(const Delta3Data& d) {
    LOG_STM_I(TAG, "--- Delta 3 Summary ---");
    LOG_STM_I(TAG, "Batt: %.1f%% (In: %.1fW, Out: %.1fW)", d.batteryLevel, d.batteryInputPower, d.batteryOutputPower);
    LOG_STM_I(TAG, "AC In: %.1fW, AC Out: %.1fW", d.acInputPower, d.acOutputPower);
    LOG_STM_I(TAG, "DC In: %.1fW (State: %d, Solar: %.1fW)", d.dcPortInputPower, d.dcPortState, d.solarInputPower);
    LOG_STM_I(TAG, "Total In: %.1fW, Out: %.1fW", d.inputPower, d.outputPower);
    LOG_STM_I(TAG, "12V Out: %.1fW, USB-C: %.1fW/%.1fW, USB-A: %.1fW/%.1fW",
             d.dc12vOutputPower, d.usbcOutputPower, d.usbc2OutputPower, d.usbaOutputPower, d.usba2OutputPower);
    LOG_STM_I(TAG, "SOC Limits: %d%% - %d%%, AC Chg Speed: %dW", d.batteryChargeLimitMin, d.batteryChargeLimitMax, d.acChargingSpeed);
    LOG_STM_I(TAG, "Flags: AC Plugged=%d, Backup=%d, AC Ports=%d, 12V Port=%d", d.pluggedInAc, d.energyBackup, d.acPorts, d.dc12vPort);
}

static void logWave2Data(const Wave2Data& w) {
    LOG_STM_I(TAG, "--- Full Wave 2 Dump ---");
    LOG_STM_I(TAG, "Mode: %d (Sub: %d), PwrMode: %d", w.mode, w.subMode, w.powerMode);
    LOG_STM_I(TAG, "Temps: Env=%.2f, Out=%.2f, Set=%d", w.envTemp, w.outLetTemp, w.setTemp);
    LOG_STM_I(TAG, "Batt: %d%% (Stat: %d), Rem: %dm/%dm", w.batSoc, w.batChgStatus, w.batChgRemainTime, w.batDsgRemainTime);
    LOG_STM_I(TAG, "Power: Bat=%dW, MPPT=%dW, PSDR=%dW", w.batPwrWatt, w.mpptPwrWatt, w.psdrPwrWatt);
    LOG_STM_I(TAG, "Fan: %d, Water: %d, RGB: %d", w.fanValue, w.waterValue, w.rgbState);
    LOG_STM_I(TAG, "Err: %u, BMS Err: %d", w.errCode, w.bmsErr);
}

static void logFullDeltaPro3Data(const mr521_DisplayPropertyUpload& msg) {
    LOG_STM_I(TAG, "--- Full D3P Dump ---");
    if (msg.has_errcode) LOG_STM_I(TAG, "errcode: %d", (int)msg.errcode);
    if (msg.has_sys_status) LOG_STM_I(TAG, "sys_status: %d", (int)msg.sys_status);
    if (msg.has_pow_in_sum_w) LOG_STM_I(TAG, "pow_in_sum_w: %.2f", msg.pow_in_sum_w);
    if (msg.has_pow_out_sum_w) LOG_STM_I(TAG, "pow_out_sum_w: %.2f", msg.pow_out_sum_w);
    if (msg.has_lcd_light) LOG_STM_I(TAG, "lcd_light: %d", (int)msg.lcd_light);
    if (msg.has_energy_backup_state) LOG_STM_I(TAG, "energy_backup_state: %d", (int)msg.energy_backup_state);
    if (msg.has_energy_backup_en) LOG_STM_I(TAG, "energy_backup_en: %d", (int)msg.energy_backup_en);
    if (msg.has_energy_backup_start_soc) LOG_STM_I(TAG, "energy_backup_start_soc: %d", (int)msg.energy_backup_start_soc);
    if (msg.has_pow_get_qcusb1) LOG_STM_I(TAG, "pow_get_qcusb1: %.2f", msg.pow_get_qcusb1);
    if (msg.has_pow_get_qcusb2) LOG_STM_I(TAG, "pow_get_qcusb2: %.2f", msg.pow_get_qcusb2);
    if (msg.has_pow_get_typec1) LOG_STM_I(TAG, "pow_get_typec1: %.2f", msg.pow_get_typec1);
    if (msg.has_pow_get_typec2) LOG_STM_I(TAG, "pow_get_typec2: %.2f", msg.pow_get_typec2);
    if (msg.has_flow_info_qcusb1) LOG_STM_I(TAG, "flow_info_qcusb1: %d", (int)msg.flow_info_qcusb1);
    if (msg.has_flow_info_qcusb2) LOG_STM_I(TAG, "flow_info_qcusb2: %d", (int)msg.flow_info_qcusb2);
    if (msg.has_flow_info_typec1) LOG_STM_I(TAG, "flow_info_typec1: %d", (int)msg.flow_info_typec1);
    if (msg.has_flow_info_typec2) LOG_STM_I(TAG, "flow_info_typec2: %d", (int)msg.flow_info_typec2);
    if (msg.has_dev_standby_time) LOG_STM_I(TAG, "dev_standby_time: %d", (int)msg.dev_standby_time);
    if (msg.has_screen_off_time) LOG_STM_I(TAG, "screen_off_time: %d", (int)msg.screen_off_time);
    if (msg.has_ac_standby_time) LOG_STM_I(TAG, "ac_standby_time: %d", (int)msg.ac_standby_time);
    if (msg.has_dc_standby_time) LOG_STM_I(TAG, "dc_standby_time: %d", (int)msg.dc_standby_time);
    if (msg.has_ac_always_on_flag) LOG_STM_I(TAG, "ac_always_on_flag: %d", (int)msg.ac_always_on_flag);
    if (msg.has_ac_always_on_mini_soc) LOG_STM_I(TAG, "ac_always_on_mini_soc: %d", (int)msg.ac_always_on_mini_soc);
    if (msg.has_xboost_en) LOG_STM_I(TAG, "xboost_en: %d", (int)msg.xboost_en);
    if (msg.has_pcs_fan_level) LOG_STM_I(TAG, "pcs_fan_level: %d", (int)msg.pcs_fan_level);
    if (msg.has_flow_info_pv_h) LOG_STM_I(TAG, "flow_info_pv_h: %d", (int)msg.flow_info_pv_h);
    if (msg.has_flow_info_pv_l) LOG_STM_I(TAG, "flow_info_pv_l: %d", (int)msg.flow_info_pv_l);
    if (msg.has_flow_info_12v) LOG_STM_I(TAG, "flow_info_12v: %d", (int)msg.flow_info_12v);
    if (msg.has_flow_info_24v) LOG_STM_I(TAG, "flow_info_24v: %d", (int)msg.flow_info_24v);
    if (msg.has_pow_get_pv_h) LOG_STM_I(TAG, "pow_get_pv_h: %.2f", msg.pow_get_pv_h);
    if (msg.has_pow_get_pv_l) LOG_STM_I(TAG, "pow_get_pv_l: %.2f", msg.pow_get_pv_l);
    if (msg.has_pow_get_12v) LOG_STM_I(TAG, "pow_get_12v: %.2f", msg.pow_get_12v);
    if (msg.has_pow_get_24v) LOG_STM_I(TAG, "pow_get_24v: %.2f", msg.pow_get_24v);
    if (msg.has_plug_in_info_pv_h_flag) LOG_STM_I(TAG, "plug_in_info_pv_h_flag: %d", (int)msg.plug_in_info_pv_h_flag);
    if (msg.has_plug_in_info_pv_h_type) LOG_STM_I(TAG, "plug_in_info_pv_h_type: %d", (int)msg.plug_in_info_pv_h_type);
    if (msg.has_plug_in_info_pv_l_flag) LOG_STM_I(TAG, "plug_in_info_pv_l_flag: %d", (int)msg.plug_in_info_pv_l_flag);
    if (msg.has_plug_in_info_pv_l_type) LOG_STM_I(TAG, "plug_in_info_pv_l_type: %d", (int)msg.plug_in_info_pv_l_type);
    if (msg.has_flow_info_ac2dc) LOG_STM_I(TAG, "flow_info_ac2dc: %d", (int)msg.flow_info_ac2dc);
    if (msg.has_flow_info_dc2ac) LOG_STM_I(TAG, "flow_info_dc2ac: %d", (int)msg.flow_info_dc2ac);
    if (msg.has_flow_info_ac_in) LOG_STM_I(TAG, "flow_info_ac_in: %d", (int)msg.flow_info_ac_in);
    if (msg.has_flow_info_ac_hv_out) LOG_STM_I(TAG, "flow_info_ac_hv_out: %d", (int)msg.flow_info_ac_hv_out);
    if (msg.has_flow_info_ac_lv_out) LOG_STM_I(TAG, "flow_info_ac_lv_out: %d", (int)msg.flow_info_ac_lv_out);
    if (msg.has_flow_info_5p8_in) LOG_STM_I(TAG, "flow_info_5p8_in: %d", (int)msg.flow_info_5p8_in);
    if (msg.has_flow_info_5p8_out) LOG_STM_I(TAG, "flow_info_5p8_out: %d", (int)msg.flow_info_5p8_out);
    if (msg.has_pow_get_llc) LOG_STM_I(TAG, "pow_get_llc: %.2f", msg.pow_get_llc);
    if (msg.has_pow_get_ac) LOG_STM_I(TAG, "pow_get_ac: %.2f", msg.pow_get_ac);
    if (msg.has_pow_get_ac_in) LOG_STM_I(TAG, "pow_get_ac_in: %.2f", msg.pow_get_ac_in);
    if (msg.has_pow_get_ac_hv_out) LOG_STM_I(TAG, "pow_get_ac_hv_out: %.2f", msg.pow_get_ac_hv_out);
    if (msg.has_pow_get_ac_lv_out) LOG_STM_I(TAG, "pow_get_ac_lv_out: %.2f", msg.pow_get_ac_lv_out);
    if (msg.has_pow_get_ac_lv_tt30_out) LOG_STM_I(TAG, "pow_get_ac_lv_tt30_out: %.2f", msg.pow_get_ac_lv_tt30_out);
    if (msg.has_pow_get_5p8) LOG_STM_I(TAG, "pow_get_5p8: %.2f", msg.pow_get_5p8);
    if (msg.has_plug_in_info_ac_in_flag) LOG_STM_I(TAG, "plug_in_info_ac_in_flag: %d", (int)msg.plug_in_info_ac_in_flag);
    if (msg.has_plug_in_info_ac_in_feq) LOG_STM_I(TAG, "plug_in_info_ac_in_feq: %d", (int)msg.plug_in_info_ac_in_feq);
    if (msg.has_plug_in_info_5p8_flag) LOG_STM_I(TAG, "plug_in_info_5p8_flag: %d", (int)msg.plug_in_info_5p8_flag);
    if (msg.has_plug_in_info_5p8_type) LOG_STM_I(TAG, "plug_in_info_5p8_type: %d", (int)msg.plug_in_info_5p8_type);
    if (msg.has_plug_in_info_5p8_detail) LOG_STM_I(TAG, "plug_in_info_5p8_detail: %d", (int)msg.plug_in_info_5p8_detail);
    if (msg.has_pow_get_pv2) LOG_STM_I(TAG, "pow_get_pv2: %.2f", msg.pow_get_pv2);
    if (msg.has_btn_ac_out_lv_switch) LOG_STM_I(TAG, "btn_ac_out_lv_switch: %d", (int)msg.btn_ac_out_lv_switch);
    if (msg.has_btn_ac_out_hv_switch) LOG_STM_I(TAG, "btn_ac_out_hv_switch: %d", (int)msg.btn_ac_out_hv_switch);
    if (msg.has_dc_out_open) LOG_STM_I(TAG, "dc_out_open: %d", (int)msg.dc_out_open);
    if (msg.has_btn_dc_12v_out_switch) LOG_STM_I(TAG, "btn_dc_12v_out_switch: %d", (int)msg.btn_dc_12v_out_switch);
    if (msg.has_btn_usb_switch) LOG_STM_I(TAG, "btn_usb_switch: %d", (int)msg.btn_usb_switch);
    if (msg.has_pow_get_dcp2) LOG_STM_I(TAG, "pow_get_dcp2: %.2f", msg.pow_get_dcp2);
    if (msg.has_flow_info_dcp2_in) LOG_STM_I(TAG, "flow_info_dcp2_in: %d", (int)msg.flow_info_dcp2_in);
    if (msg.has_flow_info_dcp2_out) LOG_STM_I(TAG, "flow_info_dcp2_out: %d", (int)msg.flow_info_dcp2_out);
    if (msg.has_plug_in_info_pv2_dc_amp_max) LOG_STM_I(TAG, "plug_in_info_pv2_dc_amp_max: %d", (int)msg.plug_in_info_pv2_dc_amp_max);
    if (msg.has_plug_in_info_pv2_chg_amp_max) LOG_STM_I(TAG, "plug_in_info_pv2_chg_amp_max: %d", (int)msg.plug_in_info_pv2_chg_amp_max);
    if (msg.has_plug_in_info_pv2_chg_vol_max) LOG_STM_I(TAG, "plug_in_info_pv2_chg_vol_max: %d", (int)msg.plug_in_info_pv2_chg_vol_max);
    if (msg.has_plug_in_info_dcp2_in_flag) LOG_STM_I(TAG, "plug_in_info_dcp2_in_flag: %d", (int)msg.plug_in_info_dcp2_in_flag);
    if (msg.has_plug_in_info_dcp2_dsg_chg_type) LOG_STM_I(TAG, "plug_in_info_dcp2_dsg_chg_type: %d", (int)msg.plug_in_info_dcp2_dsg_chg_type);
    if (msg.has_plug_in_info_dcp2_charger_flag) LOG_STM_I(TAG, "plug_in_info_dcp2_charger_flag: %d", (int)msg.plug_in_info_dcp2_charger_flag);
    if (msg.has_plug_in_info_dcp2_type) LOG_STM_I(TAG, "plug_in_info_dcp2_type: %d", (int)msg.plug_in_info_dcp2_type);
    if (msg.has_plug_in_info_dcp2_detail) LOG_STM_I(TAG, "plug_in_info_dcp2_detail: %d", (int)msg.plug_in_info_dcp2_detail);
    if (msg.has_plug_in_info_dcp2_run_state) LOG_STM_I(TAG, "plug_in_info_dcp2_run_state: %d", (int)msg.plug_in_info_dcp2_run_state);
    if (msg.has_plug_in_info_dcp2_firm_ver) LOG_STM_I(TAG, "plug_in_info_dcp2_firm_ver: %d", (int)msg.plug_in_info_dcp2_firm_ver);
    if (msg.has_cms_batt_temp) LOG_STM_I(TAG, "cms_batt_temp: %d", (int)msg.cms_batt_temp);
    if (msg.has_pow_get_dc_bidi) LOG_STM_I(TAG, "pow_get_dc_bidi: %.2f", msg.pow_get_dc_bidi);
    if (msg.has_plug_in_info_dc_bidi_flag) LOG_STM_I(TAG, "plug_in_info_dc_bidi_flag: %d", (int)msg.plug_in_info_dc_bidi_flag);
    if (msg.has_plug_in_info_acp_chg_pow_max) LOG_STM_I(TAG, "plug_in_info_acp_chg_pow_max: %d", (int)msg.plug_in_info_acp_chg_pow_max);
    if (msg.has_plug_in_info_acp_chg_hal_pow_max) LOG_STM_I(TAG, "plug_in_info_acp_chg_hal_pow_max: %d", (int)msg.plug_in_info_acp_chg_hal_pow_max);
    if (msg.has_plug_in_info_acp_dsg_pow_max) LOG_STM_I(TAG, "plug_in_info_acp_dsg_pow_max: %d", (int)msg.plug_in_info_acp_dsg_pow_max);
    if (msg.has_plug_in_info_acp_flag) LOG_STM_I(TAG, "plug_in_info_acp_flag: %d", (int)msg.plug_in_info_acp_flag);
    if (msg.has_plug_in_info_acp_dsg_chg) LOG_STM_I(TAG, "plug_in_info_acp_dsg_chg: %d", (int)msg.plug_in_info_acp_dsg_chg);
    if (msg.has_plug_in_info_acp_charger_flag) LOG_STM_I(TAG, "plug_in_info_acp_charger_flag: %d", (int)msg.plug_in_info_acp_charger_flag);
    if (msg.has_plug_in_info_acp_type) LOG_STM_I(TAG, "plug_in_info_acp_type: %d", (int)msg.plug_in_info_acp_type);
    if (msg.has_plug_in_info_acp_detail) LOG_STM_I(TAG, "plug_in_info_acp_detail: %d", (int)msg.plug_in_info_acp_detail);
    if (msg.has_plug_in_info_acp_run_state) LOG_STM_I(TAG, "plug_in_info_acp_run_state: %d", (int)msg.plug_in_info_acp_run_state);
    if (msg.has_plug_in_info_acp_err_code) LOG_STM_I(TAG, "plug_in_info_acp_err_code: %d", (int)msg.plug_in_info_acp_err_code);
    if (msg.has_plug_in_info_acp_firm_ver) LOG_STM_I(TAG, "plug_in_info_acp_firm_ver: %d", (int)msg.plug_in_info_acp_firm_ver);
    if (msg.has_generator_conn_dev_errcode) LOG_STM_I(TAG, "generator_conn_dev_errcode: %d", (int)msg.generator_conn_dev_errcode);
    if (msg.has_plug_in_info_ac_in_chg_mode) LOG_STM_I(TAG, "plug_in_info_ac_in_chg_mode: %d", (int)msg.plug_in_info_ac_in_chg_mode);
    if (msg.has_generator_low_power_en) LOG_STM_I(TAG, "generator_low_power_en: %d", (int)msg.generator_low_power_en);
    if (msg.has_generator_low_power_threshold) LOG_STM_I(TAG, "generator_low_power_threshold: %d", (int)msg.generator_low_power_threshold);
    if (msg.has_generator_lpg_monitor_en) LOG_STM_I(TAG, "generator_lpg_monitor_en: %d", (int)msg.generator_lpg_monitor_en);
    if (msg.has_dev_online_flag) LOG_STM_I(TAG, "dev_online_flag: %d", (int)msg.dev_online_flag);
    if (msg.has_fuels_liquefied_gas_lpg_uint) LOG_STM_I(TAG, "fuels_liquefied_gas_lpg_uint: %d", (int)msg.fuels_liquefied_gas_lpg_uint);
    if (msg.has_fuels_liquefied_gas_lng_uint) LOG_STM_I(TAG, "fuels_liquefied_gas_lng_uint: %d", (int)msg.fuels_liquefied_gas_lng_uint);
    if (msg.has_utc_timezone) LOG_STM_I(TAG, "utc_timezone: %d", (int)msg.utc_timezone);
    if (msg.has_utc_set_mode) LOG_STM_I(TAG, "utc_set_mode: %d", (int)msg.utc_set_mode);
    if (msg.has_sp_charger_car_batt_vol_setting) LOG_STM_I(TAG, "sp_charger_car_batt_vol_setting: %d", (int)msg.sp_charger_car_batt_vol_setting);
    if (msg.has_sp_charger_car_batt_vol) LOG_STM_I(TAG, "sp_charger_car_batt_vol: %.2f", msg.sp_charger_car_batt_vol);
    if (msg.has_bms_err_code) LOG_STM_I(TAG, "bms_err_code: %d", (int)msg.bms_err_code);
    if (msg.has_wireless_oil_self_start) LOG_STM_I(TAG, "wireless_oil_self_start: %d", (int)msg.wireless_oil_self_start);
    if (msg.has_wireless_oil_on_soc) LOG_STM_I(TAG, "wireless_oil_on_soc: %d", (int)msg.wireless_oil_on_soc);
    if (msg.has_wireless_oil_off_soc) LOG_STM_I(TAG, "wireless_oil_off_soc: %d", (int)msg.wireless_oil_off_soc);
    if (msg.has_bypass_out_disable) LOG_STM_I(TAG, "bypass_out_disable: %d", (int)msg.bypass_out_disable);
    if (msg.has_output_power_off_memory) LOG_STM_I(TAG, "output_power_off_memory: %d", (int)msg.output_power_off_memory);
    if (msg.has_pv_chg_type) LOG_STM_I(TAG, "pv_chg_type: %d", (int)msg.pv_chg_type);
    if (msg.has_flow_info_bms_dsg) LOG_STM_I(TAG, "flow_info_bms_dsg: %d", (int)msg.flow_info_bms_dsg);
    if (msg.has_flow_info_bms_chg) LOG_STM_I(TAG, "flow_info_bms_chg: %d", (int)msg.flow_info_bms_chg);
    if (msg.has_flow_info_4p8_1_in) LOG_STM_I(TAG, "flow_info_4p8_1_in: %d", (int)msg.flow_info_4p8_1_in);
    if (msg.has_flow_info_4p8_1_out) LOG_STM_I(TAG, "flow_info_4p8_1_out: %d", (int)msg.flow_info_4p8_1_out);
    if (msg.has_flow_info_4p8_2_in) LOG_STM_I(TAG, "flow_info_4p8_2_in: %d", (int)msg.flow_info_4p8_2_in);
    if (msg.has_flow_info_4p8_2_out) LOG_STM_I(TAG, "flow_info_4p8_2_out: %d", (int)msg.flow_info_4p8_2_out);
    if (msg.has_pow_get_bms) LOG_STM_I(TAG, "pow_get_bms: %.2f", msg.pow_get_bms);
    if (msg.has_pow_get_4p8_1) LOG_STM_I(TAG, "pow_get_4p8_1: %.2f", msg.pow_get_4p8_1);
    if (msg.has_pow_get_4p8_2) LOG_STM_I(TAG, "pow_get_4p8_2: %.2f", msg.pow_get_4p8_2);
    if (msg.has_plug_in_info_4p8_1_in_flag) LOG_STM_I(TAG, "plug_in_info_4p8_1_in_flag: %d", (int)msg.plug_in_info_4p8_1_in_flag);
    if (msg.has_plug_in_info_4p8_1_type) LOG_STM_I(TAG, "plug_in_info_4p8_1_type: %d", (int)msg.plug_in_info_4p8_1_type);
    if (msg.has_plug_in_info_4p8_1_detail) LOG_STM_I(TAG, "plug_in_info_4p8_1_detail: %d", (int)msg.plug_in_info_4p8_1_detail);
    if (msg.has_plug_in_info_4p8_2_in_flag) LOG_STM_I(TAG, "plug_in_info_4p8_2_in_flag: %d", (int)msg.plug_in_info_4p8_2_in_flag);
    if (msg.has_plug_in_info_4p8_2_type) LOG_STM_I(TAG, "plug_in_info_4p8_2_type: %d", (int)msg.plug_in_info_4p8_2_type);
    if (msg.has_plug_in_info_4p8_2_detail) LOG_STM_I(TAG, "plug_in_info_4p8_2_detail: %d", (int)msg.plug_in_info_4p8_2_detail);
    if (msg.has_plug_in_info_pv_l_charger_flag) LOG_STM_I(TAG, "plug_in_info_pv_l_charger_flag: %d", (int)msg.plug_in_info_pv_l_charger_flag);
    if (msg.has_plug_in_info_pv_h_charger_flag) LOG_STM_I(TAG, "plug_in_info_pv_h_charger_flag: %d", (int)msg.plug_in_info_pv_h_charger_flag);
    if (msg.has_plug_in_info_pv_l_dc_amp_max) LOG_STM_I(TAG, "plug_in_info_pv_l_dc_amp_max: %d", (int)msg.plug_in_info_pv_l_dc_amp_max);
    if (msg.has_plug_in_info_pv_h_dc_amp_max) LOG_STM_I(TAG, "plug_in_info_pv_h_dc_amp_max: %d", (int)msg.plug_in_info_pv_h_dc_amp_max);
    if (msg.has_fast_charge_switch) LOG_STM_I(TAG, "fast_charge_switch: %d", (int)msg.fast_charge_switch);
    if (msg.has_plug_in_info_4p8_1_dsg_chg_type) LOG_STM_I(TAG, "plug_in_info_4p8_1_dsg_chg_type: %d", (int)msg.plug_in_info_4p8_1_dsg_chg_type);
    if (msg.has_plug_in_info_4p8_1_firm_ver) LOG_STM_I(TAG, "plug_in_info_4p8_1_firm_ver: %d", (int)msg.plug_in_info_4p8_1_firm_ver);
    if (msg.has_plug_in_info_4p8_2_dsg_chg_type) LOG_STM_I(TAG, "plug_in_info_4p8_2_dsg_chg_type: %d", (int)msg.plug_in_info_4p8_2_dsg_chg_type);
    if (msg.has_plug_in_info_4p8_2_firm_ver) LOG_STM_I(TAG, "plug_in_info_4p8_2_firm_ver: %d", (int)msg.plug_in_info_4p8_2_firm_ver);
    if (msg.has_plug_in_info_5p8_dsg_chg) LOG_STM_I(TAG, "plug_in_info_5p8_dsg_chg: %d", (int)msg.plug_in_info_5p8_dsg_chg);
    if (msg.has_plug_in_info_5p8_firm_ver) LOG_STM_I(TAG, "plug_in_info_5p8_firm_ver: %d", (int)msg.plug_in_info_5p8_firm_ver);
    if (msg.has_en_beep) LOG_STM_I(TAG, "en_beep: %d", (int)msg.en_beep);
    if (msg.has_llc_GFCI_flag) LOG_STM_I(TAG, "llc_GFCI_flag: %d", (int)msg.llc_GFCI_flag);
    if (msg.has_plug_in_info_ac_charger_flag) LOG_STM_I(TAG, "plug_in_info_ac_charger_flag: %d", (int)msg.plug_in_info_ac_charger_flag);
    if (msg.has_plug_in_info_5p8_charger_flag) LOG_STM_I(TAG, "plug_in_info_5p8_charger_flag: %d", (int)msg.plug_in_info_5p8_charger_flag);
    if (msg.has_plug_in_info_5p8_run_state) LOG_STM_I(TAG, "plug_in_info_5p8_run_state: %d", (int)msg.plug_in_info_5p8_run_state);
    if (msg.has_plug_in_info_4p8_1_charger_flag) LOG_STM_I(TAG, "plug_in_info_4p8_1_charger_flag: %d", (int)msg.plug_in_info_4p8_1_charger_flag);
    if (msg.has_plug_in_info_4p8_1_run_state) LOG_STM_I(TAG, "plug_in_info_4p8_1_run_state: %d", (int)msg.plug_in_info_4p8_1_run_state);
    if (msg.has_plug_in_info_4p8_2_charger_flag) LOG_STM_I(TAG, "plug_in_info_4p8_2_charger_flag: %d", (int)msg.plug_in_info_4p8_2_charger_flag);
    if (msg.has_plug_in_info_4p8_2_run_state) LOG_STM_I(TAG, "plug_in_info_4p8_2_run_state: %d", (int)msg.plug_in_info_4p8_2_run_state);
    if (msg.has_plug_in_info_ac_in_chg_pow_max) LOG_STM_I(TAG, "plug_in_info_ac_in_chg_pow_max: %d", (int)msg.plug_in_info_ac_in_chg_pow_max);
    if (msg.has_plug_in_info_5p8_chg_pow_max) LOG_STM_I(TAG, "plug_in_info_5p8_chg_pow_max: %d", (int)msg.plug_in_info_5p8_chg_pow_max);
    if (msg.has_ac_out_freq) LOG_STM_I(TAG, "ac_out_freq: %d", (int)msg.ac_out_freq);
    if (msg.has_dev_sleep_state) LOG_STM_I(TAG, "dev_sleep_state: %d", (int)msg.dev_sleep_state);
    if (msg.has_pd_err_code) LOG_STM_I(TAG, "pd_err_code: %d", (int)msg.pd_err_code);
    if (msg.has_llc_err_code) LOG_STM_I(TAG, "llc_err_code: %d", (int)msg.llc_err_code);
    if (msg.has_mppt_err_code) LOG_STM_I(TAG, "mppt_err_code: %d", (int)msg.mppt_err_code);
    if (msg.has_plug_in_info_5p8_err_code) LOG_STM_I(TAG, "plug_in_info_5p8_err_code: %d", (int)msg.plug_in_info_5p8_err_code);
    if (msg.has_plug_in_info_4p8_1_err_code) LOG_STM_I(TAG, "plug_in_info_4p8_1_err_code: %d", (int)msg.plug_in_info_4p8_1_err_code);
    if (msg.has_plug_in_info_4p8_2_err_code) LOG_STM_I(TAG, "plug_in_info_4p8_2_err_code: %d", (int)msg.plug_in_info_4p8_2_err_code);
    if (msg.has_pcs_fan_err_flag) LOG_STM_I(TAG, "pcs_fan_err_flag: %d", (int)msg.pcs_fan_err_flag);
    if (msg.has_llc_hv_lv_flag) LOG_STM_I(TAG, "llc_hv_lv_flag: %d", (int)msg.llc_hv_lv_flag);
    if (msg.has_llc_inv_err_code) LOG_STM_I(TAG, "llc_inv_err_code: %d", (int)msg.llc_inv_err_code);
    if (msg.has_plug_in_info_pv_h_chg_vol_max) LOG_STM_I(TAG, "plug_in_info_pv_h_chg_vol_max: %d", (int)msg.plug_in_info_pv_h_chg_vol_max);
    if (msg.has_plug_in_info_pv_l_chg_vol_max) LOG_STM_I(TAG, "plug_in_info_pv_l_chg_vol_max: %d", (int)msg.plug_in_info_pv_l_chg_vol_max);
    if (msg.has_plug_in_info_pv_l_chg_amp_max) LOG_STM_I(TAG, "plug_in_info_pv_l_chg_amp_max: %d", (int)msg.plug_in_info_pv_l_chg_amp_max);
    if (msg.has_plug_in_info_pv_h_chg_amp_max) LOG_STM_I(TAG, "plug_in_info_pv_h_chg_amp_max: %d", (int)msg.plug_in_info_pv_h_chg_amp_max);
    if (msg.has_plug_in_info_5p8_dsg_pow_max) LOG_STM_I(TAG, "plug_in_info_5p8_dsg_pow_max: %d", (int)msg.plug_in_info_5p8_dsg_pow_max);
    if (msg.has_plug_in_info_ac_out_dsg_pow_max) LOG_STM_I(TAG, "plug_in_info_ac_out_dsg_pow_max: %d", (int)msg.plug_in_info_ac_out_dsg_pow_max);
    if (msg.has_bms_batt_soc) LOG_STM_I(TAG, "bms_batt_soc: %.2f", msg.bms_batt_soc);
    if (msg.has_bms_batt_soh) LOG_STM_I(TAG, "bms_batt_soh: %.2f", msg.bms_batt_soh);
    if (msg.has_bms_design_cap) LOG_STM_I(TAG, "bms_design_cap: %d", (int)msg.bms_design_cap);
    if (msg.has_bms_dsg_rem_time) LOG_STM_I(TAG, "bms_dsg_rem_time: %d", (int)msg.bms_dsg_rem_time);
    if (msg.has_bms_chg_rem_time) LOG_STM_I(TAG, "bms_chg_rem_time: %d", (int)msg.bms_chg_rem_time);
    if (msg.has_bms_min_cell_temp) LOG_STM_I(TAG, "bms_min_cell_temp: %d", (int)msg.bms_min_cell_temp);
    if (msg.has_bms_max_cell_temp) LOG_STM_I(TAG, "bms_max_cell_temp: %d", (int)msg.bms_max_cell_temp);
    if (msg.has_bms_min_mos_temp) LOG_STM_I(TAG, "bms_min_mos_temp: %d", (int)msg.bms_min_mos_temp);
    if (msg.has_bms_max_mos_temp) LOG_STM_I(TAG, "bms_max_mos_temp: %d", (int)msg.bms_max_mos_temp);
    if (msg.has_cms_batt_soc) LOG_STM_I(TAG, "cms_batt_soc: %.2f", msg.cms_batt_soc);
    if (msg.has_cms_batt_soh) LOG_STM_I(TAG, "cms_batt_soh: %.2f", msg.cms_batt_soh);
    if (msg.has_cms_dsg_rem_time) LOG_STM_I(TAG, "cms_dsg_rem_time: %d", (int)msg.cms_dsg_rem_time);
    if (msg.has_cms_chg_rem_time) LOG_STM_I(TAG, "cms_chg_rem_time: %d", (int)msg.cms_chg_rem_time);
    if (msg.has_cms_max_chg_soc) LOG_STM_I(TAG, "cms_max_chg_soc: %d", (int)msg.cms_max_chg_soc);
    if (msg.has_cms_min_dsg_soc) LOG_STM_I(TAG, "cms_min_dsg_soc: %d", (int)msg.cms_min_dsg_soc);
    if (msg.has_cms_oil_on_soc) LOG_STM_I(TAG, "cms_oil_on_soc: %d", (int)msg.cms_oil_on_soc);
    if (msg.has_cms_oil_off_soc) LOG_STM_I(TAG, "cms_oil_off_soc: %d", (int)msg.cms_oil_off_soc);
    if (msg.has_cms_oil_self_start) LOG_STM_I(TAG, "cms_oil_self_start: %d", (int)msg.cms_oil_self_start);
    if (msg.has_cms_bms_run_state) LOG_STM_I(TAG, "cms_bms_run_state: %d", (int)msg.cms_bms_run_state);
    if (msg.has_bms_chg_dsg_state) LOG_STM_I(TAG, "bms_chg_dsg_state: %d", (int)msg.bms_chg_dsg_state);
    if (msg.has_cms_chg_dsg_state) LOG_STM_I(TAG, "cms_chg_dsg_state: %d", (int)msg.cms_chg_dsg_state);
    if (msg.has_ac_hv_always_on) LOG_STM_I(TAG, "ac_hv_always_on: %d", (int)msg.ac_hv_always_on);
    if (msg.has_ac_lv_always_on) LOG_STM_I(TAG, "ac_lv_always_on: %d", (int)msg.ac_lv_always_on);
    if (msg.has_time_task_conflict_flag) LOG_STM_I(TAG, "time_task_conflict_flag: %d", (int)msg.time_task_conflict_flag);
    if (msg.has_time_task_change_cnt) LOG_STM_I(TAG, "time_task_change_cnt: %d", (int)msg.time_task_change_cnt);
    if (msg.has_cms_batt_full_cap) LOG_STM_I(TAG, "cms_batt_full_cap: %d", (int)msg.cms_batt_full_cap);
    if (msg.has_cms_batt_design_cap) LOG_STM_I(TAG, "cms_batt_design_cap: %d", (int)msg.cms_batt_design_cap);
    if (msg.has_cms_batt_remain_cap) LOG_STM_I(TAG, "cms_batt_remain_cap: %d", (int)msg.cms_batt_remain_cap);
    if (msg.has_ble_standby_time) LOG_STM_I(TAG, "ble_standby_time: %d", (int)msg.ble_standby_time);
    if (msg.has_pow_get_dc) LOG_STM_I(TAG, "pow_get_dc: %.2f", msg.pow_get_dc);
    if (msg.has_ac_out_open) LOG_STM_I(TAG, "ac_out_open: %d", (int)msg.ac_out_open);
    if (msg.has_generator_fuels_type) LOG_STM_I(TAG, "generator_fuels_type: %d", (int)msg.generator_fuels_type);
    if (msg.has_generator_remain_time) LOG_STM_I(TAG, "generator_remain_time: %d", (int)msg.generator_remain_time);
    if (msg.has_generator_run_time) LOG_STM_I(TAG, "generator_run_time: %d", (int)msg.generator_run_time);
    if (msg.has_generator_total_output) LOG_STM_I(TAG, "generator_total_output: %d", (int)msg.generator_total_output);
    if (msg.has_generator_abnormal_state) LOG_STM_I(TAG, "generator_abnormal_state: %d", (int)msg.generator_abnormal_state);
    if (msg.has_fuels_oil_val) LOG_STM_I(TAG, "fuels_oil_val: %d", (int)msg.fuels_oil_val);
    if (msg.has_fuels_liquefied_gas_type) LOG_STM_I(TAG, "fuels_liquefied_gas_type: %d", (int)msg.fuels_liquefied_gas_type);
    if (msg.has_fuels_liquefied_gas_uint) LOG_STM_I(TAG, "fuels_liquefied_gas_uint: %d", (int)msg.fuels_liquefied_gas_uint);
    if (msg.has_fuels_liquefied_gas_val) LOG_STM_I(TAG, "fuels_liquefied_gas_val: %.2f", msg.fuels_liquefied_gas_val);
    if (msg.has_fuels_liquefied_gas_consume_per_hour) LOG_STM_I(TAG, "fuels_liquefied_gas_consume_per_hour: %.2f", msg.fuels_liquefied_gas_consume_per_hour);
    if (msg.has_fuels_liquefied_gas_remain_val) LOG_STM_I(TAG, "fuels_liquefied_gas_remain_val: %d", (int)msg.fuels_liquefied_gas_remain_val);
    if (msg.has_generator_perf_mode) LOG_STM_I(TAG, "generator_perf_mode: %d", (int)msg.generator_perf_mode);
    if (msg.has_generator_engine_open) LOG_STM_I(TAG, "generator_engine_open: %d", (int)msg.generator_engine_open);
    if (msg.has_generator_out_pow_max) LOG_STM_I(TAG, "generator_out_pow_max: %d", (int)msg.generator_out_pow_max);
    if (msg.has_generator_ac_out_pow_max) LOG_STM_I(TAG, "generator_ac_out_pow_max: %d", (int)msg.generator_ac_out_pow_max);
    if (msg.has_generator_dc_out_pow_max) LOG_STM_I(TAG, "generator_dc_out_pow_max: %d", (int)msg.generator_dc_out_pow_max);
    if (msg.has_generator_sub_battery_temp) LOG_STM_I(TAG, "generator_sub_battery_temp: %d", (int)msg.generator_sub_battery_temp);
    if (msg.has_generator_sub_battery_soc) LOG_STM_I(TAG, "generator_sub_battery_soc: %d", (int)msg.generator_sub_battery_soc);
    if (msg.has_generator_sub_battery_state) LOG_STM_I(TAG, "generator_sub_battery_state: %d", (int)msg.generator_sub_battery_state);
    if (msg.has_generator_maintence_state) LOG_STM_I(TAG, "generator_maintence_state: %d", (int)msg.generator_maintence_state);
    if (msg.has_generator_pcs_err_code) LOG_STM_I(TAG, "generator_pcs_err_code: %d", (int)msg.generator_pcs_err_code);
    if (msg.has_ups_alram) LOG_STM_I(TAG, "ups_alram: %d", (int)msg.ups_alram);
    if (msg.has_plug_in_info_pv_dc_amp_max) LOG_STM_I(TAG, "plug_in_info_pv_dc_amp_max: %d", (int)msg.plug_in_info_pv_dc_amp_max);
    if (msg.has_led_mode) LOG_STM_I(TAG, "led_mode: %d", (int)msg.led_mode);
    if (msg.has_low_power_alarm) LOG_STM_I(TAG, "low_power_alarm: %d", (int)msg.low_power_alarm);
    if (msg.has_silence_chg_watt) LOG_STM_I(TAG, "silence_chg_watt: %d", (int)msg.silence_chg_watt);
    if (msg.has_flow_info_pv) LOG_STM_I(TAG, "flow_info_pv: %d", (int)msg.flow_info_pv);
    if (msg.has_pow_get_pv) LOG_STM_I(TAG, "pow_get_pv: %.2f", msg.pow_get_pv);
    if (msg.has_plug_in_info_pv_flag) LOG_STM_I(TAG, "plug_in_info_pv_flag: %d", (int)msg.plug_in_info_pv_flag);
    if (msg.has_plug_in_info_pv_type) LOG_STM_I(TAG, "plug_in_info_pv_type: %d", (int)msg.plug_in_info_pv_type);
    if (msg.has_plug_in_info_pv_charger_flag) LOG_STM_I(TAG, "plug_in_info_pv_charger_flag: %d", (int)msg.plug_in_info_pv_charger_flag);
    if (msg.has_plug_in_info_pv_chg_amp_max) LOG_STM_I(TAG, "plug_in_info_pv_chg_amp_max: %d", (int)msg.plug_in_info_pv_chg_amp_max);
    if (msg.has_plug_in_info_pv_chg_vol_max) LOG_STM_I(TAG, "plug_in_info_pv_chg_vol_max: %d", (int)msg.plug_in_info_pv_chg_vol_max);
    if (msg.has_flow_info_ac_out) LOG_STM_I(TAG, "flow_info_ac_out: %d", (int)msg.flow_info_ac_out);
    if (msg.has_pow_get_ac_out) LOG_STM_I(TAG, "pow_get_ac_out: %.2f", msg.pow_get_ac_out);
    if (msg.has_flow_info_pv2) LOG_STM_I(TAG, "flow_info_pv2: %d", (int)msg.flow_info_pv2);
    if (msg.has_plug_in_info_pv2_flag) LOG_STM_I(TAG, "plug_in_info_pv2_flag: %d", (int)msg.plug_in_info_pv2_flag);
    if (msg.has_plug_in_info_pv2_type) LOG_STM_I(TAG, "plug_in_info_pv2_type: %d", (int)msg.plug_in_info_pv2_type);
    if (msg.has_flow_info_dcp_in) LOG_STM_I(TAG, "flow_info_dcp_in: %d", (int)msg.flow_info_dcp_in);
    if (msg.has_flow_info_dcp_out) LOG_STM_I(TAG, "flow_info_dcp_out: %d", (int)msg.flow_info_dcp_out);
    if (msg.has_pow_get_dcp) LOG_STM_I(TAG, "pow_get_dcp: %.2f", msg.pow_get_dcp);
    if (msg.has_plug_in_info_dcp_in_flag) LOG_STM_I(TAG, "plug_in_info_dcp_in_flag: %d", (int)msg.plug_in_info_dcp_in_flag);
    if (msg.has_plug_in_info_dcp_type) LOG_STM_I(TAG, "plug_in_info_dcp_type: %d", (int)msg.plug_in_info_dcp_type);
    if (msg.has_plug_in_info_dcp_detail) LOG_STM_I(TAG, "plug_in_info_dcp_detail: %d", (int)msg.plug_in_info_dcp_detail);
    if (msg.has_plug_in_info_pv2_charger_flag) LOG_STM_I(TAG, "plug_in_info_pv2_charger_flag: %d", (int)msg.plug_in_info_pv2_charger_flag);
    if (msg.has_plug_in_info_dcp_dsg_chg_type) LOG_STM_I(TAG, "plug_in_info_dcp_dsg_chg_type: %d", (int)msg.plug_in_info_dcp_dsg_chg_type);
    if (msg.has_plug_in_info_dcp_firm_ver) LOG_STM_I(TAG, "plug_in_info_dcp_firm_ver: %d", (int)msg.plug_in_info_dcp_firm_ver);
    if (msg.has_plug_in_info_dcp_charger_flag) LOG_STM_I(TAG, "plug_in_info_dcp_charger_flag: %d", (int)msg.plug_in_info_dcp_charger_flag);
    if (msg.has_plug_in_info_dcp_run_state) LOG_STM_I(TAG, "plug_in_info_dcp_run_state: %d", (int)msg.plug_in_info_dcp_run_state);
    if (msg.has_dcdc_err_code) LOG_STM_I(TAG, "dcdc_err_code: %d", (int)msg.dcdc_err_code);
    if (msg.has_plug_in_info_dcp_err_code) LOG_STM_I(TAG, "plug_in_info_dcp_err_code: %d", (int)msg.plug_in_info_dcp_err_code);
    if (msg.has_plug_in_info_dcp2_err_code) LOG_STM_I(TAG, "plug_in_info_dcp2_err_code: %d", (int)msg.plug_in_info_dcp2_err_code);
    if (msg.has_inv_err_code) LOG_STM_I(TAG, "inv_err_code: %d", (int)msg.inv_err_code);
    if (msg.has_generator_pv_hybrid_mode_open) LOG_STM_I(TAG, "generator_pv_hybrid_mode_open: %d", (int)msg.generator_pv_hybrid_mode_open);
    if (msg.has_generator_pv_hybrid_mode_soc_max) LOG_STM_I(TAG, "generator_pv_hybrid_mode_soc_max: %d", (int)msg.generator_pv_hybrid_mode_soc_max);
    if (msg.has_generator_care_mode_open) LOG_STM_I(TAG, "generator_care_mode_open: %d", (int)msg.generator_care_mode_open);
    if (msg.has_generator_care_mode_start_time) LOG_STM_I(TAG, "generator_care_mode_start_time: %d", (int)msg.generator_care_mode_start_time);
    if (msg.has_ac_energy_saving_open) LOG_STM_I(TAG, "ac_energy_saving_open: %d", (int)msg.ac_energy_saving_open);
    if (msg.has_multi_bp_chg_dsg_mode) LOG_STM_I(TAG, "multi_bp_chg_dsg_mode: %d", (int)msg.multi_bp_chg_dsg_mode);
    if (msg.has_plug_in_info_5p8_chg_hal_pow_max) LOG_STM_I(TAG, "plug_in_info_5p8_chg_hal_pow_max: %d", (int)msg.plug_in_info_5p8_chg_hal_pow_max);
    if (msg.has_plug_in_info_ac_in_chg_hal_pow_max) LOG_STM_I(TAG, "plug_in_info_ac_in_chg_hal_pow_max: %d", (int)msg.plug_in_info_ac_in_chg_hal_pow_max);
    if (msg.has_cms_batt_pow_out_max) LOG_STM_I(TAG, "cms_batt_pow_out_max: %d", (int)msg.cms_batt_pow_out_max);
    if (msg.has_cms_batt_pow_in_max) LOG_STM_I(TAG, "cms_batt_pow_in_max: %d", (int)msg.cms_batt_pow_in_max);
    if (msg.has_cms_batt_full_energy) LOG_STM_I(TAG, "cms_batt_full_energy: %d", (int)msg.cms_batt_full_energy);
    if (msg.has_storm_pattern_enable) LOG_STM_I(TAG, "storm_pattern_enable: %d", (int)msg.storm_pattern_enable);
    if (msg.has_storm_pattern_open_flag) LOG_STM_I(TAG, "storm_pattern_open_flag: %d", (int)msg.storm_pattern_open_flag);
    if (msg.has_storm_pattern_end_time) LOG_STM_I(TAG, "storm_pattern_end_time: %d", (int)msg.storm_pattern_end_time);
    if (msg.has_sp_charger_chg_mode) LOG_STM_I(TAG, "sp_charger_chg_mode: %d", (int)msg.sp_charger_chg_mode);
    if (msg.has_sp_charger_run_state) LOG_STM_I(TAG, "sp_charger_run_state: %d", (int)msg.sp_charger_run_state);
    if (msg.has_sp_charger_is_connect_car) LOG_STM_I(TAG, "sp_charger_is_connect_car: %d", (int)msg.sp_charger_is_connect_car);
    if (msg.has_installment_payment_serve_enable) LOG_STM_I(TAG, "installment_payment_serve_enable: %d", (int)msg.installment_payment_serve_enable);
    if (msg.has_serve_middlemen) LOG_STM_I(TAG, "serve_middlemen: %d", (int)msg.serve_middlemen);
    if (msg.has_installment_payment_overdue_limit) LOG_STM_I(TAG, "installment_payment_overdue_limit: %d", (int)msg.installment_payment_overdue_limit);
    if (msg.has_installment_payment_state) LOG_STM_I(TAG, "installment_payment_state: %d", (int)msg.installment_payment_state);
    if (msg.has_installment_payment_start_utc_time) LOG_STM_I(TAG, "installment_payment_start_utc_time: %d", (int)msg.installment_payment_start_utc_time);
    if (msg.has_installment_payment_overdue_limit_utc_time) LOG_STM_I(TAG, "installment_payment_overdue_limit_utc_time: %d", (int)msg.installment_payment_overdue_limit_utc_time);
    if (msg.has_sp_charger_chg_open) LOG_STM_I(TAG, "sp_charger_chg_open: %d", (int)msg.sp_charger_chg_open);
    if (msg.has_sp_charger_chg_pow_limit) LOG_STM_I(TAG, "sp_charger_chg_pow_limit: %.2f", msg.sp_charger_chg_pow_limit);
    if (msg.has_module_bluetooth_snr) LOG_STM_I(TAG, "module_bluetooth_snr: %.2f", msg.module_bluetooth_snr);
    if (msg.has_module_bluetooth_rssi) LOG_STM_I(TAG, "module_bluetooth_rssi: %.2f", msg.module_bluetooth_rssi);
    if (msg.has_module_wifi_snr) LOG_STM_I(TAG, "module_wifi_snr: %.2f", msg.module_wifi_snr);
    if (msg.has_module_wifi_rssi) LOG_STM_I(TAG, "module_wifi_rssi: %.2f", msg.module_wifi_rssi);
    if (msg.has_sp_charger_chg_pow_max) LOG_STM_I(TAG, "sp_charger_chg_pow_max: %.2f", msg.sp_charger_chg_pow_max);
}

static void logAlternatorChargerData(const AlternatorChargerData& d) {
    LOG_STM_I(TAG, "--- Full Alternator Charger Dump ---");
    LOG_STM_I(TAG, "Mode: %d, Open: %d", d.chargerMode, d.chargerOpen);
    LOG_STM_I(TAG, "Car Batt: %.1fV, Limit: %dW", d.carBatteryVoltage, d.powerLimit);
    LOG_STM_I(TAG, "DC Power: %.1fW", d.dcPower);
}

static void logFullAlternatorChargerData(const dc009_apl_comm_DisplayPropertyUpload& msg) {
    LOG_STM_I(TAG, "--- Full Alternator Charger Dump ---");
    if (msg.has_errcode) LOG_STM_I(TAG, "errcode: %d", (int)msg.errcode);
    if (msg.has_pow_in_sum_w) LOG_STM_I(TAG, "pow_in_sum_w: %.2f", msg.pow_in_sum_w);
    if (msg.has_pow_out_sum_w) LOG_STM_I(TAG, "pow_out_sum_w: %.2f", msg.pow_out_sum_w);
    if (msg.has_cms_batt_temp) LOG_STM_I(TAG, "cms_batt_temp: %d", (int)msg.cms_batt_temp);
    if (msg.has_pow_get_dc_bidi) LOG_STM_I(TAG, "pow_get_dc_bidi: %.2f", msg.pow_get_dc_bidi);
    if (msg.has_plug_in_info_dc_bidi_flag) LOG_STM_I(TAG, "plug_in_info_dc_bidi_flag: %d", (int)msg.plug_in_info_dc_bidi_flag);
    if (msg.has_dev_online_flag) LOG_STM_I(TAG, "dev_online_flag: %d", (int)msg.dev_online_flag);
    if (msg.has_utc_timezone) LOG_STM_I(TAG, "utc_timezone: %d", (int)msg.utc_timezone);
    if (msg.has_sp_charger_car_batt_vol_setting) LOG_STM_I(TAG, "sp_charger_car_batt_vol_setting: %d", (int)msg.sp_charger_car_batt_vol_setting);
    if (msg.has_sp_charger_car_batt_vol) LOG_STM_I(TAG, "sp_charger_car_batt_vol: %.2f", msg.sp_charger_car_batt_vol);
    if (msg.has_cms_batt_soc) LOG_STM_I(TAG, "cms_batt_soc: %.2f", msg.cms_batt_soc);
    if (msg.has_cms_dsg_rem_time) LOG_STM_I(TAG, "cms_dsg_rem_time: %d", (int)msg.cms_dsg_rem_time);
    if (msg.has_cms_chg_rem_time) LOG_STM_I(TAG, "cms_chg_rem_time: %d", (int)msg.cms_chg_rem_time);
    if (msg.has_cms_chg_dsg_state) LOG_STM_I(TAG, "cms_chg_dsg_state: %d", (int)msg.cms_chg_dsg_state);
    if (msg.has_pow_get_dcp) LOG_STM_I(TAG, "pow_get_dcp: %.2f", msg.pow_get_dcp);
    if (msg.has_plug_in_info_dcp_in_flag) LOG_STM_I(TAG, "plug_in_info_dcp_in_flag: %d", (int)msg.plug_in_info_dcp_in_flag);
    if (msg.has_plug_in_info_dcp_type) LOG_STM_I(TAG, "plug_in_info_dcp_type: %d", (int)msg.plug_in_info_dcp_type);
    if (msg.has_plug_in_info_dcp_detail) LOG_STM_I(TAG, "plug_in_info_dcp_detail: %d", (int)msg.plug_in_info_dcp_detail);
    if (msg.has_plug_in_info_dcp_run_state) LOG_STM_I(TAG, "plug_in_info_dcp_run_state: %d", (int)msg.plug_in_info_dcp_run_state);
    if (msg.has_sp_charger_chg_mode) LOG_STM_I(TAG, "sp_charger_chg_mode: %d", (int)msg.sp_charger_chg_mode);
    if (msg.has_sp_charger_run_state) LOG_STM_I(TAG, "sp_charger_run_state: %d", (int)msg.sp_charger_run_state);
    if (msg.has_sp_charger_is_connect_car) LOG_STM_I(TAG, "sp_charger_is_connect_car: %d", (int)msg.sp_charger_is_connect_car);
    if (msg.has_sp_charger_chg_open) LOG_STM_I(TAG, "sp_charger_chg_open: %d", (int)msg.sp_charger_chg_open);
    if (msg.has_sp_charger_chg_pow_limit) LOG_STM_I(TAG, "sp_charger_chg_pow_limit: %.2f", msg.sp_charger_chg_pow_limit);
    if (msg.has_module_bluetooth_snr) LOG_STM_I(TAG, "module_bluetooth_snr: %.2f", msg.module_bluetooth_snr);
    if (msg.has_module_bluetooth_rssi) LOG_STM_I(TAG, "module_bluetooth_rssi: %.2f", msg.module_bluetooth_rssi);
    if (msg.has_module_wifi_snr) LOG_STM_I(TAG, "module_wifi_snr: %.2f", msg.module_wifi_snr);
    if (msg.has_module_wifi_rssi) LOG_STM_I(TAG, "module_wifi_rssi: %.2f", msg.module_wifi_rssi);
    if (msg.has_sp_charger_chg_pow_max) LOG_STM_I(TAG, "sp_charger_chg_pow_max: %.2f", msg.sp_charger_chg_pow_max);
    if (msg.has_sp_charger_extension_line_p_setting) LOG_STM_I(TAG, "sp_charger_extension_line_p_setting: %.2f", msg.sp_charger_extension_line_p_setting);
    if (msg.has_sp_charger_extension_line_n_setting) LOG_STM_I(TAG, "sp_charger_extension_line_n_setting: %.2f", msg.sp_charger_extension_line_n_setting);
    if (msg.has_sp_charger_driving_chg_setting) LOG_STM_I(TAG, "sp_charger_driving_chg_setting: %d", (int)msg.sp_charger_driving_chg_setting);
    if (msg.has_sp_charger_car_batt_chg_amp_limit) LOG_STM_I(TAG, "sp_charger_car_batt_chg_amp_limit: %.2f", msg.sp_charger_car_batt_chg_amp_limit);
    if (msg.has_sp_charger_dev_batt_chg_amp_limit) LOG_STM_I(TAG, "sp_charger_dev_batt_chg_amp_limit: %.2f", msg.sp_charger_dev_batt_chg_amp_limit);
    if (msg.has_sp_charger_car_batt_chg_amp_max) LOG_STM_I(TAG, "sp_charger_car_batt_chg_amp_max: %.2f", msg.sp_charger_car_batt_chg_amp_max);
    if (msg.has_sp_charger_car_batt_urgent_chg_state) LOG_STM_I(TAG, "sp_charger_car_batt_urgent_chg_state: %d", (int)msg.sp_charger_car_batt_urgent_chg_state);
    if (msg.has_sp_charger_dev_batt_chg_amp_max) LOG_STM_I(TAG, "sp_charger_dev_batt_chg_amp_max: %.2f", msg.sp_charger_dev_batt_chg_amp_max);
    if (msg.has_sp_charger_car_batt_urgent_chg_switch) LOG_STM_I(TAG, "sp_charger_car_batt_urgent_chg_switch: %d", (int)msg.sp_charger_car_batt_urgent_chg_switch);
}

namespace EcoflowDataParser {

void parsePacket(const Packet& pkt, EcoflowData& data, DeviceType type) {

    switch (type) {
        case DeviceType::DELTA_3: {
            if (pkt.getSrc() == 0x02 && pkt.getCmdSet() == 0xFE && (pkt.getCmdId() == 0x11 || pkt.getCmdId() == 0x15)) {
                pd335_sys_DisplayPropertyUpload d3_msg = pd335_sys_DisplayPropertyUpload_init_zero;
                pb_istream_t stream = pb_istream_from_buffer(pkt.getPayload().data(), pkt.getPayload().size());

                if (pb_decode(&stream, pd335_sys_DisplayPropertyUpload_fields, &d3_msg)) {
                    Delta3Data& d3 = data.delta3;

                    if (d3_msg.has_cms_batt_soc) d3.batteryLevel = d3_msg.cms_batt_soc;
                    else if (d3_msg.has_bms_batt_soc) d3.batteryLevel = d3_msg.bms_batt_soc;

                    if (d3_msg.has_pow_get_ac_in) d3.acInputPower = d3_msg.pow_get_ac_in;
                    if (d3_msg.has_pow_get_ac_out) d3.acOutputPower = -std::abs(d3_msg.pow_get_ac_out);

                    if (d3_msg.has_pow_in_sum_w) d3.inputPower = d3_msg.pow_in_sum_w;
                    if (d3_msg.has_pow_out_sum_w) d3.outputPower = d3_msg.pow_out_sum_w;

                    if (d3_msg.has_pow_get_12v) d3.dc12vOutputPower = -std::abs(d3_msg.pow_get_12v);
                    if (d3_msg.has_pow_get_pv) d3.dcPortInputPower = d3_msg.pow_get_pv;

                    if (d3_msg.has_plug_in_info_pv_type) d3.dcPortState = (int)d3_msg.plug_in_info_pv_type;

                    if (d3_msg.has_pow_get_typec1) d3.usbcOutputPower = -std::abs(d3_msg.pow_get_typec1);
                    if (d3_msg.has_pow_get_typec2) d3.usbc2OutputPower = -std::abs(d3_msg.pow_get_typec2);
                    if (d3_msg.has_pow_get_qcusb1) d3.usbaOutputPower = -std::abs(d3_msg.pow_get_qcusb1);
                    if (d3_msg.has_pow_get_qcusb2) d3.usba2OutputPower = -std::abs(d3_msg.pow_get_qcusb2);

                    if (d3_msg.has_plug_in_info_ac_charger_flag) d3.pluggedInAc = d3_msg.plug_in_info_ac_charger_flag;

                    if (d3_msg.has_energy_backup_en) d3.energyBackup = d3_msg.energy_backup_en;
                    if (d3_msg.has_energy_backup_start_soc) d3.energyBackupBatteryLevel = d3_msg.energy_backup_start_soc;

                    if (d3_msg.has_pow_get_bms) {
                        float bms_pow = d3_msg.pow_get_bms;
                        d3.batteryInputPower = (bms_pow > 0) ? bms_pow : 0;
                        d3.batteryOutputPower = (bms_pow < 0) ? -bms_pow : 0;
                    }

                    if (d3_msg.has_cms_min_dsg_soc) d3.batteryChargeLimitMin = d3_msg.cms_min_dsg_soc;
                    if (d3_msg.has_cms_max_chg_soc) d3.batteryChargeLimitMax = d3_msg.cms_max_chg_soc;

                    if (d3_msg.has_bms_max_cell_temp) d3.cellTemperature = d3_msg.bms_max_cell_temp;

                    if (d3_msg.has_flow_info_12v) d3.dc12vPort = is_flow_on(d3_msg.flow_info_12v);
                    if (d3_msg.has_flow_info_ac_out) d3.acPorts = is_flow_on(d3_msg.flow_info_ac_out);

                    if (d3_msg.has_plug_in_info_ac_in_chg_pow_max) d3.acChargingSpeed = d3_msg.plug_in_info_ac_in_chg_pow_max;

                    if (d3.dcPortState == 2 && d3.dcPortInputPower > 0) { // 2 = SOLAR
                         d3.solarInputPower = d3.dcPortInputPower;
                    } else {
                        d3.solarInputPower = 0;
                    }

                    d3.acOn = d3.acPorts;
                    d3.dcOn = d3.dc12vPort;
                    if (d3_msg.has_flow_info_qcusb1) d3.usbOn = is_flow_on(d3_msg.flow_info_qcusb1);

                    static uint32_t lastDumpId = 0;
                    if (currentDumpId > lastDumpId) {
                        logFullDelta3Data(d3_msg);
                        lastDumpId = currentDumpId;
                    }
                }
            }
            break;
        }

        case DeviceType::DELTA_PRO_3: {
            if (pkt.getSrc() == 0x02 && pkt.getCmdSet() == 0xFE && (pkt.getCmdId() == 0x11 || pkt.getCmdId() == 0x15)) {
                mr521_DisplayPropertyUpload mr521_msg = mr521_DisplayPropertyUpload_init_zero;
                pb_istream_t stream = pb_istream_from_buffer(pkt.getPayload().data(), pkt.getPayload().size());

                if (pb_decode(&stream, mr521_DisplayPropertyUpload_fields, &mr521_msg)) {
                     DeltaPro3Data& d3p = data.deltaPro3;

                     if (mr521_msg.has_cms_batt_soc) d3p.batteryLevel = mr521_msg.cms_batt_soc;
                     if (mr521_msg.has_bms_batt_soc) d3p.batteryLevelMain = mr521_msg.bms_batt_soc;
                     if (mr521_msg.has_pow_get_ac_in) d3p.acInputPower = mr521_msg.pow_get_ac_in;
                     if (mr521_msg.has_pow_get_ac_lv_out) d3p.acLvOutputPower = -std::abs(mr521_msg.pow_get_ac_lv_out);
                     if (mr521_msg.has_pow_get_ac_hv_out) d3p.acHvOutputPower = -std::abs(mr521_msg.pow_get_ac_hv_out);

                     if (mr521_msg.has_pow_in_sum_w) d3p.inputPower = mr521_msg.pow_in_sum_w;
                     if (mr521_msg.has_pow_out_sum_w) d3p.outputPower = mr521_msg.pow_out_sum_w;

                     if (mr521_msg.has_pow_get_12v) d3p.dc12vOutputPower = -std::abs(mr521_msg.pow_get_12v);
                     if (mr521_msg.has_pow_get_pv_l) d3p.dcLvInputPower = mr521_msg.pow_get_pv_l;
                     if (mr521_msg.has_pow_get_pv_h) d3p.dcHvInputPower = mr521_msg.pow_get_pv_h;

                     if (mr521_msg.has_plug_in_info_pv_l_type) d3p.dcLvInputState = mr521_msg.plug_in_info_pv_l_type;
                     if (mr521_msg.has_plug_in_info_pv_h_type) d3p.dcHvInputState = mr521_msg.plug_in_info_pv_h_type;

                     if (mr521_msg.has_pow_get_qcusb1) d3p.usbaOutputPower = -std::abs(mr521_msg.pow_get_qcusb1);
                     if (mr521_msg.has_pow_get_qcusb2) d3p.usba2OutputPower = -std::abs(mr521_msg.pow_get_qcusb2);
                     if (mr521_msg.has_pow_get_typec1) d3p.usbcOutputPower = -std::abs(mr521_msg.pow_get_typec1);
                     if (mr521_msg.has_pow_get_typec2) d3p.usbc2OutputPower = -std::abs(mr521_msg.pow_get_typec2);

                     if (mr521_msg.has_plug_in_info_ac_in_chg_pow_max) d3p.acChargingSpeed = mr521_msg.plug_in_info_ac_in_chg_pow_max;
                     if (mr521_msg.has_plug_in_info_ac_in_chg_hal_pow_max) d3p.maxAcChargingPower = mr521_msg.plug_in_info_ac_in_chg_hal_pow_max;

                     if (mr521_msg.has_energy_backup_en) d3p.energyBackup = mr521_msg.energy_backup_en;
                     if (mr521_msg.has_energy_backup_start_soc) d3p.energyBackupBatteryLevel = mr521_msg.energy_backup_start_soc;

                     if (mr521_msg.has_cms_min_dsg_soc) d3p.batteryChargeLimitMin = mr521_msg.cms_min_dsg_soc;
                     if (mr521_msg.has_cms_max_chg_soc) d3p.batteryChargeLimitMax = mr521_msg.cms_max_chg_soc;

                     if (mr521_msg.has_bms_max_cell_temp) d3p.cellTemperature = mr521_msg.bms_max_cell_temp;

                     if (mr521_msg.has_flow_info_12v) d3p.dc12vPort = is_flow_on(mr521_msg.flow_info_12v);
                     if (mr521_msg.has_flow_info_ac_lv_out) d3p.acLvPort = is_flow_on(mr521_msg.flow_info_ac_lv_out);
                     if (mr521_msg.has_flow_info_ac_hv_out) d3p.acHvPort = is_flow_on(mr521_msg.flow_info_ac_hv_out);

                     if (mr521_msg.has_llc_GFCI_flag) d3p.gfiMode = mr521_msg.llc_GFCI_flag;

                     // Logic for solar might need adjustment based on d3 logic
                     if (d3p.dcLvInputState == 2 && d3p.dcLvInputPower > 0) d3p.solarLvPower = d3p.dcLvInputPower;
                     else d3p.solarLvPower = 0;

                     if (d3p.dcHvInputState == 2 && d3p.dcHvInputPower > 0) d3p.solarHvPower = d3p.dcHvInputPower;
                     else d3p.solarHvPower = 0;

                     // New mappings
                     if (mr521_msg.has_pow_get_4p8_1) d3p.expansion1Power = mr521_msg.pow_get_4p8_1;
                     if (mr521_msg.has_pow_get_4p8_2) d3p.expansion2Power = mr521_msg.pow_get_4p8_2;
                     if (mr521_msg.has_flow_info_ac_in) d3p.acInputStatus = mr521_msg.flow_info_ac_in;
                     if (mr521_msg.has_bms_batt_soh) d3p.soh = mr521_msg.bms_batt_soh;
                     if (mr521_msg.has_bms_dsg_rem_time) d3p.dischargeRemainingTime = mr521_msg.bms_dsg_rem_time;
                     if (mr521_msg.has_bms_chg_rem_time) d3p.chargeRemainingTime = mr521_msg.bms_chg_rem_time;

                     static uint32_t lastDumpId = 0;
                     if (currentDumpId > lastDumpId) {
                         logFullDeltaPro3Data(mr521_msg);
                         lastDumpId = currentDumpId;
                     }
                }
            }
            break;
        }

        case DeviceType::ALTERNATOR_CHARGER: {
            if (pkt.getSrc() == 0x14 && pkt.getCmdSet() == 0xFE && (pkt.getCmdId() == 0x11 || pkt.getCmdId() == 0x15)) {
                dc009_apl_comm_DisplayPropertyUpload msg = dc009_apl_comm_DisplayPropertyUpload_init_zero;
                pb_istream_t stream = pb_istream_from_buffer(pkt.getPayload().data(), pkt.getPayload().size());

                if (pb_decode(&stream, dc009_apl_comm_DisplayPropertyUpload_fields, &msg)) {
                    AlternatorChargerData& ac = data.alternatorCharger;

                    if (msg.has_cms_batt_soc) ac.batteryLevel = msg.cms_batt_soc;
                    if (msg.has_cms_batt_temp) ac.batteryTemperature = msg.cms_batt_temp;
                    if (msg.has_pow_get_dc_bidi) ac.dcPower = msg.pow_get_dc_bidi;

                    if (msg.has_sp_charger_car_batt_vol) ac.carBatteryVoltage = msg.sp_charger_car_batt_vol;
                    if (msg.has_sp_charger_car_batt_vol_setting) ac.startVoltage = msg.sp_charger_car_batt_vol_setting / 10.0f;

                    if (msg.has_sp_charger_chg_mode) ac.chargerMode = (int)msg.sp_charger_chg_mode;
                    if (msg.has_sp_charger_chg_open) ac.chargerOpen = msg.sp_charger_chg_open;
                    if (msg.has_sp_charger_chg_pow_limit) ac.powerLimit = msg.sp_charger_chg_pow_limit;
                    if (msg.has_sp_charger_chg_pow_max) ac.powerMax = msg.sp_charger_chg_pow_max;

                    if (msg.has_sp_charger_car_batt_chg_amp_limit) ac.reverseChargingCurrentLimit = msg.sp_charger_car_batt_chg_amp_limit;
                    if (msg.has_sp_charger_dev_batt_chg_amp_limit) ac.chargingCurrentLimit = msg.sp_charger_dev_batt_chg_amp_limit;

                    if (msg.has_sp_charger_car_batt_chg_amp_max) ac.reverseChargingCurrentMax = msg.sp_charger_car_batt_chg_amp_max;
                    if (msg.has_sp_charger_dev_batt_chg_amp_max) ac.chargingCurrentMax = msg.sp_charger_dev_batt_chg_amp_max;

                    static uint32_t lastDumpId = 0;
                    if (currentDumpId > lastDumpId) {
                        logFullAlternatorChargerData(msg);
                        lastDumpId = currentDumpId;
                    }
                }
            }
            break;
        }

        case DeviceType::WAVE_2: {
            if (pkt.getCmdSet() == 0x42 && pkt.getCmdId() == 0x50) {
                const std::vector<uint8_t>& payload = pkt.getPayload();

                if (payload.size() >= 108) {
                    const uint8_t* p = payload.data();
                    Wave2Data& w2 = data.wave2;

                    w2.mode = p[0];
                    w2.subMode = p[1];
                    w2.setTemp = p[2];
                    w2.fanValue = p[3];
                    w2.envTemp = get_float_le_safe(p + 4);
                    w2.tempSys = p[8];
                    w2.displayIdleTime = get_uint16_le(p + 9);
                    w2.displayIdleMode = p[11];
                    w2.timeEn = p[12];
                    w2.timeSetVal = get_uint16_le(p + 13);
                    w2.timeRemainVal = get_uint16_le(p + 15);
                    w2.beepEnable = p[17];
                    w2.errCode = get_uint32_le(p + 18);

                    w2.refEn = p[54];
                    w2.bmsPid = get_uint16_le(p + 55);
                    w2.wteFthEn = p[57];
                    w2.tempDisplay = p[58];
                    w2.powerMode = p[59];
                    w2.powerSrc = p[60];
                    w2.psdrPwrWatt = swap_endian_and_parse_signed_int(p + 61);
                    w2.batPwrWatt = swap_endian_and_parse_signed_int(p + 63);
                    w2.mpptPwrWatt = swap_endian_and_parse_signed_int(p + 65);
                    w2.batDsgRemainTime = get_uint32_le(p + 67);
                    w2.batChgRemainTime = get_uint32_le(p + 71);
                    w2.batSoc = p[75];
                    w2.batChgStatus = p[76];
                    w2.outLetTemp = get_float_le_safe(p + 77);
                    w2.mpptWork = p[81];
                    w2.bmsErr = p[82];
                    w2.rgbState = p[83];
                    w2.waterValue = p[84];
                    w2.bmsBoundFlag = p[85];
                    w2.bmsUndervoltage = p[86];
                    w2.ver = p[87];

                    if (w2.batChgRemainTime > 0 && w2.batChgRemainTime < 6000) w2.remainingTime = w2.batChgRemainTime;
                    else if (w2.batDsgRemainTime > 0 && w2.batDsgRemainTime < 6000) w2.remainingTime = w2.batDsgRemainTime;
                    else w2.remainingTime = 0;

                    static uint32_t lastDumpId = 0;
                    if (currentDumpId > lastDumpId) {
                        logWave2Data(w2);
                        lastDumpId = currentDumpId;
                    }
                }
            }
            break;
        }
    }
}

} // namespace EcoflowDataParser
