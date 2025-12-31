#include "ui_view_debug.h"
#include "ui_core.h"
#include "ui_lvgl.h"
#include "uart_task.h"
#include "fan_task.h"
#include "ui_view_fan.h"
#include <stdio.h>
#include "lvgl.h"

// Externs should match LVGL defines or be removed if included

void UI_CreateConnectionsView(void);

// Externs not needed if lvgl.h is included properly and fonts are enabled in lv_conf.h
// If needed, they should be const

static lv_obj_t * scr_debug = NULL;
static lv_obj_t * cont_list = NULL;
static lv_obj_t * label_ip = NULL;
static lv_obj_t * label_conn_dev = NULL;
static lv_obj_t * label_paired_dev = NULL;

static void event_debug_cleanup(lv_event_t * e) {
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

static void add_list_item(lv_obj_t * parent, const char * name, const char * val) {
    lv_obj_t * item = lv_obj_create(parent);
    lv_obj_set_size(item, lv_pct(100), 40);
    lv_obj_set_style_bg_opa(item, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(item, 0, 0);
    lv_obj_set_style_pad_all(item, 0, 0);

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

static void populate_device_list(void) {
    char buf[32];
    for (int i = 0; i < MAX_DEVICES; i++) {
        DeviceStatus* dev = UI_GetDeviceCache(i);
        if (dev && dev->connected) {
            snprintf(buf, sizeof(buf), "Device %d (%s)", dev->id, dev->name);
            add_section_header(cont_list, buf);

            if (dev->id == DEV_TYPE_DELTA_PRO_3) {
                 fmt_float(buf, sizeof(buf), dev->data.d3p.batteryLevel, "%");
                 add_list_item(cont_list, "Battery Level", buf);
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
                 fmt_float(buf, sizeof(buf), dev->data.d3p.dcHvInputPower, " W");
                 add_list_item(cont_list, "DC HV In", buf);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.solarLvPower, " W");
                 add_list_item(cont_list, "Solar LV", buf);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.solarHvPower, " W");
                 add_list_item(cont_list, "Solar HV", buf);
                 snprintf(buf, sizeof(buf), "%d C", dev->data.d3p.cellTemperature);
                 add_list_item(cont_list, "Cell Temp", buf);
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
                 snprintf(buf, sizeof(buf), "%d C", dev->data.d3.cellTemperature);
                 add_list_item(cont_list, "Cell Temp", buf);
            }
            else if (dev->id == DEV_TYPE_WAVE_2) {
                 snprintf(buf, sizeof(buf), "%d", dev->data.w2.mode);
                 add_list_item(cont_list, "Mode", buf);
                 snprintf(buf, sizeof(buf), "%d", dev->data.w2.setTemp);
                 add_list_item(cont_list, "Set Temp", buf);
                 fmt_float(buf, sizeof(buf), dev->data.w2.envTemp, " C");
                 add_list_item(cont_list, "Env Temp", buf);
                 snprintf(buf, sizeof(buf), "%d %%", dev->data.w2.batSoc);
                 add_list_item(cont_list, "Bat SOC", buf);
                 snprintf(buf, sizeof(buf), "%d W", dev->data.w2.batPwrWatt);
                 add_list_item(cont_list, "Bat Power", buf);
            }
        }
    }
}

void UI_CreateDebugView(void) {
    if (scr_debug) {
        lv_scr_load(scr_debug);
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
    lv_label_set_text(lbl_manage, "Manage Connections");
    lv_obj_center(lbl_manage);

    // Fan Control Button (Next to Manage Connections)
    lv_obj_t * btn_fan = lv_btn_create(header);
    lv_obj_set_size(btn_fan, 150, 40);
    lv_obj_align(btn_fan, LV_ALIGN_LEFT_MID, 240, 0);
    lv_obj_set_style_bg_color(btn_fan, lv_palette_main(LV_PALETTE_TEAL), 0);
    lv_obj_add_event_cb(btn_fan, event_to_fan, LV_EVENT_CLICKED, NULL);

    lv_obj_t * lbl_fan = lv_label_create(btn_fan);
    lv_label_set_text(lbl_fan, "FAN Control");
    lv_obj_center(lbl_fan);

    // Close Button (Top Right)
    lv_obj_t * btn_close = lv_btn_create(header);
    lv_obj_clear_flag(btn_close, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_size(btn_close, 40, 40);
    lv_obj_align(btn_close, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_bg_color(btn_close, lv_palette_main(LV_PALETTE_RED), 0);
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

    // System Info Section
    add_section_header(cont_list, "System Info");

    lv_obj_t * item_ip = lv_obj_create(cont_list);
    lv_obj_set_size(item_ip, lv_pct(100), 40);
    lv_obj_set_style_bg_opa(item_ip, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(item_ip, 0, 0);

    lv_obj_t * l_ip_name = lv_label_create(item_ip);
    lv_label_set_text(l_ip_name, "ESP32 IP");
    lv_obj_align(l_ip_name, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_text_color(l_ip_name, lv_color_white(), 0);

    label_ip = lv_label_create(item_ip);
    lv_label_set_text(label_ip, "Loading...");
    lv_obj_align(label_ip, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_text_color(label_ip, lv_palette_main(LV_PALETTE_TEAL), 0);


    lv_obj_t * item_conn = lv_obj_create(cont_list);
    lv_obj_set_size(item_conn, lv_pct(100), 40);
    lv_obj_set_style_bg_opa(item_conn, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(item_conn, 0, 0);

    lv_obj_t * l_conn_name = lv_label_create(item_conn);
    lv_label_set_text(l_conn_name, "Connected Devices");
    lv_obj_align(l_conn_name, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_text_color(l_conn_name, lv_color_white(), 0);

    label_conn_dev = lv_label_create(item_conn);
    lv_label_set_text(label_conn_dev, "--");
    lv_obj_align(label_conn_dev, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_text_color(label_conn_dev, lv_palette_main(LV_PALETTE_TEAL), 0);

    lv_obj_t * item_pair = lv_obj_create(cont_list);
    lv_obj_set_size(item_pair, lv_pct(100), 40);
    lv_obj_set_style_bg_opa(item_pair, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(item_pair, 0, 0);

    lv_obj_t * l_pair_name = lv_label_create(item_pair);
    lv_label_set_text(l_pair_name, "Paired Devices");
    lv_obj_align(l_pair_name, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_text_color(l_pair_name, lv_color_white(), 0);

    label_paired_dev = lv_label_create(item_pair);
    lv_label_set_text(label_paired_dev, "--");
    lv_obj_align(label_paired_dev, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_text_color(label_paired_dev, lv_palette_main(LV_PALETTE_TEAL), 0);

    // Fan Info
    add_section_header(cont_list, "Fan Controller");
    FanStatus* fstatus = Fan_GetStatus();
    if (fstatus) {
        char buf[32]; // Declare buf here
        snprintf(buf, sizeof(buf), "%.1f C", fstatus->amb_temp);
        add_list_item(cont_list, "Ambient Temp", buf);
        for(int i=0; i<4; i++) {
             char label[16];
             snprintf(label, sizeof(label), "Fan %d RPM", i+1);
             snprintf(buf, sizeof(buf), "%d", fstatus->fan_rpm[i]);
             add_list_item(cont_list, label, buf);
        }
    } else {
        add_list_item(cont_list, "Status", "Not Initialized");
    }

    // Initial population of device list
    populate_device_list();

    lv_obj_add_event_cb(scr_debug, event_debug_cleanup, LV_EVENT_DELETE, NULL);

    lv_scr_load(scr_debug);

    // Request Update from ESP32
    UART_SendGetDebugInfo();
}

void UI_UpdateDebugInfo(DebugInfo* info) {
    if (!scr_debug || !info) return;

    if (label_ip) lv_label_set_text(label_ip, info->ip);
    if (label_conn_dev) lv_label_set_text_fmt(label_conn_dev, "%d", info->devices_connected);
    if (label_paired_dev) lv_label_set_text_fmt(label_paired_dev, "%d", info->devices_paired);
}
