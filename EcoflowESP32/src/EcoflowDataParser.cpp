#include "EcoflowDataParser.h"
#include "EcoflowBinaryMap.h"
#include "EcoflowConstants.h"
#include "pb_utils.h"
#include "pd335_sys.pb.h"
#include "mr521.pb.h"
#include "dc009_apl_comm.pb.h"
#include <Arduino.h>
#include "esp_log.h"
#include <cmath>
#include <algorithm>

static const char* TAG = "EcoflowDataParser";

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

static bool is_flow_on(uint32_t x) {
    return (x & 0b11) >= 0b10;
}

static void logDelta3Data(const Delta3Data& d) {
    ESP_LOGI(TAG, "=== Delta 3 Data ===");
    ESP_LOGI(TAG, "Batt: %.1f%% (In: %.1fW, Out: %.1fW)", d.batteryLevel, d.batteryInputPower, d.batteryOutputPower);
    ESP_LOGI(TAG, "AC In: %.1fW, AC Out: %.1fW", d.acInputPower, d.acOutputPower);
    ESP_LOGI(TAG, "DC In: %.1fW (State: %d, Solar: %.1fW)", d.dcPortInputPower, d.dcPortState, d.solarInputPower);
    ESP_LOGI(TAG, "Total In: %.1fW, Out: %.1fW", d.inputPower, d.outputPower);
    ESP_LOGI(TAG, "12V Out: %.1fW, USB-C: %.1fW/%.1fW, USB-A: %.1fW/%.1fW",
             d.dc12vOutputPower, d.usbcOutputPower, d.usbc2OutputPower, d.usbaOutputPower, d.usba2OutputPower);
    ESP_LOGI(TAG, "SOC Limits: %d%% - %d%%, AC Chg Speed: %dW", d.batteryChargeLimitMin, d.batteryChargeLimitMax, d.acChargingSpeed);
    ESP_LOGI(TAG, "Flags: AC Plugged=%d, Backup=%d, AC Ports=%d, 12V Port=%d", d.pluggedInAc, d.energyBackup, d.acPorts, d.dc12vPort);
}

static void logWave2Data(const Wave2Data& w) {
    ESP_LOGI(TAG, "=== Wave 2 Data ===");
    ESP_LOGI(TAG, "Mode: %d (Sub: %d), PwrMode: %d", w.mode, w.subMode, w.powerMode);
    ESP_LOGI(TAG, "Temps: Env=%.2f, Out=%.2f, Set=%d", w.envTemp, w.outLetTemp, w.setTemp);
    ESP_LOGI(TAG, "Batt: %d%% (Stat: %d), Rem: %dm/%dm", w.batSoc, w.batChgStatus, w.batChgRemainTime, w.batDsgRemainTime);
    ESP_LOGI(TAG, "Power: Bat=%dW, MPPT=%dW, PSDR=%dW", w.batPwrWatt, w.mpptPwrWatt, w.psdrPwrWatt);
    ESP_LOGI(TAG, "Fan: %d, Water: %d, RGB: %d", w.fanValue, w.waterValue, w.rgbState);
    ESP_LOGI(TAG, "Err: %u, BMS Err: %d", w.errCode, w.bmsErr);
}

static void logDelta2Data(const Delta2Data& d2) {
    ESP_LOGI(TAG, "=== Delta 2 Data ===");
    ESP_LOGI(TAG, "Batt: %.2f%%, In: %fW, Out: %fW", d2.batteryLevel, d2.inputPower, d2.outputPower);
    ESP_LOGI(TAG, "Remain: %dmin Chg / %dmin Dsg", d2.chargeTime, d2.dischargeTime);
    ESP_LOGI(TAG, "AC Out: %fW, AC In: %fW, Plugged: %d, Speed: %dW", d2.acOutputPower, d2.acInputPower, d2.pluggedInAc, d2.acChargingSpeed);
    ESP_LOGI(TAG, "DC Out: %fW", d2.dc12vOutputPower);
    ESP_LOGI(TAG, "USB-A Out: %fW, USB-C Out: %fW", d2.usba1OutputPower, d2.usbc1OutputPower);
    ESP_LOGI(TAG, "Limits: %d%% - %d%%", d2.batteryChargeLimitMin, d2.batteryChargeLimitMax);
    ESP_LOGI(TAG, "Temp: %.1fC", d2.batteryTemperature);
    ESP_LOGI(TAG, "Ports: AC=%d, DC=%d, USB=%d", d2.acOn, d2.dcOn, d2.usbOn);
    for(int i=0; i<d2.extraBatteries.size(); i++) {
        ESP_LOGI(TAG, "Extra Batt %d: %.2f%%, In: %fW, Out: %fW, Temp: %.1fC, Cycles: %d", i+1, d2.extraBatteries[i].batteryLevel, d2.extraBatteries[i].inputPower, d2.extraBatteries[i].outputPower, d2.extraBatteries[i].batteryTemperature, d2.extraBatteries[i].cycles);
    }
}


