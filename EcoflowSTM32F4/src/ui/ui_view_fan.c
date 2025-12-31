#include "ui_view_fan.h"
#include "ui_view_debug.h"
#include "ui_core.h"
#include "ui_lvgl.h"
#include "fan_task.h"
#include <stdio.h>
#include "lvgl.h"

static lv_obj_t * scr_fan = NULL;

static lv_obj_t * dd_group;
static lv_obj_t * slider_min_spd;
static lv_obj_t * slider_max_spd;
static lv_obj_t * slider_start_temp;
static lv_obj_t * slider_max_temp;

static lv_obj_t * label_min_spd;
static lv_obj_t * label_max_spd;
static lv_obj_t * label_start_temp;
static lv_obj_t * label_max_temp;

static FanConfig pendingConfig;
static int currentGroup = 0; // 0=Group1, 1=Group2

static void load_config_to_ui(void) {
    FanGroupConfig* cfg = (currentGroup == 0) ? &pendingConfig.group1 : &pendingConfig.group2;

    lv_slider_set_value(slider_min_spd, cfg->min_speed, LV_ANIM_OFF);
    lv_label_set_text_fmt(label_min_spd, "%d RPM", cfg->min_speed);

    lv_slider_set_value(slider_max_spd, cfg->max_speed, LV_ANIM_OFF);
    lv_label_set_text_fmt(label_max_spd, "%d RPM", cfg->max_speed);

    lv_slider_set_value(slider_start_temp, cfg->start_temp, LV_ANIM_OFF);
    lv_label_set_text_fmt(label_start_temp, "%d C", cfg->start_temp);

    lv_slider_set_value(slider_max_temp, cfg->max_temp, LV_ANIM_OFF);
    lv_label_set_text_fmt(label_max_temp, "%d C", cfg->max_temp);
}

static void save_ui_to_config(void) {
    FanGroupConfig* cfg = (currentGroup == 0) ? &pendingConfig.group1 : &pendingConfig.group2;

    cfg->min_speed = (uint16_t)lv_slider_get_value(slider_min_spd);
    cfg->max_speed = (uint16_t)lv_slider_get_value(slider_max_spd);
    cfg->start_temp = (uint8_t)lv_slider_get_value(slider_start_temp);
    cfg->max_temp = (uint8_t)lv_slider_get_value(slider_max_temp);
}

static void event_group_change(lv_event_t * e) {
    save_ui_to_config(); // Save current before switch
    currentGroup = lv_dropdown_get_selected(dd_group);
    load_config_to_ui();
}

static void event_slider_handler(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);

    // Validation Logic
    int min_s = lv_slider_get_value(slider_min_spd);
    int max_s = lv_slider_get_value(slider_max_spd);
    int start_t = lv_slider_get_value(slider_start_temp);
    int max_t = lv_slider_get_value(slider_max_temp);

    if (slider == slider_start_temp) {
        // Start Temp changed. Ensure Max > Start
        if (max_t <= start_t) {
            max_t = start_t + 1;
            if (max_t > 60) max_t = 60; // Clamp
            lv_slider_set_value(slider_max_temp, max_t, LV_ANIM_OFF);
            lv_label_set_text_fmt(label_max_temp, "%d C", max_t);
        }
    }
    else if (slider == slider_max_temp) {
         // Max Temp changed. Ensure Start < Max
         if (start_t >= max_t) {
             start_t = max_t - 1;
             if (start_t < 30) start_t = 30;
             lv_slider_set_value(slider_start_temp, start_t, LV_ANIM_OFF);
             lv_label_set_text_fmt(label_start_temp, "%d C", start_t);
         }
    }

    // Update Labels
    if (slider == slider_min_spd) lv_label_set_text_fmt(label_min_spd, "%d RPM", min_s);
    if (slider == slider_max_spd) lv_label_set_text_fmt(label_max_spd, "%d RPM", max_s);
    if (slider == slider_start_temp) lv_label_set_text_fmt(label_start_temp, "%d C", start_t);
    if (slider == slider_max_temp) lv_label_set_text_fmt(label_max_temp, "%d C", max_t);
}

static void event_save(lv_event_t * e) {
    save_ui_to_config();
    Fan_SetConfig(&pendingConfig);
}

static void event_exit(lv_event_t * e) {
    UI_CreateDebugView();
}

static void event_cleanup(lv_event_t * e) {
    scr_fan = NULL;
}

