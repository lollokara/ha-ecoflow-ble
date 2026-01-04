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

// Find existing item by name or create new one
static lv_obj_t* get_or_create_item(lv_obj_t * parent, const char * name, int index) {
    if (lv_obj_get_child_cnt(parent) > index) {
        lv_obj_t* item = lv_obj_get_child(parent, index);
        // Verify name matches (child 0 is name label)
        lv_obj_t* l_name = lv_obj_get_child(item, 0);
        if (strcmp(lv_label_get_text(l_name), name) == 0) {
            return item;
        }
        // If mismatch, we might need to clean but let's assume structure is static for now
        // Or we just update text regardless of name if index matches (risky if list changes)
        // Ideally we search children
    }

    // Fallback: Create new
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
    lv_label_set_text(l_val, "--");
    lv_obj_align(l_val, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_text_font(l_val, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l_val, lv_palette_main(LV_PALETTE_TEAL), 0);

    return item;
}

static void update_list_item(lv_obj_t * parent, const char * name, const char * val, int* index) {
    lv_obj_t* item = get_or_create_item(parent, name, *index);
    lv_obj_t* l_val = lv_obj_get_child(item, 1); // 2nd child is value
    lv_label_set_text(l_val, val);
    (*index)++;
}

static void add_section_header(lv_obj_t * parent, const char * title, int* index) {
    lv_obj_t * l;
    if (lv_obj_get_child_cnt(parent) > *index) {
        l = lv_obj_get_child(parent, *index);
        // Assuming types match if count matches logic
        if (!lv_obj_check_type(l, &lv_label_class)) {
             // Mismatch, recreate? For now assume sync.
        }
    } else {
        l = lv_label_create(parent);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(l, lv_palette_main(LV_PALETTE_ORANGE), 0);
        lv_obj_set_style_pad_top(l, 20, 0);
        lv_obj_set_style_pad_bottom(l, 5, 0);
    }
    lv_label_set_text(l, title);
    (*index)++;
}

extern DeviceStatus* UI_GetDeviceCache(int index);

static void fmt_float(char* buf, size_t len, float f, const char* suffix) {
    snprintf(buf, len, "%d%s", (int)f, suffix);
}

static void populate_debug_view(void) {
    if (!cont_list) return;

    int child_idx = 0;

    // --- System Info Section ---
    // Manually handled items at top of list
    // Actually we can reuse update_list_item if we treat them as part of the flow
    add_section_header(cont_list, "System Info", &child_idx);

    char buf[64];
    if (last_debug_info.ip[0] != 0) snprintf(buf, sizeof(buf), "%s", last_debug_info.ip);
    else snprintf(buf, sizeof(buf), "Loading...");
    update_list_item(cont_list, "ESP32 IP", buf, &child_idx);

    snprintf(buf, sizeof(buf), "%d", last_debug_info.devices_connected);
    update_list_item(cont_list, "Connected Devices", buf, &child_idx);

    snprintf(buf, sizeof(buf), "%d", last_debug_info.devices_paired);
    update_list_item(cont_list, "Paired Devices", buf, &child_idx);

    // --- Devices Section ---
    // Add Fan Info
    FanStatus fanStatus;
    Fan_GetStatus(&fanStatus);
    if (fanStatus.connected) {
        add_section_header(cont_list, "Fan Control (RP2040)", &child_idx);
        fmt_float(buf, sizeof(buf), fanStatus.amb_temp, " C");
        update_list_item(cont_list, "Amb Temp", buf, &child_idx);
        for(int i=0; i<4; i++) {
             char label[16];
             snprintf(label, sizeof(label), "Fan %d RPM", i+1);
             snprintf(buf, sizeof(buf), "%d", fanStatus.fan_rpm[i]);
             update_list_item(cont_list, label, buf, &child_idx);
        }
    }

    for (int i = 0; i < MAX_DEVICES; i++) {
        DeviceStatus* dev = UI_GetDeviceCache(i);
        if (dev && dev->connected) {
            snprintf(buf, sizeof(buf), "Device %d (%s)", dev->id, dev->name);
            add_section_header(cont_list, buf, &child_idx);

            if (dev->id == DEV_TYPE_DELTA_PRO_3) {
                 fmt_float(buf, sizeof(buf), dev->data.d3p.batteryLevel, "%");
                 update_list_item(cont_list, "Battery Level", buf, &child_idx);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.batteryLevelMain, "%");
                 update_list_item(cont_list, "Main Batt Level", buf, &child_idx);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.acInputPower, " W");
                 update_list_item(cont_list, "AC Input", buf, &child_idx);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.acLvOutputPower, " W");
                 update_list_item(cont_list, "AC LV Out", buf, &child_idx);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.acHvOutputPower, " W");
                 update_list_item(cont_list, "AC HV Out", buf, &child_idx);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.inputPower, " W");
                 update_list_item(cont_list, "Total Input", buf, &child_idx);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.outputPower, " W");
                 update_list_item(cont_list, "Total Output", buf, &child_idx);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.dc12vOutputPower, " W");
                 update_list_item(cont_list, "DC 12V Out", buf, &child_idx);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.dcLvInputPower, " W");
                 update_list_item(cont_list, "DC LV In", buf, &child_idx);
                 snprintf(buf, sizeof(buf), "%d", (int)dev->data.d3p.dcLvInputState);
                 update_list_item(cont_list, "DC LV State", buf, &child_idx);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.dcHvInputPower, " W");
                 update_list_item(cont_list, "DC HV In", buf, &child_idx);
                 snprintf(buf, sizeof(buf), "%d", (int)dev->data.d3p.dcHvInputState);
                 update_list_item(cont_list, "DC HV State", buf, &child_idx);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.solarLvPower, " W");
                 update_list_item(cont_list, "Solar LV", buf, &child_idx);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.solarHvPower, " W");
                 update_list_item(cont_list, "Solar HV", buf, &child_idx);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.usbaOutputPower, " W");
                 update_list_item(cont_list, "USB-A Out", buf, &child_idx);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.usba2OutputPower, " W");
                 update_list_item(cont_list, "USB-A(2) Out", buf, &child_idx);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.usbcOutputPower, " W");
                 update_list_item(cont_list, "USB-C Out", buf, &child_idx);
                 fmt_float(buf, sizeof(buf), dev->data.d3p.usbc2OutputPower, " W");
                 update_list_item(cont_list, "USB-C(2) Out", buf, &child_idx);
                 snprintf(buf, sizeof(buf), "%s", dev->data.d3p.pluggedInAc ? "Yes" : "No");
                 update_list_item(cont_list, "AC Plugged", buf, &child_idx);
                 snprintf(buf, sizeof(buf), "%d C", (int)dev->data.d3p.cellTemperature);
                 update_list_item(cont_list, "Cell Temp", buf, &child_idx);
            }
            else if (dev->id == DEV_TYPE_DELTA_3) {
                 fmt_float(buf, sizeof(buf), dev->data.d3.batteryLevel, "%");
                 update_list_item(cont_list, "Battery Level", buf, &child_idx);
                 fmt_float(buf, sizeof(buf), dev->data.d3.acInputPower, " W");
                 update_list_item(cont_list, "AC Input", buf, &child_idx);
                 fmt_float(buf, sizeof(buf), dev->data.d3.acOutputPower, " W");
                 update_list_item(cont_list, "AC Output", buf, &child_idx);
                 fmt_float(buf, sizeof(buf), dev->data.d3.solarInputPower, " W");
                 update_list_item(cont_list, "Solar Input", buf, &child_idx);
                 fmt_float(buf, sizeof(buf), dev->data.d3.dc12vOutputPower, " W");
                 update_list_item(cont_list, "DC 12V Out", buf, &child_idx);
                 snprintf(buf, sizeof(buf), "%d C", (int)dev->data.d3.cellTemperature);
                 update_list_item(cont_list, "Cell Temp", buf, &child_idx);
            }
            else if (dev->id == DEV_TYPE_WAVE_2) {
                 snprintf(buf, sizeof(buf), "%d", (int)dev->data.w2.mode);
                 update_list_item(cont_list, "Mode", buf, &child_idx);
                 snprintf(buf, sizeof(buf), "%d", (int)dev->data.w2.setTemp);
                 update_list_item(cont_list, "Set Temp", buf, &child_idx);
                 fmt_float(buf, sizeof(buf), dev->data.w2.envTemp, " C");
                 update_list_item(cont_list, "Env Temp", buf, &child_idx);
                 snprintf(buf, sizeof(buf), "%d %%", (int)dev->data.w2.batSoc);
                 update_list_item(cont_list, "Bat SOC", buf, &child_idx);
                 snprintf(buf, sizeof(buf), "%d W", (int)dev->data.w2.batPwrWatt);
                 update_list_item(cont_list, "Bat Power", buf, &child_idx);
            }
            else if (dev->id == DEV_TYPE_ALT_CHARGER) {
                 fmt_float(buf, sizeof(buf), dev->data.ac.batteryLevel, "%");
                 update_list_item(cont_list, "Battery Level", buf, &child_idx);
                 fmt_float(buf, sizeof(buf), dev->data.ac.dcPower, " W");
                 update_list_item(cont_list, "DC Power", buf, &child_idx);
                 fmt_float(buf, sizeof(buf), dev->data.ac.carBatteryVoltage, " V");
                 update_list_item(cont_list, "Car Batt Volt", buf, &child_idx);
                 fmt_float(buf, sizeof(buf), dev->data.ac.startVoltage, " V");
                 update_list_item(cont_list, "Start Volt", buf, &child_idx);
                 snprintf(buf, sizeof(buf), "%d", (int)dev->data.ac.chargerMode);
                 update_list_item(cont_list, "Mode", buf, &child_idx);
                 snprintf(buf, sizeof(buf), "%s", dev->data.ac.chargerOpen ? "ON" : "OFF");
                 update_list_item(cont_list, "Charger Open", buf, &child_idx);
                 snprintf(buf, sizeof(buf), "%d W", (int)dev->data.ac.powerLimit);
                 update_list_item(cont_list, "Power Limit", buf, &child_idx);
                 fmt_float(buf, sizeof(buf), dev->data.ac.chargingCurrentLimit, " A");
                 update_list_item(cont_list, "Chg Limit", buf, &child_idx);
                 fmt_float(buf, sizeof(buf), dev->data.ac.reverseChargingCurrentLimit, " A");
                 update_list_item(cont_list, "Rev Chg Limit", buf, &child_idx);
            }
        }
    }

    // Clean up excess items if device list shrank
    while(lv_obj_get_child_cnt(cont_list) > child_idx) {
        // Delete last child
        lv_obj_del(lv_obj_get_child(cont_list, child_idx));
    }
}

void UI_CreateDebugView(void) {
    if (scr_debug) {
        lv_scr_load(scr_debug);
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
    if (scr_debug && cont_list) {
        populate_debug_view();
    }
}
