#include "ui_view_debug.h"
#include "ui_core.h"
#include "ui_lvgl.h"
#include "uart_task.h"
#include "fan_task.h"
#include "ui_view_fan.h"
#include "ui_view_log_manager.h"
#include <stdio.h>
#include "lvgl.h"

// Externs should match LVGL defines or be removed if included

void UI_CreateConnectionsView(void);

static lv_obj_t * scr_debug = NULL;
static lv_obj_t * cont_list = NULL;
static lv_obj_t * label_ip = NULL;
static lv_obj_t * label_conn_dev = NULL;
static lv_obj_t * label_paired_dev = NULL;

static lv_timer_t * debug_timer = NULL;
static DebugInfo last_debug_info = {0};

static void populate_debug_view(void);

static void refresh_debug_info(lv_timer_t * timer) {
    UART_SendGetDebugInfo();
    if (cont_list) {
        populate_debug_view();
    }
}

static void event_debug_cleanup(lv_event_t * e) {
    if (debug_timer) {
        lv_timer_del(debug_timer);
        debug_timer = NULL;
    }
    scr_debug = NULL;
    cont_list = NULL;
    label_ip = NULL;
    label_conn_dev = NULL;
    label_paired_dev = NULL;
}

static void event_close_debug(lv_event_t * e) {
    UI_LVGL_ShowSettings(true);
}

static void event_to_connections(lv_event_t * e) {
    UI_CreateConnectionsView();
}

static void event_to_fan(lv_event_t * e) {
    UI_CreateFanView();
}

static void event_to_log(lv_event_t * e) {
    UI_CreateLogManagerView();
}