namespace EcoflowDataParser {

void parsePacket(const Packet& pkt, EcoflowData& data) {
    const std::vector<uint8_t>& payload = pkt.getPayload();
    const uint8_t* p = payload.data();

    // V3 Protobuf Packet (Delta 3) - src 0x02
    if (pkt.getSrc() == 0x02 && pkt.getCmdSet() == 0xFE && (pkt.getCmdId() == 0x11 || pkt.getCmdId() == 0x15)) {
        // Attempt to decode as Delta 3 (PD335) FIRST
        pd335_sys_DisplayPropertyUpload d3_msg = pd335_sys_DisplayPropertyUpload_init_zero;
        pb_istream_t stream = pb_istream_from_buffer(pkt.getPayload().data(), pkt.getPayload().size());

        if (pb_decode(&stream, pd335_sys_DisplayPropertyUpload_fields, &d3_msg)) {
            Delta3Data& d3 = data.delta3;

            if (d3_msg.has_cms_batt_soc) d3.batteryLevel = d3_msg.cms_batt_soc;
            else if (d3_msg.has_bms_batt_soc) d3.batteryLevel = d3_msg.bms_batt_soc;

            if (d3_msg.has_pow_get_ac_in) d3.acInputPower = d3_msg.pow_get_ac_in;
            if (d3_msg.has_pow_get_ac_out) d3.acOutputPower = -std::abs(d3_msg.pow_get_ac_out); // -round(x, 2)

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

            logDelta3Data(d3);
            return;
        }

        mr521_DisplayPropertyUpload mr521_msg = mr521_DisplayPropertyUpload_init_zero;
        pb_istream_t stream_mr = pb_istream_from_buffer(pkt.getPayload().data(), pkt.getPayload().size());

        if (pb_decode(&stream_mr, mr521_DisplayPropertyUpload_fields, &mr521_msg)) {
             DeltaPro3Data& d3p = data.deltaPro3;

             if (mr521_msg.has_cms_batt_soc) d3p.batteryLevel = mr521_msg.cms_batt_soc;
             if (mr521_msg.has_pow_get_ac) d3p.acInputPower = -mr521_msg.pow_get_ac;
             if (mr521_msg.has_pow_get_ac_lv_out) d3p.acLvOutputPower = -std::abs(mr521_msg.pow_get_ac_lv_out);
             if (mr521_msg.has_pow_get_ac_hv_out) d3p.acHvOutputPower = -std::abs(mr521_msg.pow_get_ac_hv_out);

             if (mr521_msg.has_pow_in_sum_w) d3p.inputPower = mr521_msg.pow_in_sum_w;
             if (mr521_msg.has_pow_out_sum_w) d3p.outputPower = mr521_msg.pow_out_sum_w;

             if (mr521_msg.has_pow_get_12v) d3p.dc12vOutputPower = -std::abs(mr521_msg.pow_get_12v);
             if (mr521_msg.has_pow_get_pv_l) d3p.dcLvInputPower = mr521_msg.pow_get_pv_l;
             if (mr521_msg.has_pow_get_pv_h) d3p.dcHvInputPower = mr521_msg.pow_get_pv_h;

             if (mr521_msg.has_plug_in_info_pv_l_type) d3p.dcLvInputState = mr521_msg.plug_in_info_pv_l_type;
             if (mr521_msg.has_plug_in_info_pv_h_type) d3p.dcHvInputState = mr521_msg.plug_in_info_pv_h_type;

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

             if (d3p.dcLvInputState == 2 && d3p.dcLvInputPower > 0) d3p.solarLvPower = d3p.dcLvInputPower;
             if (d3p.dcHvInputState == 2 && d3p.dcHvInputPower > 0) d3p.solarHvPower = d3p.dcHvInputPower;

             return;
        }
    }

    // Alternator Charger (src 0x14)
    else if (pkt.getSrc() == 0x14 && pkt.getCmdSet() == 0xFE && (pkt.getCmdId() == 0x11 || pkt.getCmdId() == 0x15)) {
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
        }
    }

