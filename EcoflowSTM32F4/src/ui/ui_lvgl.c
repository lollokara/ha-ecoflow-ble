#include "ui_lvgl.h"
#include "ui_icons.h"
#include "ui_view_wave2.h"
#include "lvgl.h"
#include "uart_task.h" // Added for UART commands
#include <stdio.h>
#include <math.h>
#include "stm32f4xx_hal.h"

extern void SetBacklight(uint8_t percent);
static uint32_t last_touch_time = 0;
static bool is_sleeping = false;

static int safe_float_to_int(float f) {
    if (isnan(f) || isinf(f)) return 0;
    return (int)f;
}

// --- State Variables (Settings) ---
static int lim_input_w = 600;       // 400 - 3000
static int lim_discharge_p = 5;     // 0 - 30 %
static int lim_charge_p = 100;      // 50 - 100 %

// --- Styles ---
static lv_style_t style_scr;
static lv_style_t style_panel;
static lv_style_t style_text_large;
static lv_style_t style_text_small;
static lv_style_t style_icon_label;
static lv_style_t style_btn_red;
static lv_style_t style_btn_default;
static lv_style_t style_btn_green;

// --- Dashboard Widgets ---
static lv_obj_t * scr_dash;
static lv_obj_t * label_temp;
static lv_obj_t * arc_batt;
static lv_obj_t * label_soc;

static lv_obj_t * btn_ac_toggle;
static lv_obj_t * lbl_ac_t;
static lv_obj_t * btn_dc_toggle;
static lv_obj_t * lbl_dc_t;

// --- Settings Widgets ---
static lv_obj_t * scr_settings;
static lv_obj_t * label_lim_in_val;
static lv_obj_t * label_lim_out_val;
static lv_obj_t * label_lim_chg_val;
static lv_obj_t * slider_lim_in;
static lv_obj_t * slider_lim_out;
static lv_obj_t * slider_lim_chg;
static lv_obj_t * label_calib_debug; // For touch calibration

// Indicator
static lv_obj_t * led_status_dot;
static lv_obj_t * label_disconnected;

// Flow Data Labels - Now managed via card structs
static lv_obj_t * label_solar_val;
static lv_obj_t * label_grid_val;
static lv_obj_t * label_car_val;
static lv_obj_t * label_usb_val;
static lv_obj_t * label_12v_val;
static lv_obj_t * label_ac_val;

// Popup
static lv_obj_t * cont_popup;

// --- Helpers ---

static void create_styles(void) {
    lv_style_init(&style_scr);
    lv_style_set_bg_color(&style_scr, lv_color_hex(0xFF121212)); // Dark Theme
    lv_style_set_text_color(&style_scr, lv_color_white());

    lv_style_init(&style_panel);
    lv_style_set_bg_color(&style_panel, lv_color_hex(0xFF282828)); // Dark Grey
    lv_style_set_radius(&style_panel, 12);
    lv_style_set_border_width(&style_panel, 0);

    lv_style_init(&style_text_large);
    lv_style_set_text_font(&style_text_large, &lv_font_montserrat_32);
    lv_style_set_text_color(&style_text_large, lv_color_white());

    lv_style_init(&style_text_small);
    lv_style_set_text_font(&style_text_small, &lv_font_montserrat_16);
    lv_style_set_text_color(&style_text_small, lv_palette_main(LV_PALETTE_GREY));

    lv_style_init(&style_icon_label);
    lv_style_set_text_font(&style_icon_label, &ui_font_mdi);
    lv_style_set_text_color(&style_icon_label, lv_palette_main(LV_PALETTE_TEAL));

    lv_style_init(&style_btn_red);
    lv_style_set_bg_color(&style_btn_red, lv_palette_main(LV_PALETTE_RED));
    lv_style_set_text_color(&style_btn_red, lv_color_white());
    lv_style_set_radius(&style_btn_red, 8);

    lv_style_init(&style_btn_default);
    lv_style_set_bg_color(&style_btn_default, lv_palette_darken(LV_PALETTE_GREY, 2));
    lv_style_set_text_color(&style_btn_default, lv_color_white());
    lv_style_set_radius(&style_btn_default, 8);

    lv_style_init(&style_btn_green);
    lv_style_set_bg_color(&style_btn_green, lv_palette_main(LV_PALETTE_GREEN));
    lv_style_set_text_color(&style_btn_green, lv_color_white());
    lv_style_set_radius(&style_btn_green, 8);
}

// --- Input Interceptor for Sleep ---
static void input_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if (code == LV_EVENT_PRESSED || code == LV_EVENT_CLICKED || code == LV_EVENT_VALUE_CHANGED) {
        last_touch_time = xTaskGetTickCount();
        if (is_sleeping) {
            is_sleeping = false;
            // Wake up backlight immediately handled in Update loop or here
            // We'll let the Update loop handle it to use the brightness value
        }
    }
}

