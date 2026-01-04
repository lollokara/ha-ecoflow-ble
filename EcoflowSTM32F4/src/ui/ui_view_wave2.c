#include "ui_view_wave2.h"
#include "ui_icons.h"
#include "uart_task.h"
#include "ui_lvgl.h" // For UI_LVGL_ShowDashboard
#include <stdio.h>

static lv_obj_t * scr_wave2;
static lv_style_t style_scr;
static lv_style_t style_panel;
static lv_style_t style_text_large;
static lv_style_t style_text_small;
static lv_style_t style_btn_default;
static lv_style_t style_btn_selected;

static lv_obj_t * label_cur_temp;
static lv_obj_t * arc_set_temp;
static lv_obj_t * label_set_temp_val;
static lv_obj_t * cont_sub_mode;
static lv_obj_t * dd_sub_mode;
static lv_obj_t * slider_fan;
static lv_obj_t * label_fan_val;
static lv_obj_t * btn_pwr; // Power Button

// Mode Buttons
static lv_obj_t * btn_mode_cool;
static lv_obj_t * btn_mode_heat;
static lv_obj_t * btn_mode_fan;

// State Tracking
static int current_mode = 0; // 0=Cool, 1=Heat, 2=Fan
static uint32_t last_cmd_time = 0; // Timestamp of last user interaction

/**
 * @brief Helper to safely read a float from a packed struct (unaligned safe).
 */
static float get_float_aligned(const float *ptr) {
    float val;
    memcpy(&val, ptr, sizeof(float));
    return val;
}

/**
 * @brief Helper to safely read an int32 from a packed struct (unaligned safe).
 */
static int32_t get_int32_aligned(const int32_t *ptr) {
    int32_t val;
    memcpy(&val, ptr, sizeof(int32_t));
    return val;
}

static void create_styles(void) {
    lv_style_init(&style_scr);
    lv_style_set_bg_color(&style_scr, lv_color_hex(0xFF121212));
    lv_style_set_text_color(&style_scr, lv_color_white());

    lv_style_init(&style_panel);
    lv_style_set_bg_color(&style_panel, lv_color_hex(0xFF282828));
    lv_style_set_text_color(&style_panel, lv_color_white()); // Force White Text
    lv_style_set_radius(&style_panel, 20);
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
    lv_style_set_radius(&style_btn_default, 12);

    lv_style_init(&style_btn_selected);
    lv_style_set_bg_color(&style_btn_selected, lv_palette_main(LV_PALETTE_TEAL));
    lv_style_set_text_color(&style_btn_selected, lv_color_white());
    lv_style_set_radius(&style_btn_selected, 12);
}

static void event_back(lv_event_t * e) {
    UI_LVGL_ShowDashboard();
}

static void send_cmd(uint8_t type, uint8_t val) {
    Wave2SetMsg msg;
    msg.type = type;
    msg.value = val;
    UART_SendWave2Set(&msg);
    last_cmd_time = HAL_GetTick(); // Update timestamp
}

static void event_temp_change(lv_event_t * e) {
    lv_obj_t * arc = lv_event_get_target(e);
    int val = (int)lv_arc_get_value(arc);
    lv_label_set_text_fmt(label_set_temp_val, "%d C", val);
    if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
        send_cmd(W2_PARAM_TEMP, (uint8_t)val);
    }
}

static void update_mode_ui(int mode) {
    current_mode = mode;

    // Reset styles
    lv_obj_remove_style(btn_mode_cool, &style_btn_selected, 0);
    lv_obj_add_style(btn_mode_cool, &style_btn_default, 0);

    lv_obj_remove_style(btn_mode_heat, &style_btn_selected, 0);
    lv_obj_add_style(btn_mode_heat, &style_btn_default, 0);

    lv_obj_remove_style(btn_mode_fan, &style_btn_selected, 0);
    lv_obj_add_style(btn_mode_fan, &style_btn_default, 0);

    // Apply selected style
    if (mode == 0) { // Cool
        lv_obj_add_style(btn_mode_cool, &style_btn_selected, 0);
    } else if (mode == 1) { // Heat
        lv_obj_add_style(btn_mode_heat, &style_btn_selected, 0);
    } else if (mode == 2) { // Fan
        lv_obj_add_style(btn_mode_fan, &style_btn_selected, 0);
    }
}