void UI_CreateFanView(void) {
    if (scr_fan) {
        lv_scr_load(scr_fan);
        return;
    }

    // Initialize config from current state
    FanConfig* current = Fan_GetConfig();
    if (current) {
        pendingConfig = *current;
    } else {
        // Defaults
        pendingConfig.group1.min_speed = 1000; pendingConfig.group1.max_speed = 3000;
        pendingConfig.group1.start_temp = 35; pendingConfig.group1.max_temp = 50;
        pendingConfig.group2 = pendingConfig.group1;
    }

    scr_fan = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_fan, lv_color_hex(0xFF121212), 0);
    lv_obj_set_style_text_color(scr_fan, lv_color_white(), 0);
    lv_obj_add_event_cb(scr_fan, event_cleanup, LV_EVENT_DELETE, NULL);

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

    // Group Selector
    dd_group = lv_dropdown_create(scr_fan);
    lv_dropdown_set_options(dd_group, "Group 1\nGroup 2");
    lv_obj_set_width(dd_group, 200);
    lv_obj_align(dd_group, LV_ALIGN_TOP_MID, 0, 80);
    lv_obj_add_event_cb(dd_group, event_group_change, LV_EVENT_VALUE_CHANGED, NULL);

    // Container for sliders
    lv_obj_t * cont = lv_obj_create(scr_fan);
    lv_obj_set_size(cont, 700, 280);
    lv_obj_align(cont, LV_ALIGN_TOP_MID, 0, 130);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);

    // Slider 1: Min Speed
    lv_obj_t * p1 = lv_obj_create(cont);
    lv_obj_set_size(p1, 650, 60);
    lv_obj_set_style_bg_opa(p1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(p1, 0, 0);
    lv_obj_t * l1 = lv_label_create(p1);
    lv_label_set_text(l1, "Min Speed");
    lv_obj_set_style_text_color(l1, lv_color_white(), 0);
    lv_obj_align(l1, LV_ALIGN_LEFT_MID, 0, 0);
    label_min_spd = lv_label_create(p1);
    lv_obj_align(label_min_spd, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_text_color(label_min_spd, lv_palette_main(LV_PALETTE_TEAL), 0);
    slider_min_spd = lv_slider_create(p1);
    lv_slider_set_range(slider_min_spd, 500, 3000);
    lv_obj_set_width(slider_min_spd, 400);
    lv_obj_align(slider_min_spd, LV_ALIGN_CENTER, 50, 0);
    lv_obj_add_event_cb(slider_min_spd, event_slider_handler, LV_EVENT_VALUE_CHANGED, NULL);

    // Slider 2: Max Speed
    lv_obj_t * p2 = lv_obj_create(cont);
    lv_obj_set_size(p2, 650, 60);
    lv_obj_set_style_bg_opa(p2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(p2, 0, 0);
    lv_obj_t * l2 = lv_label_create(p2);
    lv_label_set_text(l2, "Max Speed");
    lv_obj_set_style_text_color(l2, lv_color_white(), 0);
    lv_obj_align(l2, LV_ALIGN_LEFT_MID, 0, 0);
    label_max_spd = lv_label_create(p2);
    lv_obj_align(label_max_spd, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_text_color(label_max_spd, lv_palette_main(LV_PALETTE_TEAL), 0);
    slider_max_spd = lv_slider_create(p2);
    lv_slider_set_range(slider_max_spd, 3000, 5000);
    lv_obj_set_width(slider_max_spd, 400);
    lv_obj_align(slider_max_spd, LV_ALIGN_CENTER, 50, 0);
    lv_obj_add_event_cb(slider_max_spd, event_slider_handler, LV_EVENT_VALUE_CHANGED, NULL);

    // Slider 3: Start Temp
    lv_obj_t * p3 = lv_obj_create(cont);
    lv_obj_set_size(p3, 650, 60);
    lv_obj_set_style_bg_opa(p3, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(p3, 0, 0);
    lv_obj_t * l3 = lv_label_create(p3);
    lv_label_set_text(l3, "Start Temp");
    lv_obj_set_style_text_color(l3, lv_color_white(), 0);
    lv_obj_align(l3, LV_ALIGN_LEFT_MID, 0, 0);
    label_start_temp = lv_label_create(p3);
    lv_obj_align(label_start_temp, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_text_color(label_start_temp, lv_palette_main(LV_PALETTE_TEAL), 0);
    slider_start_temp = lv_slider_create(p3);
    lv_slider_set_range(slider_start_temp, 30, 45);
    lv_obj_set_width(slider_start_temp, 400);
    lv_obj_align(slider_start_temp, LV_ALIGN_CENTER, 50, 0);
    lv_obj_add_event_cb(slider_start_temp, event_slider_handler, LV_EVENT_VALUE_CHANGED, NULL);

    // Slider 4: Max Temp
    lv_obj_t * p4 = lv_obj_create(cont);
    lv_obj_set_size(p4, 650, 60);
    lv_obj_set_style_bg_opa(p4, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(p4, 0, 0);
    lv_obj_t * l4 = lv_label_create(p4);
    lv_label_set_text(l4, "Max Temp");
    lv_obj_set_style_text_color(l4, lv_color_white(), 0);
    lv_obj_align(l4, LV_ALIGN_LEFT_MID, 0, 0);
    label_max_temp = lv_label_create(p4);
    lv_obj_align(label_max_temp, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_text_color(label_max_temp, lv_palette_main(LV_PALETTE_TEAL), 0);
    slider_max_temp = lv_slider_create(p4);
    lv_slider_set_range(slider_max_temp, 40, 60);
    lv_obj_set_width(slider_max_temp, 400);
    lv_obj_align(slider_max_temp, LV_ALIGN_CENTER, 50, 0);
    lv_obj_add_event_cb(slider_max_temp, event_slider_handler, LV_EVENT_VALUE_CHANGED, NULL);


    // Buttons
    lv_obj_t * btn_save = lv_btn_create(scr_fan);
    lv_obj_set_size(btn_save, 120, 50);
    lv_obj_align(btn_save, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_obj_set_style_bg_color(btn_save, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_clear_flag(btn_save, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn_save, event_save, LV_EVENT_CLICKED, NULL);
    lv_obj_t * l_save = lv_label_create(btn_save);
    lv_label_set_text(l_save, "Save");
    lv_obj_center(l_save);

    lv_obj_t * btn_exit = lv_btn_create(scr_fan);
    lv_obj_set_size(btn_exit, 120, 50);
    lv_obj_align(btn_exit, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_set_style_bg_color(btn_exit, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_clear_flag(btn_exit, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn_exit, event_exit, LV_EVENT_CLICKED, NULL);
    lv_obj_t * l_exit = lv_label_create(btn_exit);
    lv_label_set_text(l_exit, "Exit");
    lv_obj_center(l_exit);

    // Initial Load
    load_config_to_ui();

    lv_scr_load(scr_fan);
}
