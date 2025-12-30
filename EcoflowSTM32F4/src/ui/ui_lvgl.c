#include "ui_lvgl.h"
#include "lvgl.h"
#include <stdio.h>
#include <math.h>

// --- Styles ---
static lv_style_t style_scr;
static lv_style_t style_panel;
static lv_style_t style_bar_grad;
static lv_style_t style_title;
static lv_style_t style_text_large;
static lv_style_t style_text_small;
static lv_style_t style_btn;

// --- Dashboard Widgets ---
static lv_obj_t * scr_dash;
static lv_obj_t * label_title;
static lv_obj_t * label_temp;
static lv_obj_t * bar_batt;
static lv_obj_t * label_soc;
static lv_obj_t * label_in_power;
static lv_obj_t * label_out_power;
static lv_obj_t * label_rem_time;
static lv_obj_t * btn_settings;

// Flow Widgets
static lv_obj_t * flow_canvas; // Or container for lines

// --- Settings Widgets ---
static lv_obj_t * scr_settings;
static lv_obj_t * slider_chg_lim;
static lv_obj_t * label_chg_lim_val;
static lv_obj_t * slider_dsg_lim;
static lv_obj_t * label_dsg_lim_val;
static lv_obj_t * slider_ac_in;
static lv_obj_t * label_ac_in_val;
static lv_obj_t * btn_back;
static lv_obj_t * btn_save;

// --- State ---
static DeviceStatus current_dev = {0};

// --- Helpers ---

static void create_styles(void) {
    lv_style_init(&style_scr);
    lv_style_set_bg_color(&style_scr, lv_color_white());
    lv_style_set_text_color(&style_scr, lv_color_black());

    lv_style_init(&style_panel);
    lv_style_set_bg_color(&style_panel, lv_palette_lighten(LV_PALETTE_GREY, 4));
    lv_style_set_radius(&style_panel, 8);
    lv_style_set_border_width(&style_panel, 1);
    lv_style_set_border_color(&style_panel, lv_palette_lighten(LV_PALETTE_GREY, 2));

    lv_style_init(&style_bar_grad);
    lv_style_set_bg_opa(&style_bar_grad, LV_OPA_COVER);
    lv_style_set_bg_color(&style_bar_grad, lv_palette_main(LV_PALETTE_GREEN));
    lv_style_set_bg_grad_color(&style_bar_grad, lv_palette_main(LV_PALETTE_RED));
    lv_style_set_bg_grad_dir(&style_bar_grad, LV_GRAD_DIR_HOR);

    lv_style_init(&style_title);
    lv_style_set_text_font(&style_title, &lv_font_montserrat_20);

    lv_style_init(&style_text_large);
    lv_style_set_text_font(&style_text_large, &lv_font_montserrat_24);

    lv_style_init(&style_text_small);
    lv_style_set_text_font(&style_text_small, &lv_font_montserrat_14);
    lv_style_set_text_color(&style_text_small, lv_palette_main(LV_PALETTE_GREY));
}

// Event Handlers
static void event_settings_btn(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        lv_scr_load_anim(scr_settings, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
    }
}

static void event_back_btn(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_CLICKED) {
        lv_scr_load_anim(scr_dash, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
    }
}

static void event_slider_cb(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    lv_obj_t * label = (lv_obj_t *)lv_event_get_user_data(e);
    int val = lv_slider_get_value(slider);

    if (label == label_ac_in_val) {
        lv_label_set_text_fmt(label, "%dW", val);
    } else {
        lv_label_set_text_fmt(label, "%d%%", val);
    }
}