static void update_visibility(int mode, int sub, bool power_on) {
    if (!power_on) {
        // Gray out controls
        lv_obj_add_state(arc_set_temp, LV_STATE_DISABLED);
        lv_obj_add_state(slider_fan, LV_STATE_DISABLED);
        lv_obj_add_state(dd_sub_mode, LV_STATE_DISABLED);
        lv_obj_add_state(btn_mode_cool, LV_STATE_DISABLED);
        lv_obj_add_state(btn_mode_heat, LV_STATE_DISABLED);
        lv_obj_add_state(btn_mode_fan, LV_STATE_DISABLED);
    } else {
        // Enable controls
        lv_obj_clear_state(arc_set_temp, LV_STATE_DISABLED);
        lv_obj_clear_state(slider_fan, LV_STATE_DISABLED);
        lv_obj_clear_state(dd_sub_mode, LV_STATE_DISABLED);
        lv_obj_clear_state(btn_mode_cool, LV_STATE_DISABLED);
        lv_obj_clear_state(btn_mode_heat, LV_STATE_DISABLED);
        lv_obj_clear_state(btn_mode_fan, LV_STATE_DISABLED);
    }

    if (mode == 2) { // Fan Mode
        lv_obj_add_flag(cont_sub_mode, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(slider_fan, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(label_fan_val, LV_OBJ_FLAG_HIDDEN);
    } else { // Cool/Heat
        lv_obj_clear_flag(cont_sub_mode, LV_OBJ_FLAG_HIDDEN);
        if (sub == 3) { // Auto allowing fan
             lv_obj_clear_flag(slider_fan, LV_OBJ_FLAG_HIDDEN);
             lv_obj_clear_flag(label_fan_val, LV_OBJ_FLAG_HIDDEN);
        } else {
             lv_obj_add_flag(slider_fan, LV_OBJ_FLAG_HIDDEN);
             lv_obj_add_flag(label_fan_val, LV_OBJ_FLAG_HIDDEN);
        }
    }
    update_mode_ui(mode);
}

static void event_mode_click(lv_event_t * e) {
    int mode = (int)(intptr_t)lv_event_get_user_data(e);
    send_cmd(W2_PARAM_MODE, (uint8_t)mode);

    // Optimistic UI update
    int sub = lv_dropdown_get_selected(dd_sub_mode);
    // Assume power is on if we are clicking mode (though it should be disabled)
    // But since we can only click if enabled, power must be true.
    update_visibility(mode, sub, true);
}

static void event_sub_mode_change(lv_event_t * e) {
    uint16_t opt = lv_dropdown_get_selected(dd_sub_mode);
    send_cmd(W2_PARAM_SUB_MODE, (uint8_t)opt);

    update_visibility(current_mode, opt, true);
}

static void event_fan_change(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    int val = (int)lv_slider_get_value(slider);
    lv_label_set_text_fmt(label_fan_val, "Fan: %d", val);
    if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
        send_cmd(W2_PARAM_FAN, (uint8_t)val);
    }
}

static void event_power_toggle(lv_event_t * e) {
    lv_obj_t * btn = lv_event_get_target(e);
    bool state = lv_obj_has_state(btn, LV_STATE_CHECKED);
    send_cmd(W2_PARAM_POWER, state ? 1 : 0);
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
    lv_label_set_text(title, "Wave 2");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 25);

    // Current Temp (Top Right)
    label_cur_temp = lv_label_create(scr_wave2);
    lv_label_set_text(label_cur_temp, "-- C");
    lv_obj_set_style_text_font(label_cur_temp, &lv_font_montserrat_32, 0);
    lv_obj_align(label_cur_temp, LV_ALIGN_TOP_RIGHT, -40, 30);

    // Main Panel
    lv_obj_t * panel = lv_obj_create(scr_wave2);
    lv_obj_set_size(panel, 750, 380);
    lv_obj_align(panel, LV_ALIGN_BOTTOM_MID, 0, -10);
    lv_obj_add_style(panel, &style_panel, 0);
    lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

    // Left Side: Temperature Arc
    arc_set_temp = lv_arc_create(panel);
    lv_obj_set_size(arc_set_temp, 220, 220);
    lv_arc_set_rotation(arc_set_temp, 135);
    lv_arc_set_bg_angles(arc_set_temp, 0, 270);
    lv_arc_set_range(arc_set_temp, 18, 30); // 18C - 30C
    lv_arc_set_value(arc_set_temp, 25);
    lv_obj_align(arc_set_temp, LV_ALIGN_LEFT_MID, 20, 0);
    lv_obj_add_event_cb(arc_set_temp, event_temp_change, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(arc_set_temp, event_temp_change, LV_EVENT_RELEASED, NULL);

    label_set_temp_val = lv_label_create(panel);
    lv_label_set_text(label_set_temp_val, "25 C");
    lv_obj_set_style_text_font(label_set_temp_val, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(label_set_temp_val, lv_color_white(), 0);
    lv_obj_align_to(label_set_temp_val, arc_set_temp, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t * lbl_set_title = lv_label_create(panel);
    lv_label_set_text(lbl_set_title, "Set Temp");
    lv_obj_set_style_text_color(lbl_set_title, lv_color_white(), 0);
    lv_obj_align_to(lbl_set_title, arc_set_temp, LV_ALIGN_OUT_TOP_MID, 0, -10);

    // Right Side: Controls

    // Power Button (Top Right of Panel)
    btn_pwr = lv_btn_create(panel);
    lv_obj_set_size(btn_pwr, 80, 50);
    lv_obj_align(btn_pwr, LV_ALIGN_TOP_RIGHT, -20, 20);
    lv_obj_add_style(btn_pwr, &style_btn_default, 0);
    lv_obj_add_style(btn_pwr, &style_btn_selected, LV_STATE_CHECKED);
    lv_obj_add_flag(btn_pwr, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_event_cb(btn_pwr, event_power_toggle, LV_EVENT_CLICKED, NULL);

    lv_obj_t * lbl_pwr = lv_label_create(btn_pwr);
    ui_set_icon(lbl_pwr, MDI_ICON_POWER);
    lv_obj_center(lbl_pwr);

    // Mode Buttons (Icon Selector)
    int btn_size = 80;
    int spacing = 20;
    int start_x = 300;
    int start_y = 60; // Moved down slightly

    // Cool Button
    btn_mode_cool = lv_btn_create(panel);
    lv_obj_set_size(btn_mode_cool, btn_size, btn_size);
    lv_obj_align(btn_mode_cool, LV_ALIGN_TOP_LEFT, start_x, start_y);
    lv_obj_add_style(btn_mode_cool, &style_btn_default, 0);
    lv_obj_add_event_cb(btn_mode_cool, event_mode_click, LV_EVENT_CLICKED, (void*)(intptr_t)0);
    lv_obj_t * lbl_cool = lv_label_create(btn_mode_cool);
    ui_set_icon(lbl_cool, MDI_ICON_SNOWFLAKE);
    lv_obj_center(lbl_cool);

    // Heat Button
    btn_mode_heat = lv_btn_create(panel);
    lv_obj_set_size(btn_mode_heat, btn_size, btn_size);
    lv_obj_align(btn_mode_heat, LV_ALIGN_TOP_LEFT, start_x + btn_size + spacing, start_y);
    lv_obj_add_style(btn_mode_heat, &style_btn_default, 0);
    lv_obj_add_event_cb(btn_mode_heat, event_mode_click, LV_EVENT_CLICKED, (void*)(intptr_t)1);
    lv_obj_t * lbl_heat = lv_label_create(btn_mode_heat);
    ui_set_icon(lbl_heat, MDI_ICON_FIRE);
    lv_obj_center(lbl_heat);

    // Fan Button
    btn_mode_fan = lv_btn_create(panel);
    lv_obj_set_size(btn_mode_fan, btn_size, btn_size);
    lv_obj_align(btn_mode_fan, LV_ALIGN_TOP_LEFT, start_x + (btn_size + spacing)*2, start_y);
    lv_obj_add_style(btn_mode_fan, &style_btn_default, 0);
    lv_obj_add_event_cb(btn_mode_fan, event_mode_click, LV_EVENT_CLICKED, (void*)(intptr_t)2);
    lv_obj_t * lbl_fan = lv_label_create(btn_mode_fan);
    ui_set_icon(lbl_fan, MDI_ICON_FAN);
    lv_obj_center(lbl_fan);

    // Submode (Dropdown)
    cont_sub_mode = lv_obj_create(panel);
    lv_obj_set_size(cont_sub_mode, 300, 80);
    lv_obj_align(cont_sub_mode, LV_ALIGN_TOP_LEFT, start_x, start_y + btn_size + 30);
    lv_obj_set_style_bg_opa(cont_sub_mode, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont_sub_mode, 0, 0);
    lv_obj_clear_flag(cont_sub_mode, LV_OBJ_FLAG_SCROLLABLE); // Disable Scroll

    lv_obj_t * lbl_sub = lv_label_create(cont_sub_mode);
    lv_label_set_text(lbl_sub, "Mode:");
    lv_obj_set_style_text_color(lbl_sub, lv_color_white(), 0); // White Text
    lv_obj_set_style_text_font(lbl_sub, &lv_font_montserrat_32, 0); // Large Font
    lv_obj_align(lbl_sub, LV_ALIGN_LEFT_MID, 0, 0);

    dd_sub_mode = lv_dropdown_create(cont_sub_mode);
    lv_dropdown_set_options(dd_sub_mode, "Max\nSleep\nEco\nAuto");
    lv_obj_set_width(dd_sub_mode, 150);
    lv_obj_align(dd_sub_mode, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(dd_sub_mode, event_sub_mode_change, LV_EVENT_VALUE_CHANGED, NULL);

    // Fan Slider
    slider_fan = lv_slider_create(panel);
    lv_slider_set_range(slider_fan, 1, 3);
    lv_slider_set_value(slider_fan, 1, LV_ANIM_OFF);
    lv_obj_set_width(slider_fan, 280);
    // Center alignment relative to the 3 mode buttons
    // The buttons start at start_x (300) and span 3*btn_size + 2*spacing = 3*80 + 2*20 = 280px
    // So aligning to start_x will center it under them.
    lv_obj_align(slider_fan, LV_ALIGN_TOP_LEFT, start_x, start_y + btn_size + 30 + 30 + 80);
    lv_obj_add_event_cb(slider_fan, event_fan_change, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(slider_fan, event_fan_change, LV_EVENT_RELEASED, NULL);

    label_fan_val = lv_label_create(panel);
    lv_label_set_text(label_fan_val, "Fan: 1");
    lv_obj_align_to(label_fan_val, slider_fan, LV_ALIGN_OUT_TOP_MID, 0, -10);

    update_visibility(0, 0, false); // Default Cool, Max, Off
}

lv_obj_t * ui_view_wave2_get_screen(void) {
    return scr_wave2;
}

void ui_view_wave2_update(Wave2DataStruct * data) {
    if (!data) return;

    // Suppress updates for 2 seconds after user interaction to prevent UI jumping
    if ((HAL_GetTick() - last_cmd_time) < 2000) {
        return;
    }

    float envTemp = get_float_aligned(&data->envTemp);
    lv_label_set_text_fmt(label_cur_temp, "%d C", (int)envTemp);

    int32_t setTemp = get_int32_aligned(&data->setTemp);
    if (lv_slider_is_dragged(arc_set_temp) == false) {
        lv_arc_set_value(arc_set_temp, setTemp);
        lv_label_set_text_fmt(label_set_temp_val, "%d C", setTemp);
    }

    int32_t subMode = get_int32_aligned(&data->subMode);
    if (lv_dropdown_get_selected(dd_sub_mode) != subMode) {
        lv_dropdown_set_selected(dd_sub_mode, subMode);
    }

    int32_t fanValue = get_int32_aligned(&data->fanValue);
    if (lv_slider_is_dragged(slider_fan) == false) {
        lv_slider_set_value(slider_fan, fanValue, LV_ANIM_ON);
        lv_label_set_text_fmt(label_fan_val, "Fan: %d", (int)fanValue);
    }

    int32_t powerMode = get_int32_aligned(&data->powerMode);
    // Update Power Button State
    if (btn_pwr) {
        if (powerMode != 0) lv_obj_add_state(btn_pwr, LV_STATE_CHECKED);
        else lv_obj_clear_state(btn_pwr, LV_STATE_CHECKED);
    }

    int32_t mode = get_int32_aligned(&data->mode);
    update_visibility(mode, subMode, powerMode != 0);
}