// --- Navigation Callbacks ---
static void event_to_settings(lv_event_t * e) {
    lv_scr_load_anim(scr_settings, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
}

static void event_to_dash(lv_event_t * e) {
    lv_scr_load_anim(scr_dash, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
}

static void event_to_wave2(lv_event_t * e) {
    if (!ui_view_wave2_get_screen()) {
        ui_view_wave2_init(NULL);
    }
    lv_scr_load_anim(ui_view_wave2_get_screen(), LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
}

// Exposed Navigation
void UI_LVGL_ShowDashboard(void) {
    lv_scr_load_anim(scr_dash, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
}
void UI_LVGL_ShowWave2(void) {
    event_to_wave2(NULL);
}


// --- Popup Handlers ---
static void event_power_off_click(lv_event_t * e) {
    lv_obj_clear_flag(cont_popup, LV_OBJ_FLAG_HIDDEN);
}
static void event_popup_hide(lv_event_t * e) {
    lv_obj_add_flag(cont_popup, LV_OBJ_FLAG_HIDDEN);
}

static void event_power_off_confirm(lv_event_t * e) {
    lv_obj_add_flag(cont_popup, LV_OBJ_FLAG_HIDDEN);
    UART_SendPowerOff();

    // Give time for UART transmission
    HAL_Delay(500);

    // Reboot
    NVIC_SystemReset();
}

// --- Toggle Callbacks ---
static void event_toggle_ac(lv_event_t * e) {
    lv_obj_t * btn = lv_event_get_target(e);
    bool state = lv_obj_has_state(btn, LV_STATE_CHECKED);
    UART_SendACSet(state ? 1 : 0);
}

static void event_toggle_dc(lv_event_t * e) {
    lv_obj_t * btn = lv_event_get_target(e);
    bool state = lv_obj_has_state(btn, LV_STATE_CHECKED);
    UART_SendDCSet(state ? 1 : 0);
}

// --- Calibration Debug ---
static void event_calib_touch(lv_event_t * e) {
    lv_indev_t * indev = lv_indev_get_act();
    lv_point_t p;
    lv_indev_get_point(indev, &p);

    // Print to Serial (printf redirected to SWV or UART)
    // Also update a label on screen
    if (label_calib_debug) {
        lv_label_set_text_fmt(label_calib_debug, "X: %d, Y: %d", p.x, p.y);
        printf("CALIB: Click at X=%d, Y=%d\n", p.x, p.y);
    }
}

// --- Slider Callbacks ---
static void event_slider_input(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    int val = (int)lv_slider_get_value(slider);
    val = (val + 50) / 100 * 100;
    if (val != lim_input_w) {
        lv_slider_set_value(slider, val, LV_ANIM_OFF);
        lim_input_w = val;
        lv_label_set_text_fmt(label_lim_in_val, "%d W", lim_input_w);
    }
    if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
        UART_SendSetValue(SET_VAL_AC_LIMIT, lim_input_w);
    }
}
static void event_slider_discharge(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    int val = (int)lv_slider_get_value(slider);
    val = (val + 2) / 5 * 5;
    if (val != lim_discharge_p) {
        lv_slider_set_value(slider, val, LV_ANIM_OFF);
        lim_discharge_p = val;
        lv_label_set_text_fmt(label_lim_out_val, "%d %%", lim_discharge_p);
        lv_obj_invalidate(arc_batt);
    }
    if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
        UART_SendSetValue(SET_VAL_MIN_SOC, lim_discharge_p);
    }
}
static void event_slider_charge(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    int val = (int)lv_slider_get_value(slider);
    val = (val + 2) / 5 * 5;
    if (val != lim_charge_p) {
        lv_slider_set_value(slider, val, LV_ANIM_OFF);
        lim_charge_p = val;
        lv_label_set_text_fmt(label_lim_chg_val, "%d %%", lim_charge_p);
        lv_obj_invalidate(arc_batt);
    }
    if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
        UART_SendSetValue(SET_VAL_MAX_SOC, lim_charge_p);
    }
}

// --- Arc Draw Callback for Limits ---
static void event_arc_draw(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_DRAW_PART_END) {
        lv_obj_draw_part_dsc_t * dsc = lv_event_get_draw_part_dsc(e);
        if(dsc->part == LV_PART_MAIN) {
            lv_obj_t * obj = lv_event_get_target(e);
            lv_draw_ctx_t * draw_ctx = dsc->draw_ctx;
            const lv_area_t * coords = &obj->coords;

            lv_point_t center;
            center.x = coords->x1 + lv_area_get_width(coords) / 2;
            center.y = coords->y1 + lv_area_get_height(coords) / 2;

            lv_coord_t r_out = lv_area_get_width(coords) / 2;
            lv_coord_t r_in = r_out - 15;

            // Red Line
            if (lim_discharge_p >= 0) {
                float angle_deg = 270.0f + (lim_discharge_p * 3.6f);
                float angle_rad = angle_deg * (3.14159f / 180.0f);
                lv_point_t p1, p2;
                p1.x = center.x + (lv_coord_t)((r_in - 5) * cos(angle_rad));
                p1.y = center.y + (lv_coord_t)((r_in - 5) * sin(angle_rad));
                p2.x = center.x + (lv_coord_t)((r_out + 5) * cos(angle_rad));
                p2.y = center.y + (lv_coord_t)((r_out + 5) * sin(angle_rad));
                lv_draw_line_dsc_t line_dsc;
                lv_draw_line_dsc_init(&line_dsc);
                line_dsc.color = lv_palette_main(LV_PALETTE_RED);
                line_dsc.width = 4;
                line_dsc.round_start = 1;
                line_dsc.round_end = 1;
                lv_draw_line(draw_ctx, &line_dsc, &p1, &p2);
            }

            // Blue Line
            if (lim_charge_p < 100) {
                float angle_deg = 270.0f + (lim_charge_p * 3.6f);
                float angle_rad = angle_deg * (3.14159f / 180.0f);
                lv_point_t p1, p2;
                p1.x = center.x + (lv_coord_t)((r_in - 5) * cos(angle_rad));
                p1.y = center.y + (lv_coord_t)((r_in - 5) * sin(angle_rad));
                p2.x = center.x + (lv_coord_t)((r_out + 5) * cos(angle_rad));
                p2.y = center.y + (lv_coord_t)((r_out + 5) * sin(angle_rad));
                lv_draw_line_dsc_t line_dsc;
                lv_draw_line_dsc_init(&line_dsc);
                line_dsc.color = lv_palette_main(LV_PALETTE_BLUE);
                line_dsc.width = 4;
                line_dsc.round_start = 1;
                line_dsc.round_end = 1;
                lv_draw_line(draw_ctx, &line_dsc, &p1, &p2);
            }
        }
    }
}

