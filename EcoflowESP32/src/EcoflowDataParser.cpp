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

static void logFullDelta3Data(const Delta3Data& d) {
    LOG_STM_I(TAG, "--- Full Delta 3 Dump ---");
    vTaskDelay(10);
    LOG_STM_I(TAG, "batteryLevel: %.2f", d.batteryLevel);
    LOG_STM_I(TAG, "acInputPower: %.2f", d.acInputPower);
    LOG_STM_I(TAG, "acOutputPower: %.2f", d.acOutputPower);
    LOG_STM_I(TAG, "inputPower: %.2f", d.inputPower);
    LOG_STM_I(TAG, "outputPower: %.2f", d.outputPower);
    vTaskDelay(10);
    LOG_STM_I(TAG, "dc12vOutputPower: %.2f", d.dc12vOutputPower);
    LOG_STM_I(TAG, "dcPortInputPower: %.2f", d.dcPortInputPower);
    LOG_STM_I(TAG, "dcPortState: %d", d.dcPortState);
    LOG_STM_I(TAG, "usbcOutputPower: %.2f", d.usbcOutputPower);
    LOG_STM_I(TAG, "usbc2OutputPower: %.2f", d.usbc2OutputPower);
    vTaskDelay(10);
    LOG_STM_I(TAG, "usbaOutputPower: %.2f", d.usbaOutputPower);
    LOG_STM_I(TAG, "usba2OutputPower: %.2f", d.usba2OutputPower);
    LOG_STM_I(TAG, "pluggedInAc: %d", d.pluggedInAc);
    LOG_STM_I(TAG, "energyBackup: %d", d.energyBackup);
    LOG_STM_I(TAG, "energyBackupBatteryLevel: %d", d.energyBackupBatteryLevel);
    vTaskDelay(10);
    LOG_STM_I(TAG, "batteryInputPower: %.2f", d.batteryInputPower);
    LOG_STM_I(TAG, "batteryOutputPower: %.2f", d.batteryOutputPower);
    LOG_STM_I(TAG, "batteryChargeLimitMin: %d", d.batteryChargeLimitMin);
    LOG_STM_I(TAG, "batteryChargeLimitMax: %d", d.batteryChargeLimitMax);
    LOG_STM_I(TAG, "cellTemperature: %d", d.cellTemperature);
    vTaskDelay(10);
    LOG_STM_I(TAG, "dc12vPort: %d", d.dc12vPort);
    LOG_STM_I(TAG, "acPorts: %d", d.acPorts);
    LOG_STM_I(TAG, "solarInputPower: %.2f", d.solarInputPower);
    LOG_STM_I(TAG, "acChargingSpeed: %d", d.acChargingSpeed);
    LOG_STM_I(TAG, "maxAcChargingPower: %d", d.maxAcChargingPower);
    vTaskDelay(10);
    LOG_STM_I(TAG, "acOn: %d", d.acOn);
    LOG_STM_I(TAG, "dcOn: %d", d.dcOn);
    LOG_STM_I(TAG, "usbOn: %d", d.usbOn);
}

