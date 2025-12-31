#include "ui_view_fan.h"
#include "ui_view_debug.h"
#include "ui_core.h"
#include "ui_lvgl.h"
#include "rp2040_task.h"
#include <stdio.h>
#include "lvgl.h"

static lv_obj_t * scr_fan = NULL;
static lv_obj_t * dd_group = NULL;
static lv_obj_t * slider_min_speed = NULL;
static lv_obj_t * slider_max_speed = NULL;
static lv_obj_t * slider_start_temp = NULL;
static lv_obj_t * slider_max_temp = NULL;
static lv_obj_t * lbl_min_speed_val = NULL;
static lv_obj_t * lbl_max_speed_val = NULL;
static lv_obj_t * lbl_start_temp_val = NULL;
static lv_obj_t * lbl_max_temp_val = NULL;

// Local store for editing before save
static FanGroupConfig local_config[2];
static uint8_t current_group = 0;
static bool config_received[2] = {false, false};

static void update_sliders(void);

// Callback from UART Task
static void on_config_received(uint8_t group, FanGroupConfig* config) {
    if (group >= 2) return;
    local_config[group] = *config;
    config_received[group] = true;

    if (scr_fan && group == current_group) {
        update_sliders();
    }
}

static void update_sliders(void) {
    if (!scr_fan) return;
    FanGroupConfig* cfg = &local_config[current_group];

    if (!config_received[current_group]) {
        // Request if not valid
        RP2040_RequestConfig(current_group);
    }

    lv_slider_set_value(slider_min_speed, cfg->min_speed, LV_ANIM_OFF);
    lv_label_set_text_fmt(lbl_min_speed_val, "%d RPM", cfg->min_speed);

    lv_slider_set_value(slider_max_speed, cfg->max_speed, LV_ANIM_OFF);
    lv_label_set_text_fmt(lbl_max_speed_val, "%d RPM", cfg->max_speed);

    lv_slider_set_value(slider_start_temp, cfg->start_temp, LV_ANIM_OFF);
    lv_label_set_text_fmt(lbl_start_temp_val, "%d C", cfg->start_temp);

    lv_slider_set_value(slider_max_temp, cfg->max_temp, LV_ANIM_OFF);
    lv_label_set_text_fmt(lbl_max_temp_val, "%d C", cfg->max_temp);
}

static void event_group_change(lv_event_t * e) {
    uint16_t idx = lv_dropdown_get_selected(dd_group);
    if (idx < 2) {
        current_group = idx;
        RP2040_RequestConfig(current_group); // Always refresh on switch
        update_sliders();
    }
}

static void event_slider_change(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    FanGroupConfig* cfg = &local_config[current_group];

    if (slider == slider_min_speed) {
        cfg->min_speed = lv_slider_get_value(slider);
        lv_label_set_text_fmt(lbl_min_speed_val, "%d RPM", cfg->min_speed);
    } else if (slider == slider_max_speed) {
        cfg->max_speed = lv_slider_get_value(slider);
        lv_label_set_text_fmt(lbl_max_speed_val, "%d RPM", cfg->max_speed);
    } else if (slider == slider_start_temp) {
        cfg->start_temp = (uint8_t)lv_slider_get_value(slider);
        // Ensure Max > Start
        if (cfg->max_temp <= cfg->start_temp) {
             cfg->max_temp = cfg->start_temp + 1;
             lv_slider_set_value(slider_max_temp, cfg->max_temp, LV_ANIM_ON);
             lv_label_set_text_fmt(lbl_max_temp_val, "%d C", cfg->max_temp);
        }
        lv_label_set_text_fmt(lbl_start_temp_val, "%d C", cfg->start_temp);
    } else if (slider == slider_max_temp) {
        cfg->max_temp = (uint8_t)lv_slider_get_value(slider);
        // Ensure Max > Start
        if (cfg->max_temp <= cfg->start_temp) {
             cfg->start_temp = cfg->max_temp - 1;
             lv_slider_set_value(slider_start_temp, cfg->start_temp, LV_ANIM_ON);
             lv_label_set_text_fmt(lbl_start_temp_val, "%d C", cfg->start_temp);
        }
        lv_label_set_text_fmt(lbl_max_temp_val, "%d C", cfg->max_temp);
    }
}