    // V2 Binary Packet (Wave 2 / KT210)
    else if (pkt.getCmdSet() == 0x42 && pkt.getCmdId() == 0x50) {
        if (payload.size() >= sizeof(Wave2Packet)) {
            Wave2Data& w2 = data.wave2;
            const Wave2Packet* p = reinterpret_cast<const Wave2Packet*>(payload.data());

            w2.mode = p->mode;
            w2.subMode = p->sub_mode;
            w2.setTemp = p->set_temp;
            w2.fanValue = p->fan_value;
            w2.envTemp = p->env_temp;
            w2.tempSys = p->temp_sys;
            w2.displayIdleTime = p->display_idle_time;
            w2.displayIdleMode = p->display_idle_mode;
            w2.timeEn = p->time_en;
            w2.timeSetVal = p->time_set_val;
            w2.timeRemainVal = p->time_remain_val;
            w2.beepEnable = p->beep_enable;
            w2.errCode = p->err_code;

            w2.refEn = p->ref_en;
            w2.bmsPid = p->bms_pid;
            w2.wteFthEn = p->wte_fth_en;
            w2.tempDisplay = p->temp_display;
            w2.powerMode = p->power_mode;
            w2.powerSrc = p->power_src;
            w2.psdrPwrWatt = p->psdr_pwr_watt;
            w2.batPwrWatt = p->bat_pwr_watt;
            w2.mpptPwrWatt = p->mppt_pwr_watt;
            w2.batDsgRemainTime = p->bat_dsg_remain_time;
            w2.batChgRemainTime = p->bat_chg_remain_time;
            w2.batSoc = p->bat_soc;
            w2.batChgStatus = p->bat_chg_status;
            w2.outLetTemp = p->out_let_temp;
            w2.mpptWork = p->mppt_work;
            w2.bmsErr = p->bms_err;
            w2.rgbState = p->rgb_state;
            w2.waterValue = p->water_value;
            w2.bmsBoundFlag = p->bms_bound_flag;
            w2.bmsUndervoltage = p->bms_undervoltage;
            w2.ver = p->ver;

            if (w2.batChgRemainTime > 0 && w2.batChgRemainTime < 6000) w2.remainingTime = w2.batChgRemainTime;
            else if (w2.batDsgRemainTime > 0 && w2.batDsgRemainTime < 6000) w2.remainingTime = w2.batDsgRemainTime;
            else w2.remainingTime = 0;

            logWave2Data(w2);
        }
    }
    // V2 Binary Packet (Delta 2)
    else if (pkt.getCmdSet() == Ecoflow::D2::CMDSET_PD) {
        Delta2Data& d2 = data.delta2;
        bool changed = false;
        if (pkt.getSrc() == Ecoflow::D2::SRC_PD && pkt.getCmdId() == Ecoflow::D2::CMDID_PD_HEARTBEAT) { // PD Heartbeat
            if (payload.size() < sizeof(Delta2PdPacket)) return;
            const Delta2PdPacket* pd = reinterpret_cast<const Delta2PdPacket*>(p);
            d2.outputPower = pd->watts_out_sum;
            d2.inputPower = pd->watts_in_sum;
            d2.usba1OutputPower = pd->usb1_watt;
            d2.usba2OutputPower = pd->usb2_watt;
            d2.usbc1OutputPower = pd->typec1_watts;
            d2.usbc2OutputPower = pd->typec2_watts;
            d2.dc12vOutputPower = pd->car_watts;
            d2.pluggedInAc = pd->ac_charge_flag;
            d2.acInputPower = pd->ac_input_watts;
            d2.acOutputPower = pd->ac_output_watts;
            d2.acOn = pd->cfg_ac_enabled;
            d2.dcOn = pd->car_state;
            d2.usbOn = pd->dc_out_state;
            d2.batteryTemperature = pd->car_temp;
            if (d2.batteryLevel == 0) {
                d2.batteryLevel = pd->soc;
            }
            d2.dischargeTime = pd->remain_time;
            d2.chargeTime = pd->remain_time;
            changed = true;
        }
        else if (pkt.getSrc() == Ecoflow::D2::SRC_EMS && pkt.getCmdId() == Ecoflow::D2::CMDID_EMS_HEARTBEAT) { // EMS Heartbeat
            if (payload.size() < sizeof(Delta2EmsPacket)) return;
            const Delta2EmsPacket* ems = reinterpret_cast<const Delta2EmsPacket*>(p);
            d2.batteryChargeLimitMax = ems->max_charge_soc;
            d2.chargeTime = ems->chg_remain_time;
            d2.dischargeTime = ems->dsg_remain_time;
            d2.batteryChargeLimitMin = ems->min_dsg_soc;
            d2.batteryLevel = ems->f32_lcd_show_soc > 0 ? ems->f32_lcd_show_soc : d2.batteryLevel;
            changed = true;
        }
        else if (pkt.getSrc() == Ecoflow::D2::SRC_BMS && pkt.getCmdId() == Ecoflow::D2::CMDID_BMS_HEARTBEAT) { // BMS Heartbeat
            if (payload.size() < sizeof(Delta2BmsPacket)) return;

            int offset = 0;
            const Delta2BmsPacket* bms = reinterpret_cast<const Delta2BmsPacket*>(p + offset);
            d2.batteryLevel = bms->f32_show_soc > 0 ? bms->f32_show_soc : d2.batteryLevel;
            d2.batteryTemperature = bms->temp;
            d2.batteryVoltage = bms->vol;
            d2.batteryCurrent = bms->amp;

            offset += sizeof(Delta2BmsPacket);

            d2.extraBatteries.clear();
            while(offset + sizeof(Delta2BmsPacket) <= payload.size()){
                const Delta2BmsPacket* extra_bms = reinterpret_cast<const Delta2BmsPacket*>(p + offset);
                ExtraBatteryData extra;
                extra.batteryLevel = extra_bms->f32_show_soc;
                extra.inputPower = extra_bms->input_watts;
                extra.outputPower = extra_bms->output_watts;
                extra.batteryTemperature = extra_bms->temp;
                extra.cycles = extra_bms->cycles;
                d2.extraBatteries.push_back(extra);
                offset += sizeof(Delta2BmsPacket);
            }
            changed = true;
        } else if (pkt.getSrc() == Ecoflow::D2::SRC_MPPT && pkt.getCmdId() == Ecoflow::D2::CMDID_MPPT_HEARTBEAT) { // MPPT Heartbeat
            if (payload.size() < sizeof(Delta2MpptPacket)) return;
            const Delta2MpptPacket* mppt = reinterpret_cast<const Delta2MpptPacket*>(p);
            d2.solarInputPower = mppt->in_watts;
            d2.dc12vOutputVoltage = mppt->car_out_vol / 1000.0;
            d2.dc12vOutputCurrent = mppt->car_out_amp / 1000.0;
            d2.acChargingSpeed = mppt->cfg_chg_watts;
            changed = true;
        }
        if (changed) {
            logDelta2Data(d2);
        }
    } else if (pkt.getSrc() == Ecoflow::D2::SRC_KIT_INFO && pkt.getCmdSet() == 0x03 && pkt.getCmdId() == Ecoflow::D2::CMDID_KIT_INFO) {
        if (payload.size() < sizeof(AllKitDetailDataPacket)) return;
        const AllKitDetailDataPacket* kit_info = reinterpret_cast<const AllKitDetailDataPacket*>(p);

        int offset = sizeof(AllKitDetailDataPacket);
        for(int i=0; i<kit_info->support_kit_max_num; i++) {
            if (offset + sizeof(KitBaseInfoPacket) > payload.size()) break;
            const KitBaseInfoPacket* base_info = reinterpret_cast<const KitBaseInfoPacket*>(p + offset);
            // We can add logic here to handle the kit info data if needed in the future
            offset += sizeof(KitBaseInfoPacket);
        }
    }
}

} // namespace EcoflowDataParser