static void logWave2Data(const Wave2Data& w) {
    LOG_STM_I(TAG, "--- Full Wave 2 Dump ---");
    vTaskDelay(10);
    LOG_STM_I(TAG, "mode: %d", w.mode);
    LOG_STM_I(TAG, "subMode: %d", w.subMode);
    LOG_STM_I(TAG, "setTemp: %d", w.setTemp);
    LOG_STM_I(TAG, "fanValue: %d", w.fanValue);
    LOG_STM_I(TAG, "envTemp: %.2f", w.envTemp);
    LOG_STM_I(TAG, "tempSys: %d", w.tempSys);
    vTaskDelay(10);
    LOG_STM_I(TAG, "displayIdleTime: %d", w.displayIdleTime);
    LOG_STM_I(TAG, "displayIdleMode: %d", w.displayIdleMode);
    LOG_STM_I(TAG, "timeEn: %d", w.timeEn);
    LOG_STM_I(TAG, "timeSetVal: %d", w.timeSetVal);
    LOG_STM_I(TAG, "timeRemainVal: %d", w.timeRemainVal);
    LOG_STM_I(TAG, "beepEnable: %d", w.beepEnable);
    vTaskDelay(10);
    LOG_STM_I(TAG, "errCode: %u", w.errCode);
    LOG_STM_I(TAG, "refEn: %d", w.refEn);
    LOG_STM_I(TAG, "bmsPid: %d", w.bmsPid);
    LOG_STM_I(TAG, "wteFthEn: %d", w.wteFthEn);
    LOG_STM_I(TAG, "tempDisplay: %d", w.tempDisplay);
    LOG_STM_I(TAG, "powerMode: %d", w.powerMode);
    vTaskDelay(10);
    LOG_STM_I(TAG, "powerSrc: %d", w.powerSrc);
    LOG_STM_I(TAG, "psdrPwrWatt: %d", w.psdrPwrWatt);
    LOG_STM_I(TAG, "batPwrWatt: %d", w.batPwrWatt);
    LOG_STM_I(TAG, "mpptPwrWatt: %d", w.mpptPwrWatt);
    LOG_STM_I(TAG, "batDsgRemainTime: %u", w.batDsgRemainTime);
    LOG_STM_I(TAG, "batChgRemainTime: %u", w.batChgRemainTime);
    vTaskDelay(10);
    LOG_STM_I(TAG, "batSoc: %d", w.batSoc);
    LOG_STM_I(TAG, "batChgStatus: %d", w.batChgStatus);
    LOG_STM_I(TAG, "outLetTemp: %.2f", w.outLetTemp);
    LOG_STM_I(TAG, "mpptWork: %d", w.mpptWork);
    LOG_STM_I(TAG, "bmsErr: %d", w.bmsErr);
    LOG_STM_I(TAG, "rgbState: %d", w.rgbState);
    vTaskDelay(10);
    LOG_STM_I(TAG, "waterValue: %d", w.waterValue);
    LOG_STM_I(TAG, "bmsBoundFlag: %d", w.bmsBoundFlag);
    LOG_STM_I(TAG, "bmsUndervoltage: %d", w.bmsUndervoltage);
    LOG_STM_I(TAG, "ver: %d", w.ver);
    LOG_STM_I(TAG, "remainingTime: %d", w.remainingTime);
}