static void event_save(lv_event_t * e) {
    RP2040_SendConfig(current_group, &local_config[current_group]);
}

static void event_back(lv_event_t * e) {
    UI_CreateDebugView();
}

static void event_cleanup(lv_event_t * e) {
    scr_fan = NULL;
}

static void create_slider_row(lv_obj_t * parent, const char * title, int min, int max, int step, lv_obj_t ** slider_out, lv_obj_t ** lbl_out) {
    lv_obj_t * cont = lv_obj_create(parent);
    lv_obj_set_size(cont, lv_pct(100), 80);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * lbl = lv_label_create(cont);
    lv_label_set_text(lbl, title);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 10, 5);
    lv_obj_set_style_text_color(lbl, lv_color_white(), 0);

    *lbl_out = lv_label_create(cont);
    lv_obj_align(*lbl_out, LV_ALIGN_TOP_RIGHT, -10, 5);
    lv_obj_set_style_text_color(*lbl_out, lv_palette_main(LV_PALETTE_TEAL), 0);

    *slider_out = lv_slider_create(cont);
    lv_obj_set_size(*slider_out, lv_pct(90), 20);
    lv_obj_align(*slider_out, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_slider_set_range(*slider_out, min, max);
    // Setting step doesn't force snap in v8 without custom event logic, but visual range helps
    // We handle snap in logic if needed, or rely on integer increments.
    // User asked for intervals. We can use logic in event callback if needed but slider granularity is usually 1.
    // We can multiply/divide if needed. But here 100RPM steps can be done by range 5-30 * 100?
    // Let's use range directly and snap in callback? Or just let it be 1 unit.
    // User: "intervals of 100RPM".
    // Slider range 500-3000. 2500 steps.
    // Let's just allow 1 RPM resolution or check event.
    // To enforcing 100 step:
    // lv_slider_set_range(s, 5, 30); value * 100 for display.
    // Let's do that for RPM.
    // Wait, user said "Min Fan Speed 500-3000".

    if (max > 100) { // It's RPM
       lv_slider_set_range(*slider_out, min/100, max/100);
    } else { // Temp
       lv_slider_set_range(*slider_out, min, max);
    }

    lv_obj_add_event_cb(*slider_out, event_slider_change, LV_EVENT_VALUE_CHANGED, NULL);
}

// Wrapper to handle scale
static void event_slider_change_wrapper(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    FanGroupConfig* cfg = &local_config[current_group];
    int val = lv_slider_get_value(slider);

    if (slider == slider_min_speed) {
        cfg->min_speed = val * 100;
        lv_label_set_text_fmt(lbl_min_speed_val, "%d RPM", cfg->min_speed);
    } else if (slider == slider_max_speed) {
        cfg->max_speed = val * 100;
        lv_label_set_text_fmt(lbl_max_speed_val, "%d RPM", cfg->max_speed);
    } else {
        event_slider_change(e); // Pass through for Temp (scale 1)
    }
}