// --- Helper to create Info Card ---
// Stores object pointers for dynamic styling
typedef struct {
    lv_obj_t * card;
    lv_obj_t * icon;
    lv_obj_t * title;
    lv_obj_t * value;
} InfoCardObj;

// Global Card Objects
static InfoCardObj card_solar, card_grid, card_car;
static InfoCardObj card_usb, card_12v, card_ac;

static void create_info_card(lv_obj_t * parent, const char* icon_code, const char* label_text, int x, int y, InfoCardObj * obj) {
    obj->card = lv_obj_create(parent);
    lv_obj_set_size(obj->card, 160, 90);
    lv_obj_set_pos(obj->card, x, y);
    lv_obj_add_style(obj->card, &style_panel, 0);
    lv_obj_clear_flag(obj->card, LV_OBJ_FLAG_SCROLLABLE);

    obj->icon = lv_label_create(obj->card);
    ui_set_icon(obj->icon, icon_code);
    lv_obj_align(obj->icon, LV_ALIGN_LEFT_MID, -5, 0);
    lv_obj_set_style_text_font(obj->icon, &ui_font_mdi, 0);
    lv_obj_set_style_text_color(obj->icon, lv_palette_main(LV_PALETTE_TEAL), 0);

    obj->title = lv_label_create(obj->card);
    lv_label_set_text(obj->title, label_text);
    lv_obj_add_style(obj->title, &style_text_small, 0);
    lv_obj_align(obj->title, LV_ALIGN_TOP_RIGHT, 0, -5);

    obj->value = lv_label_create(obj->card);
    lv_label_set_text(obj->value, "0 W");
    lv_obj_add_style(obj->value, &style_text_large, 0);
    lv_obj_set_style_text_font(obj->value, &lv_font_montserrat_20, 0);
    lv_obj_align(obj->value, LV_ALIGN_BOTTOM_RIGHT, 0, 5);
}

static void update_card_style(InfoCardObj * obj, int val) {
    if (val > 0) {
        lv_obj_set_style_bg_color(obj->card, lv_color_white(), 0); // Light BG
        lv_obj_set_style_text_color(obj->title, lv_color_black(), 0); // Dark Text
        lv_obj_set_style_text_color(obj->value, lv_color_black(), 0);
        // Invert Icon? Or keep Teal? User said "invert then the icon and text to be still readable"
        // Dark grey or black icon on white looks good.
        lv_obj_set_style_text_color(obj->icon, lv_palette_main(LV_PALETTE_GREY), 0);
    } else {
        // Revert to Dark Theme
        lv_obj_set_style_bg_color(obj->card, lv_color_hex(0xFF282828), 0);
        lv_obj_set_style_text_color(obj->title, lv_palette_main(LV_PALETTE_GREY), 0);
        lv_obj_set_style_text_color(obj->value, lv_color_white(), 0);
        lv_obj_set_style_text_color(obj->icon, lv_palette_main(LV_PALETTE_TEAL), 0);
    }
}

