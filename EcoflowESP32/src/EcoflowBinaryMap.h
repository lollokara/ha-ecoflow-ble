#ifndef ECOFLOW_BINARY_MAP_H
#define ECOFLOW_BINARY_MAP_H

#include <stdint.h>

#pragma pack(push, 1)

struct Wave2Packet {
    uint8_t mode;
    uint8_t sub_mode;
    uint8_t set_temp;
    uint8_t fan_value;
    float env_temp;
    uint8_t temp_sys;
    uint16_t display_idle_time;
    uint8_t display_idle_mode;
    uint8_t time_en;
    uint16_t time_set_val;
    uint16_t time_remain_val;
    uint8_t beep_enable;
    uint32_t err_code;
    uint8_t reserved1[32];
    uint8_t ref_en;
    uint16_t bms_pid;
    uint8_t wte_fth_en;
    uint8_t temp_display;
    uint8_t power_mode;
    uint8_t power_src;
    int16_t psdr_pwr_watt;
    int16_t bat_pwr_watt;
    int16_t mppt_pwr_watt;
    uint32_t bat_dsg_remain_time;
    uint32_t bat_chg_remain_time;
    uint8_t bat_soc;
    uint8_t bat_chg_status;
    float out_let_temp;
    uint8_t mppt_work;
    uint8_t bms_err;
    uint8_t rgb_state;
    uint8_t water_value;
    uint8_t bms_bound_flag;
    uint8_t bms_undervoltage;
    uint8_t ver;
};


struct Delta2PdPacket {
    uint8_t model;
    uint8_t error_code[4];
    uint8_t sys_ver[4];
    uint8_t wifi_ver[4];
    uint8_t wifi_auto_recovery;
    uint8_t soc;
    uint16_t watts_out_sum;
    uint16_t watts_in_sum;
    int32_t remain_time;
    uint8_t quiet_mode;
    uint8_t dc_out_state;
    uint8_t usb1_watt;
    uint8_t usb2_watt;
    uint8_t qc_usb1_watt;
    uint8_t qc_usb2_watt;
    uint8_t typec1_watts;
    uint8_t typec2_watts;
    uint8_t typec1_temp;
    uint8_t typec2_temp;
    uint8_t car_state;
    uint8_t car_watts;
    uint8_t car_temp;
    uint16_t standby_min;
    uint16_t lcd_off_sec;
    uint8_t lcd_brightness;
    int32_t dc_chg_power;
    int32_t sun_chg_power;
    int32_t ac_chg_power;
    int32_t dc_dsg_power;
    int32_t ac_dsg_power;
    int32_t usb_used_time;
    int32_t usb_qc_used_time;
    int32_t typec_used_time;
    int32_t car_used_time;
    int32_t inv_used_time;
    int32_t dc_in_used_time;
    int32_t mppt_used_time;
    uint16_t reverser;
    char screen_state[14];
    uint8_t ext_rj45_port;
    uint8_t ext_3p8_port;
    uint8_t ext_4p8_port;
    uint8_t syc_chg_dsg_state;
    uint8_t wifi_rssi;
    uint8_t wireless_watts;
    uint8_t charge_type;
    uint16_t ac_input_watts;
    uint16_t ac_output_watts;
    uint16_t dc_pv_input_watts;
    uint16_t dc_pv_output_watts;
    uint8_t cfg_ac_enabled;
    uint8_t pv_priority;
    uint8_t ac_auto_on;
    uint8_t watthis_config;
    uint8_t bp_power_soc;
    uint8_t hysteresis_soc;
    uint32_t reply_switchcnt;
    uint8_t ac_auto_out_config;
    uint8_t min_auto_soc;
    uint8_t ac_auto_out_pause;
    uint32_t schedule_id;
    uint32_t heartbeat_duration;
    uint16_t bkw_watts_in_power;
    uint8_t input_power_limit_flag;
    uint8_t ac_charge_flag;
    uint8_t cloud_ctrl_en;
    uint8_t redun_charge_flag;
};