void UI_CreateFanView(void) {
    if (scr_fan) {
        RP2040_RequestConfig(current_group);
        lv_scr_load(scr_fan);
        return;
    }

    RP2040_SetConfigCallback(on_config_received);

    // Init defaults to prevent empty sliders if RP2040 is offline
    for(int i=0; i<2; i++) {
        if(local_config[i].max_speed == 0) {
            local_config[i].min_speed = 500;
            local_config[i].max_speed = 3000;
            local_config[i].start_temp = 30;
            local_config[i].max_temp = 40;
        }
    }

    scr_fan = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_fan, lv_color_hex(0xFF121212), 0);
    lv_obj_set_style_text_color(scr_fan, lv_color_white(), 0);

    // Header
    lv_obj_t * header = lv_obj_create(scr_fan);
    lv_obj_set_size(header, lv_pct(100), 60);
    lv_obj_set_style_bg_color(header, lv_color_hex(0xFF202020), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t * title = lv_label_create(header);
    lv_label_set_text(title, "Fan Control");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    // Content
    lv_obj_t * cont = lv_obj_create(scr_fan);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(85));
    lv_obj_align(cont, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);

    // Group Selector
    lv_obj_t * row_group = lv_obj_create(cont);
    lv_obj_set_size(row_group, lv_pct(100), 60);
    lv_obj_set_style_bg_opa(row_group, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row_group, 0, 0);

    lv_obj_t * l_grp = lv_label_create(row_group);
    lv_label_set_text(l_grp, "Select Group:");
    lv_obj_align(l_grp, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_text_color(l_grp, lv_color_white(), 0);

    dd_group = lv_dropdown_create(row_group);
    lv_dropdown_set_options(dd_group, "Group 1 (Fans 1-2)\nGroup 2 (Fans 3-4)");
    lv_obj_align(dd_group, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_add_event_cb(dd_group, event_group_change, LV_EVENT_VALUE_CHANGED, NULL);

    // Sliders
    create_slider_row(cont, "Min Fan Speed", 500, 3000, 100, &slider_min_speed, &lbl_min_speed_val);
    create_slider_row(cont, "Max Fan Speed", 3000, 5000, 100, &slider_max_speed, &lbl_max_speed_val);
    create_slider_row(cont, "Start Temp", 30, 45, 1, &slider_start_temp, &lbl_start_temp_val);
    create_slider_row(cont, "Max Temp", 40, 60, 1, &slider_max_temp, &lbl_max_temp_val);

    // Fix callbacks for RPM scaling
    lv_obj_remove_event_cb(slider_min_speed, event_slider_change);
    lv_obj_add_event_cb(slider_min_speed, event_slider_change_wrapper, LV_EVENT_VALUE_CHANGED, NULL);

    lv_obj_remove_event_cb(slider_max_speed, event_slider_change);
    lv_obj_add_event_cb(slider_max_speed, event_slider_change_wrapper, LV_EVENT_VALUE_CHANGED, NULL);

    // Buttons
    lv_obj_t * btn_row = lv_obj_create(cont);
    lv_obj_set_size(btn_row, lv_pct(100), 80);
    lv_obj_set_style_bg_opa(btn_row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(btn_row, 0, 0);
    lv_obj_clear_flag(btn_row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * btn_save = lv_btn_create(btn_row);
    lv_obj_set_size(btn_save, 150, 50);
    lv_obj_align(btn_save, LV_ALIGN_LEFT_MID, 20, 0);
    lv_obj_set_style_bg_color(btn_save, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_clear_flag(btn_save, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn_save, event_save, LV_EVENT_CLICKED, NULL);
    lv_obj_t * l_save = lv_label_create(btn_save);
    lv_label_set_text(l_save, "Save");
    lv_obj_center(l_save);

    lv_obj_t * btn_exit = lv_btn_create(btn_row);
    lv_obj_set_size(btn_exit, 150, 50);
    lv_obj_align(btn_exit, LV_ALIGN_RIGHT_MID, -20, 0);
    lv_obj_set_style_bg_color(btn_exit, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_clear_flag(btn_exit, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn_exit, event_back, LV_EVENT_CLICKED, NULL);
    lv_obj_t * l_exit = lv_label_create(btn_exit);
    lv_label_set_text(l_exit, "Exit");
    lv_obj_center(l_exit);

    update_sliders(); // Load initial val

    lv_obj_add_event_cb(scr_fan, event_cleanup, LV_EVENT_DELETE, NULL);
    lv_scr_load(scr_fan);
}