static void create_settings(void) {
    scr_settings = lv_obj_create(NULL);
    lv_obj_add_style(scr_settings, &style_scr, 0);

    // Debug Touch Area for Calibration
    label_calib_debug = lv_label_create(scr_settings);
    lv_label_set_text(label_calib_debug, "Touch Debug: --, --");
    lv_obj_align(label_calib_debug, LV_ALIGN_BOTTOM_MID, 0, -10);
    // Add event to screen to capture clicks on background
    lv_obj_add_event_cb(scr_settings, event_calib_touch, LV_EVENT_CLICKED, NULL);

    // Header
    lv_obj_t * btn_back = lv_btn_create(scr_settings);
    lv_obj_set_size(btn_back, 100, 50);
    lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 20, 20);
    lv_obj_add_style(btn_back, &style_btn_default, 0);
    lv_obj_add_event_cb(btn_back, event_to_dash, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_back = lv_label_create(btn_back);
    ui_set_icon(lbl_back, MDI_ICON_BACK);
    lv_obj_center(lbl_back);

    lv_obj_t * title = lv_label_create(scr_settings);
    lv_label_set_text(title, "Settings");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 25);

    // Container
    lv_obj_t * cont = lv_obj_create(scr_settings);
    lv_obj_set_size(cont, 700, 350);
    lv_obj_align(cont, LV_ALIGN_CENTER, 0, 40);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);

    // 1. AC Input Limit (400 - 3000)
    lv_obj_t * p1 = lv_obj_create(cont);
    lv_obj_set_size(p1, 650, 100); // Increased height to prevent overlap
    lv_obj_set_style_bg_opa(p1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(p1, 0, 0);

    lv_obj_t * l1 = lv_label_create(p1);
    lv_label_set_text(l1, "Max AC Input");
    lv_obj_set_style_text_color(l1, lv_color_white(), 0);
    lv_obj_set_style_text_font(l1, &lv_font_montserrat_32, 0);
    lv_obj_align(l1, LV_ALIGN_TOP_LEFT, 0, -10); // Moved up

    label_lim_in_val = lv_label_create(p1);
    lv_label_set_text_fmt(label_lim_in_val, "%d W", lim_input_w);
    lv_obj_set_style_text_color(label_lim_in_val, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_lim_in_val, &lv_font_montserrat_32, 0);
    lv_obj_align(label_lim_in_val, LV_ALIGN_TOP_RIGHT, 0, -10);

    slider_lim_in = lv_slider_create(p1);
    lv_slider_set_range(slider_lim_in, 400, 3000);
    lv_slider_set_value(slider_lim_in, lim_input_w, LV_ANIM_OFF);
    lv_obj_set_width(slider_lim_in, 600);
    lv_obj_align(slider_lim_in, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_add_event_cb(slider_lim_in, event_slider_input, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(slider_lim_in, event_slider_input, LV_EVENT_RELEASED, NULL); // Catch release to send cmd
    lv_obj_add_event_cb(slider_lim_in, event_calib_touch, LV_EVENT_CLICKED, NULL); // Catch touch

    // 2. Min Discharge (0 - 30)
    lv_obj_t * p2 = lv_obj_create(cont);
    lv_obj_set_size(p2, 650, 100);
    lv_obj_set_style_bg_opa(p2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(p2, 0, 0);

    lv_obj_t * l2 = lv_label_create(p2);
    lv_label_set_text(l2, "Min Discharge Limit (Red)");
    lv_obj_set_style_text_color(l2, lv_color_white(), 0);
    lv_obj_set_style_text_font(l2, &lv_font_montserrat_32, 0);
    lv_obj_align(l2, LV_ALIGN_TOP_LEFT, 0, -10);

    label_lim_out_val = lv_label_create(p2);
    lv_label_set_text_fmt(label_lim_out_val, "%d %%", lim_discharge_p);
    lv_obj_set_style_text_color(label_lim_out_val, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_lim_out_val, &lv_font_montserrat_32, 0);
    lv_obj_align(label_lim_out_val, LV_ALIGN_TOP_RIGHT, 0, -10);

    slider_lim_out = lv_slider_create(p2);
    lv_slider_set_range(slider_lim_out, 0, 30);
    lv_slider_set_value(slider_lim_out, lim_discharge_p, LV_ANIM_OFF);
    lv_obj_set_width(slider_lim_out, 600);
    lv_obj_align(slider_lim_out, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_add_event_cb(slider_lim_out, event_slider_discharge, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(slider_lim_out, event_slider_discharge, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(slider_lim_out, event_calib_touch, LV_EVENT_CLICKED, NULL);

    // 3. Max Charge (50 - 100)
    lv_obj_t * p3 = lv_obj_create(cont);
    lv_obj_set_size(p3, 650, 100);
    lv_obj_set_style_bg_opa(p3, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(p3, 0, 0);

    lv_obj_t * l3 = lv_label_create(p3);
    lv_label_set_text(l3, "Max Charge Limit (Blue)");
    lv_obj_set_style_text_color(l3, lv_color_white(), 0);
    lv_obj_set_style_text_font(l3, &lv_font_montserrat_32, 0);
    lv_obj_align(l3, LV_ALIGN_TOP_LEFT, 0, -10);

    label_lim_chg_val = lv_label_create(p3);
    lv_label_set_text_fmt(label_lim_chg_val, "%d %%", lim_charge_p);
    lv_obj_set_style_text_color(label_lim_chg_val, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_lim_chg_val, &lv_font_montserrat_32, 0);
    lv_obj_align(label_lim_chg_val, LV_ALIGN_TOP_RIGHT, 0, -10);

    slider_lim_chg = lv_slider_create(p3);
    lv_slider_set_range(slider_lim_chg, 50, 100);
    lv_slider_set_value(slider_lim_chg, lim_charge_p, LV_ANIM_OFF);
    lv_obj_set_width(slider_lim_chg, 600);
    lv_obj_align(slider_lim_chg, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_add_event_cb(slider_lim_chg, event_slider_charge, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(slider_lim_chg, event_slider_charge, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(slider_lim_chg, event_calib_touch, LV_EVENT_CLICKED, NULL);
}


static void create_dashboard(void) {
    scr_dash = lv_obj_create(NULL);
    lv_obj_add_style(scr_dash, &style_scr, 0);

    // --- Header ---
    lv_obj_t * title = lv_label_create(scr_dash);
    lv_label_set_text(title, "EcoFlow Controller");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 20, 10);

    label_temp = lv_label_create(scr_dash);
    lv_label_set_text(label_temp, "25 C");
    lv_obj_set_style_text_font(label_temp, &lv_font_montserrat_20, 0);
    lv_obj_align(label_temp, LV_ALIGN_TOP_RIGHT, -20, 10);

    led_status_dot = lv_obj_create(scr_dash);
    lv_obj_set_size(led_status_dot, 15, 15);
    lv_obj_set_style_radius(led_status_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(led_status_dot, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_border_width(led_status_dot, 0, 0);
    lv_obj_align(led_status_dot, LV_ALIGN_TOP_RIGHT, -100, 15); // Left of Temp

    label_disconnected = lv_label_create(scr_dash);
    lv_label_set_text(label_disconnected, "DISCONNECTED");
    lv_obj_set_style_text_color(label_disconnected, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_text_font(label_disconnected, &lv_font_montserrat_32, 0);
    lv_obj_center(label_disconnected);
    lv_obj_add_flag(label_disconnected, LV_OBJ_FLAG_HIDDEN);

    // --- Left Column (Inputs) ---
    create_info_card(scr_dash, MDI_ICON_SOLAR, "Solar", 20, 60, &card_solar);
    label_solar_val = card_solar.value;
    create_info_card(scr_dash, MDI_ICON_PLUG, "Grid", 20, 160, &card_grid);
    label_grid_val = card_grid.value;
    create_info_card(scr_dash, MDI_ICON_CAR, "Car", 20, 260, &card_car);
    label_car_val = card_car.value;

    // --- Right Column (Outputs) ---
    create_info_card(scr_dash, MDI_ICON_USB, "USB", 620, 60, &card_usb);
    label_usb_val = card_usb.value;
    create_info_card(scr_dash, MDI_ICON_AC, "12V DC", 620, 160, &card_12v);
    label_12v_val = card_12v.value;
    create_info_card(scr_dash, MDI_ICON_AC, "AC Out", 620, 260, &card_ac);
    label_ac_val = card_ac.value;

    // --- Center (Battery) ---
    arc_batt = lv_arc_create(scr_dash);
    lv_obj_set_size(arc_batt, 220, 220);
    lv_arc_set_rotation(arc_batt, 270);
    lv_arc_set_bg_angles(arc_batt, 0, 360);
    lv_arc_set_value(arc_batt, 50);
    lv_obj_align(arc_batt, LV_ALIGN_TOP_MID, 0, 80);
    lv_obj_remove_style(arc_batt, NULL, LV_PART_KNOB);
    lv_obj_clear_flag(arc_batt, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_arc_color(arc_batt, lv_palette_main(LV_PALETTE_TEAL), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_batt, 15, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_batt, 15, LV_PART_MAIN);
    lv_obj_add_event_cb(arc_batt, event_arc_draw, LV_EVENT_DRAW_PART_END, NULL);

    lv_obj_t * icon_bat = lv_label_create(scr_dash);
    ui_set_icon(icon_bat, MDI_ICON_BATTERY);
    lv_obj_set_style_text_font(icon_bat, &ui_font_mdi, 0);
    lv_obj_set_style_text_color(icon_bat, lv_palette_main(LV_PALETTE_TEAL), 0);
    lv_obj_align(icon_bat, LV_ALIGN_CENTER, 0, -80);

    label_soc = lv_label_create(scr_dash);
    lv_label_set_text(label_soc, "50%");
    lv_obj_set_style_text_font(label_soc, &lv_font_montserrat_32, 0);
    lv_obj_align(label_soc, LV_ALIGN_CENTER, 0, -30);

    // --- Footer Controls ---
    int btn_h = 70;
    int btn_w = 120;
    int btn_y = -10;

    // Power Off (Bottom Left)
    lv_obj_t * btn_pwr = lv_btn_create(scr_dash);
    lv_obj_set_size(btn_pwr, btn_w, btn_h);
    lv_obj_align(btn_pwr, LV_ALIGN_BOTTOM_LEFT, 20, btn_y);
    lv_obj_add_style(btn_pwr, &style_btn_red, 0);
    lv_obj_add_event_cb(btn_pwr, event_power_off_click, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_pwr = lv_label_create(btn_pwr);
    lv_label_set_text(lbl_pwr, "OFF");
    lv_obj_center(lbl_pwr);

    // Settings (Bottom Right)
    lv_obj_t * btn_settings = lv_btn_create(scr_dash);
    lv_obj_set_size(btn_settings, 80, 60);
    lv_obj_align(btn_settings, LV_ALIGN_BOTTOM_RIGHT, -20, btn_y - 5);
    lv_obj_add_style(btn_settings, &style_btn_default, 0);
    lv_obj_add_event_cb(btn_settings, event_to_settings, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_set = lv_label_create(btn_settings);
    ui_set_icon(lbl_set, MDI_ICON_SETTINGS);
    lv_obj_center(lbl_set);

    // Wave 2 (Left of Settings)
    lv_obj_t * btn_wave2 = lv_btn_create(scr_dash);
    lv_obj_set_size(btn_wave2, 120, 70); // Same Size as 12V/AC
    lv_obj_align(btn_wave2, LV_ALIGN_BOTTOM_MID, 210, btn_y);
    lv_obj_add_style(btn_wave2, &style_btn_default, 0);
    lv_obj_add_event_cb(btn_wave2, event_to_wave2, LV_EVENT_CLICKED, NULL); // Link to Wave 2
    lv_obj_t * lbl_wave = lv_label_create(btn_wave2);
    lv_label_set_text(lbl_wave, "Wave 2");
    lv_obj_center(lbl_wave);

    // Toggles (Center Bottom)
    btn_ac_toggle = lv_btn_create(scr_dash);
    lv_obj_set_size(btn_ac_toggle, btn_w, btn_h);
    lv_obj_align(btn_ac_toggle, LV_ALIGN_BOTTOM_MID, -70, btn_y);
    lv_obj_add_style(btn_ac_toggle, &style_btn_default, 0);
    lv_obj_add_flag(btn_ac_toggle, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_event_cb(btn_ac_toggle, event_toggle_ac, LV_EVENT_CLICKED, NULL);
    lbl_ac_t = lv_label_create(btn_ac_toggle);
    lv_label_set_text(lbl_ac_t, "AC\nOFF");
    lv_obj_set_style_text_align(lbl_ac_t, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl_ac_t);

    btn_dc_toggle = lv_btn_create(scr_dash);
    lv_obj_set_size(btn_dc_toggle, btn_w, btn_h);
    lv_obj_align(btn_dc_toggle, LV_ALIGN_BOTTOM_MID, 70, btn_y);
    lv_obj_add_style(btn_dc_toggle, &style_btn_default, 0);
    lv_obj_add_flag(btn_dc_toggle, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_event_cb(btn_dc_toggle, event_toggle_dc, LV_EVENT_CLICKED, NULL);
    lbl_dc_t = lv_label_create(btn_dc_toggle);
    lv_label_set_text(lbl_dc_t, "12V\nOFF");
    lv_obj_set_style_text_align(lbl_dc_t, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl_dc_t);


    // --- Popup Overlay ---
    cont_popup = lv_obj_create(scr_dash);
    lv_obj_set_size(cont_popup, 800, 480);
    lv_obj_center(cont_popup);
    lv_obj_set_style_bg_color(cont_popup, lv_color_black(), 0);
    lv_obj_set_style_bg_opa(cont_popup, LV_OPA_70, 0);
    lv_obj_add_flag(cont_popup, LV_OBJ_FLAG_HIDDEN);

    lv_obj_t * popup_panel = lv_obj_create(cont_popup);
    lv_obj_set_size(popup_panel, 400, 200);
    lv_obj_center(popup_panel);
    lv_obj_add_style(popup_panel, &style_panel, 0);

    lv_obj_t * lbl_msg = lv_label_create(popup_panel);
    lv_label_set_text(lbl_msg, "Really Power Off?");
    lv_obj_set_style_text_font(lbl_msg, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl_msg, lv_color_white(), 0); // Explicit White Color
    lv_obj_align(lbl_msg, LV_ALIGN_TOP_MID, 0, 30);

    lv_obj_t * btn_yes = lv_btn_create(popup_panel);
    lv_obj_set_size(btn_yes, 100, 50);
    lv_obj_align(btn_yes, LV_ALIGN_BOTTOM_LEFT, 40, -20);
    lv_obj_add_style(btn_yes, &style_btn_red, 0);
    lv_obj_add_event_cb(btn_yes, event_power_off_confirm, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_yes = lv_label_create(btn_yes);
    lv_label_set_text(lbl_yes, "YES");
    lv_obj_center(lbl_yes);

    lv_obj_t * btn_no = lv_btn_create(popup_panel);
    lv_obj_set_size(btn_no, 100, 50);
    lv_obj_align(btn_no, LV_ALIGN_BOTTOM_RIGHT, -40, -20);
    lv_obj_add_style(btn_no, &style_btn_default, 0);
    lv_obj_add_event_cb(btn_no, event_popup_hide, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_no = lv_label_create(btn_no);
    lv_label_set_text(lbl_no, "NO");
    lv_obj_center(lbl_no);
}

void UI_LVGL_Init(void) {
    lv_init();

    extern void lv_port_disp_init(void);
    extern void lv_port_indev_init(void);
    lv_port_disp_init();
    lv_port_indev_init();

    create_styles();
    create_settings();
    create_dashboard();
    ui_view_wave2_init(NULL); // Pre-init Wave 2 to ensure styles

    lv_scr_load(scr_dash);

    // Add global input listener
    lv_obj_add_event_cb(lv_layer_top(), input_event_cb, LV_EVENT_ALL, NULL);
    // Also add to active screen just in case layer_top isn't catching everything?
    // Usually layer_sys or checking indev is better, but this is simple.
    // Actually, let's just use the tick count in the Update loop if we can detect input.
    // A better way is to attach to the indev driver, but since we can't easily modify drivers here:
    // We will assume any meaningful interaction triggers a callback we already have OR we rely on touch polling.
    // But `lv_layer_top` doesn't always catch events if they are consumed by buttons.
    // We'll stick to updating `last_touch_time` in `input_event_cb` and attaching it to screens.
    lv_obj_add_event_cb(scr_dash, input_event_cb, LV_EVENT_ALL, NULL);
    lv_obj_add_event_cb(scr_settings, input_event_cb, LV_EVENT_ALL, NULL);

    last_touch_time = xTaskGetTickCount();
}

void UI_LVGL_Update(DeviceStatus* dev) {
    // Handle Sleep
    uint32_t now = xTaskGetTickCount();
    uint8_t target_brightness = 100;

    if (dev) {
         if (dev->brightness > 0) target_brightness = dev->brightness;
    }

    if ((now - last_touch_time) > (60000 / portTICK_PERIOD_MS)) { // 60s
        if (!is_sleeping) {
            is_sleeping = true;
            SetBacklight(1); // Dim
        }
    } else {
        if (is_sleeping) {
            is_sleeping = false;
            SetBacklight(target_brightness);
        } else {
            // Continuous update of brightness (if auto-brightness changes)
            SetBacklight(target_brightness);
        }
    }

    if (!dev) return;

    if (dev->id == DEV_TYPE_WAVE_2) {
        ui_view_wave2_update(&dev->data.w2);
        return; // Don't update dashboard with Wave 2 data
    }

    // Map data
    int soc = 0;
    int in_solar=0, in_ac=0, in_alt=0;
    int out_usb=0, out_12v=0, out_ac=0;
    float temp = 25.0f;

    if (dev->id == DEV_TYPE_DELTA_PRO_3) {
        soc = safe_float_to_int(dev->data.d3p.batteryLevel);
        in_ac = safe_float_to_int(dev->data.d3p.inputPower); // Corrected to use inputPower instead of acInputPower
        in_solar = safe_float_to_int(dev->data.d3p.solarLvPower + dev->data.d3p.solarHvPower);
        in_alt = safe_float_to_int(dev->data.d3p.dcLvInputPower);
        out_ac = safe_float_to_int(dev->data.d3p.acLvOutputPower + dev->data.d3p.acHvOutputPower);
        out_12v = safe_float_to_int(dev->data.d3p.dc12vOutputPower);
        out_usb = safe_float_to_int(dev->data.d3p.usbaOutputPower + dev->data.d3p.usbcOutputPower);
        temp = (float)dev->data.d3p.cellTemperature;
    } else if (dev->id == DEV_TYPE_DELTA_3) {
        soc = safe_float_to_int(dev->data.d3.batteryLevel);
        in_ac = safe_float_to_int(dev->data.d3.acInputPower);
        in_solar = safe_float_to_int(dev->data.d3.solarInputPower);
        in_alt = safe_float_to_int(dev->data.d3.dcPortInputPower);
        out_ac = safe_float_to_int(dev->data.d3.acOutputPower);
        out_12v = safe_float_to_int(dev->data.d3.dc12vOutputPower);
        out_usb = safe_float_to_int(dev->data.d3.usbaOutputPower + dev->data.d3.usbcOutputPower);
        temp = (float)dev->data.d3.cellTemperature;
    }

    // Static variables to track state
    static int last_soc = -1;
    static int last_temp = -999;
    static int last_solar = -1;
    static int last_grid = -1;
    static int last_car = -1;
    static int last_usb = -1;
    static int last_12v = -1;
    static int last_ac = -1;

    static bool last_ac_on = false;
    static bool last_dc_on = false;
    static bool first_run = true;

    int temp_int = safe_float_to_int(temp);

    // Settings Sync: Update state variables only if settings screen is not active
    if (lv_scr_act() != scr_settings) {
        if (dev->id == DEV_TYPE_DELTA_PRO_3 || dev->id == DEV_TYPE_DELTA_3) {
            int new_ac_lim = 0;
            int new_max_chg = 0;
            int new_min_dsg = 0;

            if (dev->id == DEV_TYPE_DELTA_PRO_3) {
                new_ac_lim = dev->data.d3p.acChargingSpeed;
                new_max_chg = dev->data.d3p.batteryChargeLimitMax;
                new_min_dsg = dev->data.d3p.batteryChargeLimitMin;
            } else {
                new_ac_lim = dev->data.d3.acChargingSpeed;
                new_max_chg = dev->data.d3.batteryChargeLimitMax;
                new_min_dsg = dev->data.d3.batteryChargeLimitMin;
            }

            if (new_ac_lim > 0 && new_ac_lim != lim_input_w) {
                lim_input_w = new_ac_lim;
                if (label_lim_in_val) lv_label_set_text_fmt(label_lim_in_val, "%d W", lim_input_w);
                if (slider_lim_in) lv_slider_set_value(slider_lim_in, lim_input_w, LV_ANIM_OFF);
            }
            if (new_max_chg > 0 && new_max_chg != lim_charge_p) {
                lim_charge_p = new_max_chg;
                if (label_lim_chg_val) lv_label_set_text_fmt(label_lim_chg_val, "%d %%", lim_charge_p);
                if (slider_lim_chg) lv_slider_set_value(slider_lim_chg, lim_charge_p, LV_ANIM_OFF);
                lv_obj_invalidate(arc_batt);
            }
            // Red Line Fix: Ensure it updates and invalidates even if it was 0 or same (to force redraw if hidden)
            // Actually, simply checking != is enough, but let's ensure the red line logic in draw callback works.
            if (new_min_dsg >= 0 && new_min_dsg != lim_discharge_p) {
                lim_discharge_p = new_min_dsg;
                if (label_lim_out_val) lv_label_set_text_fmt(label_lim_out_val, "%d %%", lim_discharge_p);
                if (slider_lim_out) lv_slider_set_value(slider_lim_out, lim_discharge_p, LV_ANIM_OFF);
                lv_obj_invalidate(arc_batt);
            }
        }
    }

    // Update Disconnected State
    if (dev->connected) {
        lv_obj_add_flag(label_disconnected, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(led_status_dot, lv_palette_main(LV_PALETTE_GREEN), 0); // Green dot for data
    } else {
        lv_obj_clear_flag(label_disconnected, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(led_status_dot, lv_palette_main(LV_PALETTE_RED), 0);
    }

    if (first_run || temp_int != last_temp) {
        lv_label_set_text_fmt(label_temp, "%d C", temp_int);
        last_temp = temp_int;
    }

    if (first_run || soc != last_soc) {
        lv_arc_set_value(arc_batt, soc);
        lv_label_set_text_fmt(label_soc, "%d%%", soc);
        last_soc = soc;
    }

    if (first_run || in_solar != last_solar) {
        lv_label_set_text_fmt(label_solar_val, "%d W", in_solar);
        update_card_style(&card_solar, in_solar);
        last_solar = in_solar;
    }
    if (first_run || in_ac != last_grid) {
        lv_label_set_text_fmt(label_grid_val, "%d W", in_ac);
        update_card_style(&card_grid, in_ac);
        last_grid = in_ac;
    }
    if (first_run || in_alt != last_car) {
        lv_label_set_text_fmt(label_car_val, "%d W", in_alt);
        update_card_style(&card_car, in_alt);
        last_car = in_alt;
    }
    if (first_run || out_usb != last_usb) {
        lv_label_set_text_fmt(label_usb_val, "%d W", out_usb);
        update_card_style(&card_usb, out_usb);
        last_usb = out_usb;
    }
    if (first_run || out_12v != last_12v) {
        lv_label_set_text_fmt(label_12v_val, "%d W", out_12v);
        update_card_style(&card_12v, out_12v);
        last_12v = out_12v;
    }
    if (first_run || out_ac != last_ac) {
        lv_label_set_text_fmt(label_ac_val, "%d W", out_ac);
        update_card_style(&card_ac, out_ac);
        last_ac = out_ac;
    }

    // Toggle Styles based on PORT state, not power
    bool ac_on = false;
    bool dc_on = false;

    if (dev->id == DEV_TYPE_DELTA_PRO_3) {
        ac_on = dev->data.d3p.acHvPort; // Or acLvPort, D3P typically uses HV for output
        dc_on = dev->data.d3p.dc12vPort;
    } else if (dev->id == DEV_TYPE_DELTA_3) {
        ac_on = dev->data.d3.acOn;
        dc_on = dev->data.d3.dcOn;
    }

    if (first_run || ac_on != last_ac_on) {
        if (ac_on) {
            lv_obj_add_style(btn_ac_toggle, &style_btn_green, 0);
            lv_obj_remove_style(btn_ac_toggle, &style_btn_default, 0);
            lv_label_set_text(lbl_ac_t, "AC\nON");
            lv_obj_add_state(btn_ac_toggle, LV_STATE_CHECKED);
        } else {
            lv_obj_add_style(btn_ac_toggle, &style_btn_default, 0);
            lv_obj_remove_style(btn_ac_toggle, &style_btn_green, 0);
            lv_label_set_text(lbl_ac_t, "AC\nOFF");
            lv_obj_clear_state(btn_ac_toggle, LV_STATE_CHECKED);
        }
        last_ac_on = ac_on;
    }

    if (first_run || dc_on != last_dc_on) {
        if (dc_on) {
            lv_obj_add_style(btn_dc_toggle, &style_btn_green, 0);
            lv_obj_remove_style(btn_dc_toggle, &style_btn_default, 0);
            lv_label_set_text(lbl_dc_t, "12V\nON");
            lv_obj_add_state(btn_dc_toggle, LV_STATE_CHECKED);
        } else {
            lv_obj_add_style(btn_dc_toggle, &style_btn_default, 0);
            lv_obj_remove_style(btn_dc_toggle, &style_btn_green, 0);
            lv_label_set_text(lbl_dc_t, "12V\nOFF");
            lv_obj_clear_state(btn_dc_toggle, LV_STATE_CHECKED);
        }
        last_dc_on = dc_on;
    }

    first_run = false;

    // Reset LED to Red after a short while?
    // For now, it stays Green if connected.
    // To "Blink" on data, we could toggle it, but simply showing connected status is usually better.
    // User asked for "dot that blinks red and green when new data is received".
    // Implementation: Toggle it here.
    static bool toggle = false;
    if (dev->connected) {
       if (toggle) lv_obj_set_style_bg_color(led_status_dot, lv_palette_main(LV_PALETTE_GREEN), 0);
       else lv_obj_set_style_bg_color(led_status_dot, lv_palette_main(LV_PALETTE_RED), 0);
       toggle = !toggle;
    }
}