static void logFullDeltaPro3Data(const DeltaPro3Data& d) {
    LOG_STM_I(TAG, "--- Full D3P Dump ---");
    vTaskDelay(10);
    LOG_STM_I(TAG, "batteryLevel: %.2f", d.batteryLevel);
    LOG_STM_I(TAG, "batteryLevelMain: %.2f", d.batteryLevelMain);
    LOG_STM_I(TAG, "acInputPower: %.2f", d.acInputPower);
    LOG_STM_I(TAG, "acLvOutputPower: %.2f", d.acLvOutputPower);
    LOG_STM_I(TAG, "acHvOutputPower: %.2f", d.acHvOutputPower);
    vTaskDelay(10);
    LOG_STM_I(TAG, "inputPower: %.2f", d.inputPower);
    LOG_STM_I(TAG, "outputPower: %.2f", d.outputPower);
    LOG_STM_I(TAG, "dc12vOutputPower: %.2f", d.dc12vOutputPower);
    LOG_STM_I(TAG, "dcLvInputPower: %.2f", d.dcLvInputPower);
    LOG_STM_I(TAG, "dcHvInputPower: %.2f", d.dcHvInputPower);
    vTaskDelay(10);
    LOG_STM_I(TAG, "dcLvInputState: %d", d.dcLvInputState);
    LOG_STM_I(TAG, "dcHvInputState: %d", d.dcHvInputState);
    LOG_STM_I(TAG, "usbcOutputPower: %.2f", d.usbcOutputPower);
    LOG_STM_I(TAG, "usbc2OutputPower: %.2f", d.usbc2OutputPower);
    LOG_STM_I(TAG, "usbaOutputPower: %.2f", d.usbaOutputPower);
    vTaskDelay(10);
    LOG_STM_I(TAG, "usba2OutputPower: %.2f", d.usba2OutputPower);
    LOG_STM_I(TAG, "acChargingSpeed: %d", d.acChargingSpeed);
    LOG_STM_I(TAG, "maxAcChargingPower: %d", d.maxAcChargingPower);
    LOG_STM_I(TAG, "pluggedInAc: %d", d.pluggedInAc);
    LOG_STM_I(TAG, "energyBackup: %d", d.energyBackup);
    vTaskDelay(10);
    LOG_STM_I(TAG, "energyBackupBatteryLevel: %d", d.energyBackupBatteryLevel);
    LOG_STM_I(TAG, "batteryChargeLimitMin: %d", d.batteryChargeLimitMin);
    LOG_STM_I(TAG, "batteryChargeLimitMax: %d", d.batteryChargeLimitMax);
    LOG_STM_I(TAG, "cellTemperature: %d", d.cellTemperature);
    LOG_STM_I(TAG, "dc12vPort: %d", d.dc12vPort);
    vTaskDelay(10);
    LOG_STM_I(TAG, "acLvPort: %d", d.acLvPort);
    LOG_STM_I(TAG, "acHvPort: %d", d.acHvPort);
    LOG_STM_I(TAG, "solarLvPower: %.2f", d.solarLvPower);
    LOG_STM_I(TAG, "solarHvPower: %.2f", d.solarHvPower);
    LOG_STM_I(TAG, "gfiMode: %d", d.gfiMode);
    vTaskDelay(10);
    LOG_STM_I(TAG, "expansion1Power: %.2f", d.expansion1Power);
    LOG_STM_I(TAG, "expansion2Power: %.2f", d.expansion2Power);
    LOG_STM_I(TAG, "acInputStatus: %d", d.acInputStatus);
    LOG_STM_I(TAG, "soh: %.2f", d.soh);
    LOG_STM_I(TAG, "dischargeRemainingTime: %u", d.dischargeRemainingTime);
    LOG_STM_I(TAG, "chargeRemainingTime: %u", d.chargeRemainingTime);
}

static void logFullAlternatorChargerData(const AlternatorChargerData& d) {
    LOG_STM_I(TAG, "--- Full Alternator Charger Dump ---");
    vTaskDelay(10);
    LOG_STM_I(TAG, "batteryLevel: %.2f", d.batteryLevel);
    LOG_STM_I(TAG, "batteryTemperature: %.2f", d.batteryTemperature);
    LOG_STM_I(TAG, "dcPower: %.2f", d.dcPower);
    LOG_STM_I(TAG, "carBatteryVoltage: %.2f", d.carBatteryVoltage);
    LOG_STM_I(TAG, "startVoltage: %.2f", d.startVoltage);
    vTaskDelay(10);
    LOG_STM_I(TAG, "startVoltageMin: %d", d.startVoltageMin);
    LOG_STM_I(TAG, "startVoltageMax: %d", d.startVoltageMax);
    LOG_STM_I(TAG, "chargerMode: %d", d.chargerMode);
    LOG_STM_I(TAG, "chargerOpen: %d", d.chargerOpen);
    LOG_STM_I(TAG, "powerLimit: %d", d.powerLimit);
    vTaskDelay(10);
    LOG_STM_I(TAG, "powerMax: %d", d.powerMax);
    LOG_STM_I(TAG, "reverseChargingCurrentLimit: %.2f", d.reverseChargingCurrentLimit);
    LOG_STM_I(TAG, "chargingCurrentLimit: %.2f", d.chargingCurrentLimit);
    LOG_STM_I(TAG, "reverseChargingCurrentMax: %.2f", d.reverseChargingCurrentMax);
    LOG_STM_I(TAG, "chargingCurrentMax: %.2f", d.chargingCurrentMax);
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
                        logFullDelta3Data(d3);
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
                         logFullDeltaPro3Data(d3p);
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
                        logFullAlternatorChargerData(ac);
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