struct Delta2EmsPacket {
    uint8_t chg_state;
    uint8_t chg_cmd;
    uint8_t dsg_cmd;
    uint32_t chg_vol;
    uint32_t chg_amp;
    uint8_t fan_level;
    uint8_t max_charge_soc;
    uint8_t bms_model;
    uint8_t lcd_show_soc;
    uint8_t open_ups_flag;
    uint8_t bms_warning_state;
    uint32_t chg_remain_time;
    uint32_t dsg_remain_time;
    uint8_t ems_is_normal_flag;
    float f32_lcd_show_soc;
    uint8_t bms_is_connt[3];
    uint8_t max_available_num;
    uint8_t open_bms_idx;
    uint32_t para_vol_min;
    uint32_t para_vol_max;
    uint8_t min_dsg_soc;
    uint8_t open_oil_eb_soc;
    uint8_t close_oil_eb_soc;
};

struct Delta2BmsPacket {
    uint8_t num;
    uint8_t type;
    uint8_t cell_id;
    uint32_t err_code;
    uint32_t sys_ver;
    uint8_t soc;
    uint32_t vol;
    uint32_t amp;
    uint8_t temp;
    uint8_t open_bms_idx;
    uint32_t design_cap;
    uint32_t remain_cap;
    uint32_t full_cap;
    uint32_t cycles;
    uint8_t soh;
    uint16_t max_cell_vol;
    uint16_t min_cell_vol;
    uint8_t max_cell_temp;
    uint8_t min_cell_temp;
    uint8_t max_mos_temp;
    uint8_t min_mos_temp;
    uint8_t bms_fault;
    uint8_t bq_sys_stat_reg;
    uint32_t tag_chg_amp;
    float f32_show_soc;
    uint32_t input_watts;
    uint32_t output_watts;
    uint32_t remain_time;
};

struct Delta2MpptPacket {
    uint32_t fault_code;
    char sw_ver[4];
    uint32_t in_vol;
    uint32_t in_amp;
    uint16_t in_watts;
    uint32_t out_vol;
    uint32_t out_amp;
    uint16_t out_watts;
    int16_t mppt_temp;
    uint8_t xt60_chg_type;
    uint8_t cfg_chg_type;
    uint8_t chg_type;
    uint8_t chg_state;
    uint32_t dcdc_12v_vol;
    uint32_t dcdc_12v_amp;
    uint16_t dcdc_12v_watts;
    uint32_t car_out_vol;
    uint32_t car_out_amp;
    uint16_t car_out_watts;
    int16_t car_temp;
    uint8_t car_state;
    int16_t dc24v_temp;
    uint8_t dc24v_state;
    uint8_t chg_pause_flag;
    uint32_t cfg_dc_chg_current;
    uint8_t beep_state;
    uint8_t cfg_ac_enabled;
    uint8_t cfg_ac_xboost;
    uint32_t cfg_ac_out_voltage;
    uint8_t cfg_ac_out_freq;
    uint16_t cfg_chg_watts;
    uint16_t ac_standby_mins;
    uint8_t discharge_type;
    uint16_t car_standby_mins;
    uint16_t power_standby_mins;
    uint16_t screen_standby_mins;
    uint16_t pay_flag;
    uint8_t reserved[8];
};

struct KitBaseInfoPacket {
    uint8_t avai_flag;
    char sn[16];
    uint16_t product_type;
    uint16_t product_detail;
    uint8_t procedure_state;
    uint32_t app_version;
    uint32_t loader_version;
    uint32_t cur_real_power;
    uint32_t f32_soc;
    uint8_t soc;
};

struct AllKitDetailDataPacket {
    uint8_t protocol_version;
    uint16_t available_data_len;
    uint16_t support_kit_max_num;
    // Followed by `support_kit_max_num` instances of KitBaseInfoPacket
};


#pragma pack(pop)

#endif // ECOFLOW_BINARY_MAP_H
