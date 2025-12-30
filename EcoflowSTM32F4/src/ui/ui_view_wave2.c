#include "ui_view_wave2.h"
#include "ui_icons.h"
#include "uart_task.h"
#include <stdio.h>

static lv_obj_t * scr_wave2;
static lv_style_t style_scr;
static lv_style_t style_panel;
static lv_style_t style_text_large;
static lv_style_t style_text_small;
static lv_style_t style_btn_default;

static lv_obj_t * label_cur_temp;
static lv_obj_t * slider_set_temp;
static lv_obj_t * label_set_temp_val;
static lv_obj_t * dd_mode;
static lv_obj_t * dd_sub_mode;
static lv_obj_t * slider_fan;
static lv_obj_t * label_fan_val;
static lv_obj_t * cont_sub_controls;
static lv_obj_t * label_status;

static void create_styles(void) {
    lv_style_init(&style_scr);
    lv_style_set_bg_color(&style_scr, lv_color_hex(0xFF121212));
    lv_style_set_text_color(&style_scr, lv_color_white());

    lv_style_init(&style_panel);
    lv_style_set_bg_color(&style_panel, lv_color_hex(0xFF282828));
    lv_style_set_radius(&style_panel, 12);
    lv_style_set_border_width(&style_panel, 0);

    lv_style_init(&style_text_large);
    lv_style_set_text_font(&style_text_large, &lv_font_montserrat_32);
    lv_style_set_text_color(&style_text_large, lv_color_white());

    lv_style_init(&style_text_small);
    lv_style_set_text_font(&style_text_small, &lv_font_montserrat_16);
    lv_style_set_text_color(&style_text_small, lv_palette_main(LV_PALETTE_GREY));

    lv_style_init(&style_btn_default);
    lv_style_set_bg_color(&style_btn_default, lv_palette_darken(LV_PALETTE_GREY, 2));
    lv_style_set_text_color(&style_btn_default, lv_color_white());
    lv_style_set_radius(&style_btn_default, 8);
}

static void event_back(lv_event_t * e) {
    UI_LVGL_ShowDashboard();
}

static void send_cmd(uint8_t type, uint8_t val) {
    Wave2SetMsg msg;
    msg.type = type;
    msg.value = val;
    UART_SendWave2Set(&msg);
}

static void event_temp_change(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    int val = (int)lv_slider_get_value(slider);
    lv_label_set_text_fmt(label_set_temp_val, "%d C", val);
    if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
        send_cmd(W2_SET_TEMP, (uint8_t)val);
    }
}