static void create_dashboard(void) {
    scr_dash = lv_obj_create(NULL);
    lv_obj_add_style(scr_dash, &style_scr, 0);

    // Header
    lv_obj_t * header = lv_obj_create(scr_dash);
    lv_obj_set_size(header, 800, 50);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_add_style(header, &style_panel, 0);
    // Remove border/bg? No, keep it clean.
    lv_obj_set_style_bg_color(header, lv_color_hex(0xFF202020), 0);

    lv_obj_t * title = lv_label_create(header);
    lv_label_set_text(title, "EcoFlow Controller");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_LEFT_MID, 20, 0);

    label_temp = lv_label_create(header);
    lv_label_set_text(label_temp, "Temp: -- C");
    lv_obj_set_style_text_color(label_temp, lv_color_white(), 0);
    lv_obj_align(label_temp, LV_ALIGN_RIGHT_MID, -20, 0);

    // Battery Bar
    bar_batt = lv_bar_create(scr_dash);
    lv_obj_set_size(bar_batt, 700, 40);
    lv_obj_align(bar_batt, LV_ALIGN_TOP_MID, 0, 70);
    lv_obj_set_style_bg_color(bar_batt, lv_palette_lighten(LV_PALETTE_GREY, 4), LV_PART_MAIN);
    // Gradient Indicator
    lv_obj_set_style_bg_color(bar_batt, lv_palette_main(LV_PALETTE_GREEN), LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_color(bar_batt, lv_palette_main(LV_PALETTE_RED), LV_PART_INDICATOR);
    lv_obj_set_style_bg_grad_dir(bar_batt, LV_GRAD_DIR_HOR, LV_PART_INDICATOR);
    lv_bar_set_value(bar_batt, 50, LV_ANIM_OFF);
    lv_bar_set_range(bar_batt, 0, 100);

    label_soc = lv_label_create(scr_dash);
    lv_label_set_text(label_soc, "50%");
    lv_obj_add_style(label_soc, &style_text_large, 0);
    lv_obj_align_to(label_soc, bar_batt, LV_ALIGN_CENTER, 0, 0);

    // Stats Row
    lv_obj_t * cont_stats = lv_obj_create(scr_dash);
    lv_obj_set_size(cont_stats, 800, 60);
    lv_obj_align(cont_stats, LV_ALIGN_TOP_MID, 0, 120);
    lv_obj_set_style_border_width(cont_stats, 0, 0);
    lv_obj_set_style_bg_opa(cont_stats, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(cont_stats, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont_stats, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    label_in_power = lv_label_create(cont_stats);
    lv_label_set_text(label_in_power, "In: 0W");
    lv_obj_add_style(label_in_power, &style_text_large, 0);

    label_out_power = lv_label_create(cont_stats);
    lv_label_set_text(label_out_power, "Out: 0W");
    lv_obj_add_style(label_out_power, &style_text_large, 0);

    label_rem_time = lv_label_create(cont_stats);
    lv_label_set_text(label_rem_time, "--:-- rem");
    lv_obj_add_style(label_rem_time, &style_text_large, 0);

    // Footer / Settings
    btn_settings = lv_btn_create(scr_dash);
    lv_obj_align(btn_settings, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_obj_t * lbl = lv_label_create(btn_settings);
    lv_label_set_text(lbl, LV_SYMBOL_SETTINGS " Settings");
    lv_obj_add_event_cb(btn_settings, event_settings_btn, LV_EVENT_CLICKED, NULL);

    // Central Diagram (Placeholder for advanced vector lines)
    lv_obj_t * img_bat = lv_label_create(scr_dash);
    lv_label_set_text(img_bat, LV_SYMBOL_BATTERY_FULL);
    lv_obj_set_style_text_font(img_bat, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(img_bat, lv_palette_main(LV_PALETTE_TEAL), 0);
    lv_obj_align(img_bat, LV_ALIGN_CENTER, 0, 50);

    lv_obj_t * lbl_flow_hint = lv_label_create(scr_dash);
    lv_label_set_text(lbl_flow_hint, "Solar   Grid   Car      USB   12V   AC");
    lv_obj_align(lbl_flow_hint, LV_ALIGN_CENTER, 0, 20);
    lv_obj_add_style(lbl_flow_hint, &style_text_small, 0);
}

static void create_settings(void) {
    scr_settings = lv_obj_create(NULL);
    lv_obj_add_style(scr_settings, &style_scr, 0);

    // Header
    lv_obj_t * header = lv_obj_create(scr_settings);
    lv_obj_set_size(header, 800, 50);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0xFF202020), 0);

    lv_obj_t * title = lv_label_create(header);
    lv_label_set_text(title, "Device Settings");
    lv_obj_set_style_text_color(title, lv_color_white(), 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    btn_back = lv_btn_create(header);
    lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_t * lbl = lv_label_create(btn_back);
    lv_label_set_text(lbl, LV_SYMBOL_LEFT " Back");
    lv_obj_add_event_cb(btn_back, event_back_btn, LV_EVENT_CLICKED, NULL);

    // Content Container
    lv_obj_t * cont = lv_obj_create(scr_settings);
    lv_obj_set_size(cont, 700, 350);
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, 30);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);

    // 1. Charge Limit (50-100%)
    lv_obj_t * row1 = lv_obj_create(cont);
    lv_obj_set_size(row1, 600, 80);
    lv_obj_set_style_border_width(row1, 0, 0);

    lv_obj_t * l1 = lv_label_create(row1);
    lv_label_set_text(l1, "Max Charge Limit");
    lv_obj_align(l1, LV_ALIGN_TOP_LEFT, 0, 0);

    slider_chg_lim = lv_slider_create(row1);
    lv_slider_set_range(slider_chg_lim, 50, 100);
    lv_slider_set_value(slider_chg_lim, 100, LV_ANIM_OFF);
    lv_obj_set_width(slider_chg_lim, 400);
    lv_obj_align(slider_chg_lim, LV_ALIGN_BOTTOM_LEFT, 0, -10);

    label_chg_lim_val = lv_label_create(row1);
    lv_label_set_text(label_chg_lim_val, "100%");
    lv_obj_align(label_chg_lim_val, LV_ALIGN_BOTTOM_RIGHT, -20, -10);
    lv_obj_add_event_cb(slider_chg_lim, event_slider_cb, LV_EVENT_VALUE_CHANGED, label_chg_lim_val);


    // 2. Discharge Limit (0-50%)
    lv_obj_t * row2 = lv_obj_create(cont);
    lv_obj_set_size(row2, 600, 80);
    lv_obj_set_style_border_width(row2, 0, 0);

    lv_obj_t * l2 = lv_label_create(row2);
    lv_label_set_text(l2, "Min Discharge Limit");
    lv_obj_align(l2, LV_ALIGN_TOP_LEFT, 0, 0);

    slider_dsg_lim = lv_slider_create(row2);
    lv_slider_set_range(slider_dsg_lim, 0, 50);
    lv_slider_set_value(slider_dsg_lim, 0, LV_ANIM_OFF);
    lv_obj_set_width(slider_dsg_lim, 400);
    lv_obj_align(slider_dsg_lim, LV_ALIGN_BOTTOM_LEFT, 0, -10);

    label_dsg_lim_val = lv_label_create(row2);
    lv_label_set_text(label_dsg_lim_val, "0%");
    lv_obj_align(label_dsg_lim_val, LV_ALIGN_BOTTOM_RIGHT, -20, -10);
    lv_obj_add_event_cb(slider_dsg_lim, event_slider_cb, LV_EVENT_VALUE_CHANGED, label_dsg_lim_val);


    // 3. AC Input (0-3000W)
    lv_obj_t * row3 = lv_obj_create(cont);
    lv_obj_set_size(row3, 600, 80);
    lv_obj_set_style_border_width(row3, 0, 0);

    lv_obj_t * l3 = lv_label_create(row3);
    lv_label_set_text(l3, "AC Input Limit");
    lv_obj_align(l3, LV_ALIGN_TOP_LEFT, 0, 0);

    slider_ac_in = lv_slider_create(row3);
    lv_slider_set_range(slider_ac_in, 0, 3000);
    lv_slider_set_value(slider_ac_in, 2000, LV_ANIM_OFF);
    lv_obj_set_width(slider_ac_in, 400);
    lv_obj_align(slider_ac_in, LV_ALIGN_BOTTOM_LEFT, 0, -10);

    label_ac_in_val = lv_label_create(row3);
    lv_label_set_text(label_ac_in_val, "2000W");
    lv_obj_align(label_ac_in_val, LV_ALIGN_BOTTOM_RIGHT, -20, -10);
    lv_obj_add_event_cb(slider_ac_in, event_slider_cb, LV_EVENT_VALUE_CHANGED, label_ac_in_val);

    // Save Button
    btn_save = lv_btn_create(scr_settings);
    lv_obj_set_size(btn_save, 200, 50);
    lv_obj_align(btn_save, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_add_event_cb(btn_save, event_back_btn, LV_EVENT_CLICKED, NULL); // Go back on save
    lv_obj_t * lbls = lv_label_create(btn_save);
    lv_label_set_text(lbls, "Save");
    lv_obj_center(lbls);
}

void UI_LVGL_Init(void) {
    lv_init();

    // Drivers
    extern void lv_port_disp_init(void);
    extern void lv_port_indev_init(void);
    lv_port_disp_init();
    lv_port_indev_init();

    create_styles();
    create_dashboard();
    create_settings();

    lv_scr_load(scr_dash);
}

void UI_LVGL_Update(DeviceStatus* dev) {
    if (!dev) return;

    // Map data
    int soc = 0;
    int in_w = 0;
    int out_w = 0;
    float temp = 25.0f;

    if (dev->id == DEV_TYPE_DELTA_PRO_3) {
        soc = (int)dev->data.d3p.batteryLevel;
        in_w = (int)dev->data.d3p.inputPower;
        out_w = (int)dev->data.d3p.outputPower;
        temp = dev->data.d3p.cellTemperature;
    } else if (dev->id == DEV_TYPE_DELTA_3) {
        soc = (int)dev->data.d3.batteryLevel;
        in_w = (int)dev->data.d3.inputPower;
        out_w = (int)dev->data.d3.outputPower;
        temp = dev->data.d3.cellTemperature;
    }

    // Update Widgets
    lv_bar_set_value(bar_batt, soc, LV_ANIM_ON);
    lv_label_set_text_fmt(label_soc, "%d%%", soc);
    lv_label_set_text_fmt(label_in_power, "In: %dW", in_w);
    lv_label_set_text_fmt(label_out_power, "Out: %dW", out_w);
    lv_label_set_text_fmt(label_temp, "Temp: %.1f C", temp);

    // Force redraw
    // lv_task_handler handles this.
}