static void add_list_item(lv_obj_t * parent, const char * name, const char * val) {
    lv_obj_t * item = lv_obj_create(parent);
    lv_obj_set_size(item, lv_pct(100), 40);
    lv_obj_set_style_bg_opa(item, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(item, 0, 0);
    lv_obj_set_style_pad_all(item, 0, 0);
    lv_obj_clear_flag(item, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * l_name = lv_label_create(item);
    lv_label_set_text(l_name, name);
    lv_obj_align(l_name, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_text_font(l_name, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l_name, lv_color_white(), 0);

    lv_obj_t * l_val = lv_label_create(item);
    lv_label_set_text(l_val, val);
    lv_obj_align(l_val, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_text_font(l_val, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l_val, lv_palette_main(LV_PALETTE_TEAL), 0);
}

static void add_section_header(lv_obj_t * parent, const char * title) {
    lv_obj_t * l = lv_label_create(parent);
    lv_label_set_text(l, title);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(l, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_obj_set_style_pad_top(l, 20, 0);
    lv_obj_set_style_pad_bottom(l, 5, 0);
}

extern DeviceStatus* UI_GetDeviceCache(int index);

// Helper to format float to string safely
static void fmt_float(char* buf, size_t len, float f, const char* suffix) {
    snprintf(buf, len, "%d%s", (int)f, suffix);
}

// Helper to format time (seconds -> Xd Xh Xm or Xh Xm or Xm)
static void fmt_time(char* buf, size_t len, uint32_t seconds) {
    if (seconds == 0) {
        snprintf(buf, len, "--");
        return;
    }
    int days = seconds / 86400;
    seconds %= 86400;
    int hours = seconds / 3600;
    seconds %= 3600;
    int minutes = seconds / 60;

    if (days > 0) {
        snprintf(buf, len, "%dD %dH %dM", days, hours, minutes);
    } else if (hours > 0) {
        snprintf(buf, len, "%dH %dM", hours, minutes);
    } else {
        snprintf(buf, len, "%dM", minutes);
    }
}

static void populate_debug_view(void) {
    if (!cont_list) return;

    // Save scroll position
    lv_coord_t scroll_y = lv_obj_get_scroll_y(cont_list);

    // Clear the list
    lv_obj_clean(cont_list);

    // --- System Info Section ---
    add_section_header(cont_list, "System Info");

    lv_obj_t * item_ip = lv_obj_create(cont_list);
    lv_obj_set_size(item_ip, lv_pct(100), 40);
    lv_obj_set_style_bg_opa(item_ip, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(item_ip, 0, 0);
    lv_obj_clear_flag(item_ip, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * l_ip_name = lv_label_create(item_ip);
    lv_label_set_text(l_ip_name, "ESP32 IP");
    lv_obj_align(l_ip_name, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_text_color(l_ip_name, lv_color_white(), 0);

    label_ip = lv_label_create(item_ip);
    if (last_debug_info.ip[0] != 0) {
        lv_label_set_text(label_ip, last_debug_info.ip);
    } else {
        lv_label_set_text(label_ip, "Loading...");
    }
    lv_obj_align(label_ip, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_text_color(label_ip, lv_palette_main(LV_PALETTE_TEAL), 0);


    lv_obj_t * item_conn = lv_obj_create(cont_list);
    lv_obj_set_size(item_conn, lv_pct(100), 40);
    lv_obj_set_style_bg_opa(item_conn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(item_conn, 0, 0);
    lv_obj_clear_flag(item_conn, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * l_conn_name = lv_label_create(item_conn);
    lv_label_set_text(l_conn_name, "Connected Devices");
    lv_obj_align(l_conn_name, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_text_color(l_conn_name, lv_color_white(), 0);

    label_conn_dev = lv_label_create(item_conn);
    lv_label_set_text_fmt(label_conn_dev, "%d", last_debug_info.devices_connected);
    lv_obj_align(label_conn_dev, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_text_color(label_conn_dev, lv_palette_main(LV_PALETTE_TEAL), 0);

    lv_obj_t * item_pair = lv_obj_create(cont_list);
    lv_obj_set_size(item_pair, lv_pct(100), 40);
    lv_obj_set_style_bg_opa(item_pair, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(item_pair, 0, 0);
    lv_obj_clear_flag(item_pair, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * l_pair_name = lv_label_create(item_pair);
    lv_label_set_text(l_pair_name, "Paired Devices");
    lv_obj_align(l_pair_name, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_text_color(l_pair_name, lv_color_white(), 0);

    label_paired_dev = lv_label_create(item_pair);
    lv_label_set_text_fmt(label_paired_dev, "%d", last_debug_info.devices_paired);
    lv_obj_align(label_paired_dev, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_text_color(label_paired_dev, lv_palette_main(LV_PALETTE_TEAL), 0);


    // --- Devices Section ---
    char buf[32];

    // Add Fan Info
    FanStatus fanStatus;
    Fan_GetStatus(&fanStatus);
    if (fanStatus.connected) {
        add_section_header(cont_list, "Fan Control (RP2040)");
        fmt_float(buf, sizeof(buf), fanStatus.amb_temp, " C");
        add_list_item(cont_list, "Amb Temp", buf);
        for(int i=0; i<4; i++) {
             char label[16];
             snprintf(label, sizeof(label), "Fan %d RPM", i+1);
             snprintf(buf, sizeof(buf), "%d", fanStatus.fan_rpm[i]);
             add_list_item(cont_list, label, buf);
        }
    }

    for (int i = 0; i < MAX_DEVICES; i++) {
        DeviceStatus* dev = UI_GetDeviceCache(i);
        if (dev && dev->connected) {
            snprintf(buf, sizeof(buf), "Device %d (%s)", dev->id, dev->name);
            add_section_header(cont_list, buf);

            if (dev->id == DEV_TYPE_DELTA_PRO_3) {
                 fmt_float(buf, sizeof(buf), dev->data.d3p.batteryLevel, "%");
                 add_list_item(cont_list, "Battery Level", buf);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.batteryLevelMain, "%");
                 add_list_item(cont_list, "Main Batt Level", buf);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.acInputPower, " W");
                 add_list_item(cont_list, "AC Input", buf);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.acLvOutputPower, " W");
                 add_list_item(cont_list, "AC LV Out", buf);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.acHvOutputPower, " W");
                 add_list_item(cont_list, "AC HV Out", buf);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.inputPower, " W");
                 add_list_item(cont_list, "Total Input", buf);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.outputPower, " W");
                 add_list_item(cont_list, "Total Output", buf);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.dc12vOutputPower, " W");
                 add_list_item(cont_list, "DC 12V Out", buf);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.dcLvInputPower, " W");
                 add_list_item(cont_list, "DC LV In", buf);
                 snprintf(buf, sizeof(buf), "%d", (int)dev->data.d3p.dcLvInputState);
                 add_list_item(cont_list, "DC LV State", buf);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.dcHvInputPower, " W");
                 add_list_item(cont_list, "DC HV In", buf);
                 snprintf(buf, sizeof(buf), "%d", (int)dev->data.d3p.dcHvInputState);
                 add_list_item(cont_list, "DC HV State", buf);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.solarLvPower, " W");
                 add_list_item(cont_list, "Solar LV", buf);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.solarHvPower, " W");
                 add_list_item(cont_list, "Solar HV", buf);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.expansion1Power, " W");
                 add_list_item(cont_list, "Expansion 1", buf);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.expansion2Power, " W");
                 add_list_item(cont_list, "Expansion 2", buf);
                 snprintf(buf, sizeof(buf), "%d", (int)dev->data.d3p.acInputStatus);
                 add_list_item(cont_list, "AC Input Status", buf);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.usbaOutputPower, " W");
                 add_list_item(cont_list, "USB-A Out", buf);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.usba2OutputPower, " W");
                 add_list_item(cont_list, "USB-A(2) Out", buf);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.usbcOutputPower, " W");
                 add_list_item(cont_list, "USB-C Out", buf);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.usbc2OutputPower, " W");
                 add_list_item(cont_list, "USB-C(2) Out", buf);
                 snprintf(buf, sizeof(buf), "%s", dev->data.d3p.pluggedInAc ? "Yes" : "No");
                 add_list_item(cont_list, "AC Plugged", buf);
                 snprintf(buf, sizeof(buf), "%d C", (int)dev->data.d3p.cellTemperature);
                 add_list_item(cont_list, "Cell Temp", buf);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.soh, "%");
                 add_list_item(cont_list, "SOH", buf);
                 fmt_time(buf, sizeof(buf), dev->data.d3p.dischargeRemainingTime);
                 add_list_item(cont_list, "Dsg Time", buf);
                 fmt_time(buf, sizeof(buf), dev->data.d3p.chargeRemainingTime);
                 add_list_item(cont_list, "Chg Time", buf);
            }
            else if (dev->id == DEV_TYPE_DELTA_3) {
                 fmt_float(buf, sizeof(buf), dev->data.d3.batteryLevel, "%");
                 add_list_item(cont_list, "Battery Level", buf);
                 fmt_float(buf, sizeof(buf), dev->data.d3.acInputPower, " W");
                 add_list_item(cont_list, "AC Input", buf);
                 fmt_float(buf, sizeof(buf), dev->data.d3.acOutputPower, " W");
                 add_list_item(cont_list, "AC Output", buf);
                 fmt_float(buf, sizeof(buf), dev->data.d3.solarInputPower, " W");
                 add_list_item(cont_list, "Solar Input", buf);
                 fmt_float(buf, sizeof(buf), dev->data.d3.dc12vOutputPower, " W");
                 add_list_item(cont_list, "DC 12V Out", buf);
                 snprintf(buf, sizeof(buf), "%d C", (int)dev->data.d3.cellTemperature);
                 add_list_item(cont_list, "Cell Temp", buf);
            }
            else if (dev->id == DEV_TYPE_WAVE_2) {
                 snprintf(buf, sizeof(buf), "%d", (int)dev->data.w2.mode);
                 add_list_item(cont_list, "Mode", buf);
                 snprintf(buf, sizeof(buf), "%d", (int)dev->data.w2.setTemp);
                 add_list_item(cont_list, "Set Temp", buf);
                 fmt_float(buf, sizeof(buf), dev->data.w2.envTemp, " C");
                 add_list_item(cont_list, "Env Temp", buf);
                 snprintf(buf, sizeof(buf), "%d %%", (int)dev->data.w2.batSoc);
                 add_list_item(cont_list, "Bat SOC", buf);
                 snprintf(buf, sizeof(buf), "%d W", (int)dev->data.w2.batPwrWatt);
                 add_list_item(cont_list, "Bat Power", buf);
            }
            else if (dev->id == DEV_TYPE_ALT_CHARGER) {
                 fmt_float(buf, sizeof(buf), dev->data.ac.batteryLevel, "%");
                 add_list_item(cont_list, "Battery Level", buf);
                 fmt_float(buf, sizeof(buf), dev->data.ac.dcPower, " W");
                 add_list_item(cont_list, "DC Power", buf);
                 fmt_float(buf, sizeof(buf), dev->data.ac.carBatteryVoltage, " V");
                 add_list_item(cont_list, "Car Batt Volt", buf);
                 fmt_float(buf, sizeof(buf), dev->data.ac.startVoltage, " V");
                 add_list_item(cont_list, "Start Volt", buf);
                 snprintf(buf, sizeof(buf), "%d", (int)dev->data.ac.chargerMode);
                 add_list_item(cont_list, "Mode", buf);
                 snprintf(buf, sizeof(buf), "%s", dev->data.ac.chargerOpen ? "ON" : "OFF");
                 add_list_item(cont_list, "Charger Open", buf);
                 snprintf(buf, sizeof(buf), "%d W", (int)dev->data.ac.powerLimit);
                 add_list_item(cont_list, "Power Limit", buf);
                 fmt_float(buf, sizeof(buf), dev->data.ac.chargingCurrentLimit, " A");
                 add_list_item(cont_list, "Chg Limit", buf);
                 fmt_float(buf, sizeof(buf), dev->data.ac.reverseChargingCurrentLimit, " A");
                 add_list_item(cont_list, "Rev Chg Limit", buf);
            }
        }
    }

    // Restore scroll position
    lv_obj_scroll_to_y(cont_list, scroll_y, LV_ANIM_OFF);
}