static void update_visibility(void) {
    int mode = lv_dropdown_get_selected(dd_mode); // 0=Cool, 1=Heat, 2=Fan
    int sub = lv_dropdown_get_selected(dd_sub_mode); // 0=Max, 1=Sleep, 2=Eco, 3=Auto

    // Submode visible if Cool(0) or Heat(1)
    bool show_sub = (mode == 0 || mode == 1);

    if (mode == 2) { // Fan Mode
        lv_obj_add_flag(dd_sub_mode, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(slider_fan, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(label_fan_val, LV_OBJ_FLAG_HIDDEN);
    } else { // Cool/Heat
        lv_obj_clear_flag(dd_sub_mode, LV_OBJ_FLAG_HIDDEN);
        if (sub == 3) { // Auto allowing fan
             lv_obj_clear_flag(slider_fan, LV_OBJ_FLAG_HIDDEN);
             lv_obj_clear_flag(label_fan_val, LV_OBJ_FLAG_HIDDEN);
        } else {
             lv_obj_add_flag(slider_fan, LV_OBJ_FLAG_HIDDEN);
             lv_obj_add_flag(label_fan_val, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void event_mode_change(lv_event_t * e) {
    uint16_t opt = lv_dropdown_get_selected(dd_mode);
    send_cmd(W2_SET_MODE, (uint8_t)opt);
    update_visibility();
}

static void event_sub_mode_change(lv_event_t * e) {
    uint16_t opt = lv_dropdown_get_selected(dd_sub_mode);
    send_cmd(W2_SET_SUB_MODE, (uint8_t)opt);
    update_visibility();
}

static void event_fan_change(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    int val = (int)lv_slider_get_value(slider);
    lv_label_set_text_fmt(label_fan_val, "Fan: %d", val);
    if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
        send_cmd(W2_SET_FAN, (uint8_t)val);
    }
}

void ui_view_wave2_init(lv_obj_t * parent) {
    create_styles();
    scr_wave2 = lv_obj_create(NULL);
    lv_obj_add_style(scr_wave2, &style_scr, 0);

    // Header
    lv_obj_t * btn_back = lv_btn_create(scr_wave2);
    lv_obj_set_size(btn_back, 80, 50);
    lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 20, 20);
    lv_obj_add_style(btn_back, &style_btn_default, 0);
    lv_obj_add_event_cb(btn_back, event_back, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_back = lv_label_create(btn_back);
    ui_set_icon(lbl_back, MDI_ICON_BACK);
    lv_obj_center(lbl_back);

    lv_obj_t * title = lv_label_create(scr_wave2);
    lv_label_set_text(title, "Wave 2 Control");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 25);

    // Current Temp Display
    label_cur_temp = lv_label_create(scr_wave2);
    lv_label_set_text(label_cur_temp, "-- C");
    lv_obj_set_style_text_font(label_cur_temp, &lv_font_montserrat_32, 0);
    lv_obj_align(label_cur_temp, LV_ALIGN_TOP_RIGHT, -40, 30);

    // Main Container
    lv_obj_t * cont = lv_obj_create(scr_wave2);
    lv_obj_set_size(cont, 700, 320);
    lv_obj_align(cont, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_color(cont, lv_color_hex(0xFF1E1E1E), 0);
    lv_obj_set_style_border_width(cont, 0, 0);

    // Set Temp Slider
    lv_obj_t * l1 = lv_label_create(cont);
    // Use text labels as font generator is unavailable
    lv_label_set_text(l1, "Set Temp");
    lv_obj_set_style_text_color(l1, lv_color_white(), 0);
    lv_obj_align(l1, LV_ALIGN_TOP_LEFT, 20, 20);

    label_set_temp_val = lv_label_create(cont);
    lv_label_set_text(label_set_temp_val, "25 C");
    lv_obj_set_style_text_font(label_set_temp_val, &lv_font_montserrat_32, 0);
    lv_obj_align(label_set_temp_val, LV_ALIGN_TOP_RIGHT, -20, 10);

    slider_set_temp = lv_slider_create(cont);
    lv_slider_set_range(slider_set_temp, 16, 30);
    lv_slider_set_value(slider_set_temp, 25, LV_ANIM_OFF);
    lv_obj_set_width(slider_set_temp, 600);
    lv_obj_align(slider_set_temp, LV_ALIGN_TOP_MID, 0, 60);
    lv_obj_add_event_cb(slider_set_temp, event_temp_change, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(slider_set_temp, event_temp_change, LV_EVENT_RELEASED, NULL);

    // Mode Selector
    lv_obj_t * l2 = lv_label_create(cont);
    lv_label_set_text(l2, "Mode");
    lv_obj_align(l2, LV_ALIGN_LEFT_MID, 20, 0);

    dd_mode = lv_dropdown_create(cont);
    lv_dropdown_set_options(dd_mode, "Cool\nHeat\nFan");
    lv_obj_set_width(dd_mode, 150);
    lv_obj_align(dd_mode, LV_ALIGN_LEFT_MID, 20, 40);
    lv_obj_add_event_cb(dd_mode, event_mode_change, LV_EVENT_VALUE_CHANGED, NULL);

    // SubMode Selector
    dd_sub_mode = lv_dropdown_create(cont);
    lv_dropdown_set_options(dd_sub_mode, "Max\nSleep\nEco\nAuto"); // Changed Manual to Auto
    lv_obj_set_width(dd_sub_mode, 150);
    lv_obj_align(dd_sub_mode, LV_ALIGN_CENTER, 0, 40);
    lv_obj_add_event_cb(dd_sub_mode, event_sub_mode_change, LV_EVENT_VALUE_CHANGED, NULL);

    // Fan Slider
    label_fan_val = lv_label_create(cont);
    lv_label_set_text(label_fan_val, "Fan: 1");
    lv_obj_align(label_fan_val, LV_ALIGN_RIGHT_MID, -20, 0);

    slider_fan = lv_slider_create(cont);
    lv_slider_set_range(slider_fan, 1, 3);
    lv_slider_set_value(slider_fan, 1, LV_ANIM_OFF);
    lv_obj_set_width(slider_fan, 150);
    lv_obj_align(slider_fan, LV_ALIGN_RIGHT_MID, -20, 40);
    lv_obj_add_event_cb(slider_fan, event_fan_change, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(slider_fan, event_fan_change, LV_EVENT_RELEASED, NULL);

    update_visibility();
}

lv_obj_t * ui_view_wave2_get_screen(void) {
    return scr_wave2;
}

void ui_view_wave2_update(Wave2DataStruct * data) {
    if (!data) return;

    // Update labels
    lv_label_set_text_fmt(label_cur_temp, "%d C", (int)data->envTemp);

    // Update controls if not being interacted with
    if (lv_slider_is_dragged(slider_set_temp) == false) {
        lv_slider_set_value(slider_set_temp, data->setTemp, LV_ANIM_ON);
        lv_label_set_text_fmt(label_set_temp_val, "%d C", data->setTemp);
    }

    if (lv_dropdown_get_selected(dd_mode) != data->mode) {
        lv_dropdown_set_selected(dd_mode, data->mode);
    }

    if (lv_dropdown_get_selected(dd_sub_mode) != data->subMode) {
        lv_dropdown_set_selected(dd_sub_mode, data->subMode);
    }

    if (lv_slider_is_dragged(slider_fan) == false) {
        lv_slider_set_value(slider_fan, data->fanValue, LV_ANIM_ON);
        lv_label_set_text_fmt(label_fan_val, "Fan: %d", data->fanValue);
    }

    update_visibility();
}
