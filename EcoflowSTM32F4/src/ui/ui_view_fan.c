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

static lv_obj_t * lbl_min_spd_val;
static lv_obj_t * lbl_max_spd_val;
static lv_obj_t * lbl_start_temp_val;
static lv_obj_t * lbl_max_temp_val;

static FanConfig localConfig;

static void event_fan_cleanup(lv_event_t * e) {
    scr_fan = NULL;
}

static void event_back_to_debug(lv_event_t * e) {
    UI_CreateDebugView();
}

static void update_sliders_from_config(int group_idx) {
    FanGroupConfig *grp = (group_idx == 0) ? &localConfig.group1 : &localConfig.group2;

    lv_slider_set_value(slider_min_spd, grp->min_speed, LV_ANIM_OFF);
    lv_label_set_text_fmt(lbl_min_spd_val, "%d RPM", grp->min_speed);

    lv_slider_set_value(slider_max_spd, grp->max_speed, LV_ANIM_OFF);
    lv_label_set_text_fmt(lbl_max_spd_val, "%d RPM", grp->max_speed);

    lv_slider_set_value(slider_start_temp, grp->start_temp, LV_ANIM_OFF);
    lv_label_set_text_fmt(lbl_start_temp_val, "%d C", grp->start_temp);

    lv_slider_set_value(slider_max_temp, grp->max_temp, LV_ANIM_OFF);
    lv_label_set_text_fmt(lbl_max_temp_val, "%d C", grp->max_temp);
}

static void event_group_change(lv_event_t * e) {
    int idx = lv_dropdown_get_selected(dd_group);
    update_sliders_from_config(idx);
}

static void event_slider_change(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    int idx = lv_dropdown_get_selected(dd_group);
    FanGroupConfig *grp = (idx == 0) ? &localConfig.group1 : &localConfig.group2;
    int val = lv_slider_get_value(slider);

    if (slider == slider_min_spd) {
        val = (val / 100) * 100; // Snap to 100
        lv_slider_set_value(slider, val, LV_ANIM_OFF);
        grp->min_speed = val;
        lv_label_set_text_fmt(lbl_min_spd_val, "%d RPM", val);
    } else if (slider == slider_max_spd) {
        val = (val / 100) * 100; // Snap to 100
        lv_slider_set_value(slider, val, LV_ANIM_OFF);
        grp->max_speed = val;
        lv_label_set_text_fmt(lbl_max_spd_val, "%d RPM", val);
    } else if (slider == slider_start_temp) {
        grp->start_temp = val;
        lv_label_set_text_fmt(lbl_start_temp_val, "%d C", val);
        // Constraint: Start < Max
        if (grp->start_temp >= grp->max_temp) {
             grp->max_temp = grp->start_temp + 1;
             lv_slider_set_value(slider_max_temp, grp->max_temp, LV_ANIM_ON);
             lv_label_set_text_fmt(lbl_max_temp_val, "%d C", grp->max_temp);
        }
    } else if (slider == slider_max_temp) {
        grp->max_temp = val;
        lv_label_set_text_fmt(lbl_max_temp_val, "%d C", val);
        // Constraint: Max > Start
        if (grp->max_temp <= grp->start_temp) {
             grp->start_temp = grp->max_temp - 1;
             lv_slider_set_value(slider_start_temp, grp->start_temp, LV_ANIM_ON);
             lv_label_set_text_fmt(lbl_start_temp_val, "%d C", grp->start_temp);
        }
    }
}

static void event_save(lv_event_t * e) {
    Fan_SetConfig(&localConfig);

    // Visual Confirmation
    lv_obj_t * label_saved = lv_label_create(scr_fan);
    lv_label_set_text(label_saved, "Config Saved!");
    lv_obj_set_style_text_font(label_saved, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(label_saved, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_center(label_saved);
    lv_obj_del_delayed(label_saved, 1500);
}

static void create_slider_row(lv_obj_t * parent, const char * label, int min, int max, lv_obj_t ** slider_out, lv_obj_t ** lbl_out) {
    lv_obj_t * cont = lv_obj_create(parent);
    lv_obj_set_size(cont, lv_pct(100), 80);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);

    lv_obj_t * l = lv_label_create(cont);
    lv_label_set_text(l, label);
    lv_obj_set_style_text_color(l, lv_color_white(), 0);
    lv_obj_align(l, LV_ALIGN_TOP_LEFT, 0, 0);

    *lbl_out = lv_label_create(cont);
    lv_label_set_text(*lbl_out, "--");
    lv_obj_set_style_text_color(*lbl_out, lv_palette_main(LV_PALETTE_TEAL), 0);
    lv_obj_align(*lbl_out, LV_ALIGN_TOP_RIGHT, 0, 0);

    *slider_out = lv_slider_create(cont);
    lv_slider_set_range(*slider_out, min, max);
    lv_obj_set_width(*slider_out, lv_pct(100));
    lv_obj_align(*slider_out, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(*slider_out, event_slider_change, LV_EVENT_VALUE_CHANGED, NULL);
}

void UI_CreateFanView(void) {
    if (scr_fan) {
        lv_scr_load(scr_fan);
        return;
    }

    // Load current config
    Fan_RequestConfig();
    Fan_GetConfig(&localConfig);

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

    // Save Button
    lv_obj_t * btn_save = lv_btn_create(header);
    lv_obj_set_size(btn_save, 100, 40);
    lv_obj_align(btn_save, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_bg_color(btn_save, lv_palette_main(LV_PALETTE_GREEN), 0);
    lv_obj_clear_flag(btn_save, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn_save, event_save, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_save = lv_label_create(btn_save);
    lv_label_set_text(lbl_save, "Save");
    lv_obj_center(lbl_save);

    // Exit Button
    lv_obj_t * btn_exit = lv_btn_create(header);
    lv_obj_set_size(btn_exit, 100, 40);
    lv_obj_align(btn_exit, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_bg_color(btn_exit, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_clear_flag(btn_exit, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_event_cb(btn_exit, event_back_to_debug, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_exit = lv_label_create(btn_exit);
    lv_label_set_text(lbl_exit, "Exit");
    lv_obj_center(lbl_exit);

    // Content
    lv_obj_t * content = lv_obj_create(scr_fan);
    lv_obj_set_size(content, lv_pct(100), lv_pct(85));
    lv_obj_align(content, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(content, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(content, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(content, LV_OBJ_FLAG_SCROLLABLE);

    // Group Selector
    dd_group = lv_dropdown_create(content);
    lv_dropdown_set_options(dd_group, "Group 1 (Fans 1-2)\nGroup 2 (Fans 3-4)");
    lv_obj_set_width(dd_group, 300);
    lv_obj_add_event_cb(dd_group, event_group_change, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_clear_flag(dd_group, LV_OBJ_FLAG_SCROLLABLE);

    // Sliders
    create_slider_row(content, "Min Fan Speed", 500, 3000, &slider_min_spd, &lbl_min_spd_val);
    create_slider_row(content, "Max Fan Speed", 3000, 5000, &slider_max_spd, &lbl_max_spd_val);
    create_slider_row(content, "Start Temperature", 30, 45, &slider_start_temp, &lbl_start_temp_val);
    create_slider_row(content, "Max Temperature", 40, 60, &slider_max_temp, &lbl_max_temp_val);

    // Initial Update
    update_sliders_from_config(0);

    lv_obj_add_event_cb(scr_fan, event_fan_cleanup, LV_EVENT_DELETE, NULL);
    lv_scr_load(scr_fan);
}