void UI_CreateDebugView(void) {
    if (scr_debug) {
        lv_scr_load(scr_debug);
        // Ensure timer is running if reopened (though we cleanup on delete)
        if (!debug_timer) {
            debug_timer = lv_timer_create(refresh_debug_info, 5000, NULL);
        }
        return;
    }

    scr_debug = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_debug, lv_color_hex(0xFF121212), 0);
    lv_obj_set_style_text_color(scr_debug, lv_color_white(), 0);

    // Header
    lv_obj_t * header = lv_obj_create(scr_debug);
    lv_obj_set_size(header, lv_pct(100), 60);
    lv_obj_set_style_bg_color(header, lv_color_hex(0xFF202020), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

    // Title Centered
    lv_obj_t * title = lv_label_create(header);
    lv_label_set_text(title, "Debug Info");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    // Manage Connections Button (Top Left)
    lv_obj_t * btn_manage = lv_btn_create(header);
    lv_obj_set_size(btn_manage, 220, 40);
    lv_obj_align(btn_manage, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_bg_color(btn_manage, lv_palette_main(LV_PALETTE_BLUE), 0);
    lv_obj_add_event_cb(btn_manage, event_to_connections, LV_EVENT_CLICKED, NULL);

    lv_obj_t * lbl_manage = lv_label_create(btn_manage);
    lv_label_set_text(lbl_manage, "Connections");
    lv_obj_center(lbl_manage);

    // Fan Control Button (Next to Connections)
    lv_obj_t * btn_fan = lv_btn_create(header);
    lv_obj_set_size(btn_fan, 180, 40);
    lv_obj_align(btn_fan, LV_ALIGN_LEFT_MID, 240, 0);
    lv_obj_set_style_bg_color(btn_fan, lv_palette_main(LV_PALETTE_PURPLE), 0);
    lv_obj_add_event_cb(btn_fan, event_to_fan, LV_EVENT_CLICKED, NULL);

    lv_obj_t * lbl_fan = lv_label_create(btn_fan);
    lv_label_set_text(lbl_fan, "Fan Control");
    lv_obj_center(lbl_fan);

    // Log Manager Button
    lv_obj_t * btn_log = lv_btn_create(header);
    lv_obj_set_size(btn_log, 140, 40);
    lv_obj_align(btn_log, LV_ALIGN_LEFT_MID, 440, 0);
    lv_obj_set_style_bg_color(btn_log, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_add_event_cb(btn_log, event_to_log, LV_EVENT_CLICKED, NULL);

    lv_obj_t * lbl_log = lv_label_create(btn_log);
    lv_label_set_text(lbl_log, "Log Mgr");
    lv_obj_center(lbl_log);

    // Close Button (Top Right)
    lv_obj_t * btn_close = lv_btn_create(header);
    lv_obj_set_size(btn_close, 40, 40);
    lv_obj_align(btn_close, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_bg_color(btn_close, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_clear_flag(btn_close, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn_close, event_close_debug, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_x = lv_label_create(btn_close);
    lv_label_set_text(lbl_x, "X");
    lv_obj_center(lbl_x);

    // List Container
    cont_list = lv_obj_create(scr_debug);
    lv_obj_set_size(cont_list, lv_pct(100), lv_pct(85));
    lv_obj_align(cont_list, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(cont_list, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(cont_list, LV_FLEX_FLOW_COLUMN);

    // Initial population of device list
    populate_debug_view();

    lv_obj_add_event_cb(scr_debug, event_debug_cleanup, LV_EVENT_DELETE, NULL);

    lv_scr_load(scr_debug);

    // Request Update from ESP32
    UART_SendGetDebugInfo();

    // Start Auto-Refresh Timer (5s)
    debug_timer = lv_timer_create(refresh_debug_info, 5000, NULL);
}

void UI_UpdateDebugInfo(DebugInfo* info) {
    if (!info) return;

    // Cache the info
    memcpy(&last_debug_info, info, sizeof(DebugInfo));

    // Update UI if valid
    if (scr_debug) {
        if (label_ip) lv_label_set_text(label_ip, info->ip);
        if (label_conn_dev) lv_label_set_text_fmt(label_conn_dev, "%d", info->devices_connected);
        if (label_paired_dev) lv_label_set_text_fmt(label_paired_dev, "%d", info->devices_paired);
    }

}
