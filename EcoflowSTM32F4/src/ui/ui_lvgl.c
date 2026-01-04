/**
 * @file ui_lvgl.c
 * @author Lollokara
 * @brief LVGL UI Implementation for STM32F4.
 *
 * This file contains the main UI logic, including:
 * - Creating screens (Dashboard, Settings, Wave 2 Control).
 * - Handling touch events and navigation.
 * - Updating UI widgets with data from the UART task.
 * - Managing backlight dimming (sleep mode).
 */

#include "ui_lvgl.h"
#include "ui_icons.h"
#include "ui_view_wave2.h"
#include "ui_view_debug.h"
#include "lvgl.h"
#include "uart_task.h" // Added for UART commands
#include "fan_task.h"  // Added for Fan/Amb Temp
#include "ui_utils.h"  // For safe aligned access
#include <stdio.h>
#include <math.h>
#include "stm32f4xx_hal.h"

// External Backlight Control
extern void SetBacklight(uint8_t percent);

static uint32_t last_touch_time = 0;
static uint32_t last_alt_cmd_time = 0; // Timestamp for Alt Charger interaction
static bool is_sleeping = false;
bool is_charging_active = false;

// --- State Variables (Settings) ---
static int lim_input_w = 600;       // 400 - 3000
static int lim_discharge_p = 5;     // 0 - 30 %
static int lim_charge_p = 100;      // 50 - 100 %

// Alternator Charger Settings
static int alt_start_v = 130;       // 13.0V (100-140)
static int alt_rev_curr = 20;       // 20A (5-45)
static int alt_chg_curr = 20;       // 20A (5-45)
static int alt_pow_limit = 500;     // 500W (100-500)

// --- Device Cache ---
static DeviceStatus device_cache[MAX_DEVICES];

DeviceStatus* UI_GetDeviceCache(int index) {
    if (index >= 0 && index < MAX_DEVICES) {
        return &device_cache[index];
    }
    return NULL;
}

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
static lv_obj_t * label_temp; // Now Battery Temp
static lv_obj_t * label_amb_temp; // New Ambient Temp
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

// Alt Charger Settings Widgets
static lv_obj_t * label_alt_start_v;
static lv_obj_t * label_alt_rev_curr;
static lv_obj_t * label_alt_chg_curr;
static lv_obj_t * label_alt_pow;
static lv_obj_t * slider_alt_start_v;
static lv_obj_t * slider_alt_rev_curr;
static lv_obj_t * slider_alt_chg_curr;
static lv_obj_t * slider_alt_pow;

static lv_obj_t * label_calib_debug; // For touch calibration

// Indicator
static lv_obj_t * led_status_dot;
static lv_obj_t * led_rp2040_dot; // New RP2040 Dot
static lv_obj_t * label_disconnected;

// Flow Data Labels - Now managed via card structs
static lv_obj_t * label_solar_val;
static lv_obj_t * label_grid_val;
static lv_obj_t * label_car_val;
static lv_obj_t * label_usb_val;
static lv_obj_t * label_12v_val;
static lv_obj_t * label_ac_val;

// Wave 2 Button
static lv_obj_t * btn_wave2;
static lv_obj_t * lbl_wave_txt;

// Popup
static lv_obj_t * cont_popup;
static lv_obj_t * cont_popup_alt;

// Animation
static float anim_phase = 0.0f;

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
void UI_ResetIdleTimer(void) {
    last_touch_time = xTaskGetTickCount();
    if (is_sleeping) {
        is_sleeping = false;
        // Immediate Wake up
        SetBacklight(100);
    }
}

static void input_event_cb(lv_event_t * e) {
    UI_ResetIdleTimer();
}

// --- Navigation Callbacks ---
static void event_to_settings(lv_event_t * e) {
    lv_scr_load_anim(scr_settings, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
}

static void event_to_dash(lv_event_t * e) {
    lv_scr_load_anim(scr_dash, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
}

static void event_to_debug(lv_event_t * e) {
    UI_CreateDebugView();
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

void UI_LVGL_ShowSettings(bool auto_del_current) {
    lv_scr_load_anim(scr_settings, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, auto_del_current);
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
    HAL_Delay(3000);

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

// --- Alt Charger Slider Callbacks ---
static void event_slider_alt_start_v(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    int val = (int)lv_slider_get_value(slider);
    if (val != alt_start_v) {
        alt_start_v = val;
        lv_label_set_text_fmt(label_alt_start_v, "%d.%d V", alt_start_v / 10, alt_start_v % 10);
    }
    if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
        for(int i=0; i<3; i++) { UART_SendSetValue(SET_VAL_ALT_START_VOLTAGE, alt_start_v); HAL_Delay(10); }
        last_alt_cmd_time = HAL_GetTick();
    }
}
static void event_slider_alt_rev_curr(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    int val = (int)lv_slider_get_value(slider);
    val = (val + 2) / 5 * 5; // Snap to 5
    if (val != alt_rev_curr) {
        lv_slider_set_value(slider, val, LV_ANIM_OFF);
        alt_rev_curr = val;
        lv_label_set_text_fmt(label_alt_rev_curr, "%d A", alt_rev_curr);
    }
    if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
        for(int i=0; i<3; i++) { UART_SendSetValue(SET_VAL_ALT_REV_LIMIT, alt_rev_curr); HAL_Delay(10); }
        last_alt_cmd_time = HAL_GetTick();
    }
}
static void event_slider_alt_chg_curr(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    int val = (int)lv_slider_get_value(slider);
    val = (val + 2) / 5 * 5; // Snap to 5
    if (val != alt_chg_curr) {
        lv_slider_set_value(slider, val, LV_ANIM_OFF);
        alt_chg_curr = val;
        lv_label_set_text_fmt(label_alt_chg_curr, "%d A", alt_chg_curr);
    }
    if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
        for(int i=0; i<3; i++) { UART_SendSetValue(SET_VAL_ALT_CHG_LIMIT, alt_chg_curr); HAL_Delay(10); }
        last_alt_cmd_time = HAL_GetTick();
    }
}
static void event_slider_alt_pow(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    int val = (int)lv_slider_get_value(slider);
    val = (val + 25) / 50 * 50; // Snap to 50
    if (val != alt_pow_limit) {
        lv_slider_set_value(slider, val, LV_ANIM_OFF);
        alt_pow_limit = val;
        lv_label_set_text_fmt(label_alt_pow, "%d W", alt_pow_limit);
    }
    if (lv_event_get_code(e) == LV_EVENT_RELEASED) {
        for(int i=0; i<3; i++) { UART_SendSetValue(SET_VAL_ALT_PROD_LIMIT, alt_pow_limit); HAL_Delay(10); }
        last_alt_cmd_time = HAL_GetTick();
    }
}

// --- Arc Draw Callback for Limits & Animation ---
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

            // Red Line (Min SOC)
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

            // Blue Line (Max SOC)
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

            // Charging Animation (Waves)
            // Draw an arc segment that moves based on anim_phase
            // Phase goes 0.0 to 1.0. Map to Current SOC Angle -> 360 deg.
            int val = lv_arc_get_value(obj);
            // Only draw if charging
            extern bool is_charging_active;
            if (val < 100 && is_charging_active) {
                float start_angle = 270.0f + (val * 3.6f);
                float end_angle = 270.0f + 360.0f; // Wraps around
                float total_span = end_angle - start_angle;

                // Anim phase maps to position in the empty space
                float current_anim_angle = start_angle + (total_span * anim_phase);

                // Draw a small fading arc segment at current_anim_angle
                lv_draw_arc_dsc_t arc_dsc;
                lv_draw_arc_dsc_init(&arc_dsc);
                arc_dsc.color = lv_palette_main(LV_PALETTE_TEAL);
                arc_dsc.width = 4;
                arc_dsc.opa = LV_OPA_50; // Semi-transparent

                // Draw 3 segments for "wave" effect
                for(int i=0; i<3; i++) {
                    float offset = i * 15.0f; // degrees spacing
                    float a = current_anim_angle - offset;
                    if (a > start_angle) {
                         // Simplify: Just draw a small arc
                         lv_draw_arc(draw_ctx, &arc_dsc, &center, r_out - 7, (uint16_t)a, (uint16_t)(a+10));
                    }
                }
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

static void update_card_style_active(InfoCardObj * obj, bool active) {
    if (active) {
        lv_obj_set_style_bg_color(obj->card, lv_color_white(), 0); // Light BG
        lv_obj_set_style_text_color(obj->title, lv_color_black(), 0); // Dark Text
        lv_obj_set_style_text_color(obj->value, lv_color_black(), 0);
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

    // Debug Button
    lv_obj_t * btn_debug = lv_btn_create(scr_settings);
    lv_obj_set_size(btn_debug, 100, 50);
    lv_obj_align(btn_debug, LV_ALIGN_TOP_RIGHT, -20, 20);
    lv_obj_add_style(btn_debug, &style_btn_default, 0);
    lv_obj_add_event_cb(btn_debug, event_to_debug, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_debug = lv_label_create(btn_debug);
    lv_label_set_text(lbl_debug, "Debug");
    lv_obj_center(lbl_debug);

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

    // --- Alternator Charger Section ---
    lv_obj_t * l_alt = lv_label_create(cont);
    lv_label_set_text(l_alt, "Alternator Charger");
    lv_obj_set_style_text_color(l_alt, lv_palette_main(LV_PALETTE_TEAL), 0);
    lv_obj_set_style_text_font(l_alt, &lv_font_montserrat_32, 0);
    lv_obj_set_style_pad_top(l_alt, 20, 0);

    // 4. Start Voltage (10.0 - 14.0 V)
    lv_obj_t * p4 = lv_obj_create(cont);
    lv_obj_set_size(p4, 650, 100);
    lv_obj_set_style_bg_opa(p4, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(p4, 0, 0);

    lv_obj_t * l4 = lv_label_create(p4);
    lv_label_set_text(l4, "Start Voltage");
    lv_obj_set_style_text_color(l4, lv_color_white(), 0);
    lv_obj_set_style_text_font(l4, &lv_font_montserrat_32, 0);
    lv_obj_align(l4, LV_ALIGN_TOP_LEFT, 0, -10);

    label_alt_start_v = lv_label_create(p4);
    lv_label_set_text_fmt(label_alt_start_v, "%d.%d V", alt_start_v / 10, alt_start_v % 10);
    lv_obj_set_style_text_color(label_alt_start_v, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_alt_start_v, &lv_font_montserrat_32, 0);
    lv_obj_align(label_alt_start_v, LV_ALIGN_TOP_RIGHT, 0, -10);

    slider_alt_start_v = lv_slider_create(p4);
    lv_slider_set_range(slider_alt_start_v, 100, 140);
    lv_slider_set_value(slider_alt_start_v, alt_start_v, LV_ANIM_OFF);
    lv_obj_set_width(slider_alt_start_v, 600);
    lv_obj_align(slider_alt_start_v, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_add_event_cb(slider_alt_start_v, event_slider_alt_start_v, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(slider_alt_start_v, event_slider_alt_start_v, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(slider_alt_start_v, event_calib_touch, LV_EVENT_CLICKED, NULL);

    // 5. Reverse Charging Current (5 - 45 A)
    lv_obj_t * p5 = lv_obj_create(cont);
    lv_obj_set_size(p5, 650, 100);
    lv_obj_set_style_bg_opa(p5, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(p5, 0, 0);

    lv_obj_t * l5 = lv_label_create(p5);
    lv_label_set_text(l5, "Rev. Charging Current");
    lv_obj_set_style_text_color(l5, lv_color_white(), 0);
    lv_obj_set_style_text_font(l5, &lv_font_montserrat_32, 0);
    lv_obj_align(l5, LV_ALIGN_TOP_LEFT, 0, -10);

    label_alt_rev_curr = lv_label_create(p5);
    lv_label_set_text_fmt(label_alt_rev_curr, "%d A", alt_rev_curr);
    lv_obj_set_style_text_color(label_alt_rev_curr, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_alt_rev_curr, &lv_font_montserrat_32, 0);
    lv_obj_align(label_alt_rev_curr, LV_ALIGN_TOP_RIGHT, 0, -10);

    slider_alt_rev_curr = lv_slider_create(p5);
    lv_slider_set_range(slider_alt_rev_curr, 5, 45);
    lv_slider_set_value(slider_alt_rev_curr, alt_rev_curr, LV_ANIM_OFF);
    lv_obj_set_width(slider_alt_rev_curr, 600);
    lv_obj_align(slider_alt_rev_curr, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_add_event_cb(slider_alt_rev_curr, event_slider_alt_rev_curr, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(slider_alt_rev_curr, event_slider_alt_rev_curr, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(slider_alt_rev_curr, event_calib_touch, LV_EVENT_CLICKED, NULL);

    // 6. Charging Current (5 - 45 A)
    lv_obj_t * p6 = lv_obj_create(cont);
    lv_obj_set_size(p6, 650, 100);
    lv_obj_set_style_bg_opa(p6, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(p6, 0, 0);

    lv_obj_t * l6 = lv_label_create(p6);
    lv_label_set_text(l6, "Charging Current");
    lv_obj_set_style_text_color(l6, lv_color_white(), 0);
    lv_obj_set_style_text_font(l6, &lv_font_montserrat_32, 0);
    lv_obj_align(l6, LV_ALIGN_TOP_LEFT, 0, -10);

    label_alt_chg_curr = lv_label_create(p6);
    lv_label_set_text_fmt(label_alt_chg_curr, "%d A", alt_chg_curr);
    lv_obj_set_style_text_color(label_alt_chg_curr, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_alt_chg_curr, &lv_font_montserrat_32, 0);
    lv_obj_align(label_alt_chg_curr, LV_ALIGN_TOP_RIGHT, 0, -10);

    slider_alt_chg_curr = lv_slider_create(p6);
    lv_slider_set_range(slider_alt_chg_curr, 5, 45);
    lv_slider_set_value(slider_alt_chg_curr, alt_chg_curr, LV_ANIM_OFF);
    lv_obj_set_width(slider_alt_chg_curr, 600);
    lv_obj_align(slider_alt_chg_curr, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_add_event_cb(slider_alt_chg_curr, event_slider_alt_chg_curr, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(slider_alt_chg_curr, event_slider_alt_chg_curr, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(slider_alt_chg_curr, event_calib_touch, LV_EVENT_CLICKED, NULL);

    // 7. Power Limit (100 - 500 W)
    lv_obj_t * p7 = lv_obj_create(cont);
    lv_obj_set_size(p7, 650, 100);
    lv_obj_set_style_bg_opa(p7, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(p7, 0, 0);

    lv_obj_t * l7 = lv_label_create(p7);
    lv_label_set_text(l7, "Power Limit");
    lv_obj_set_style_text_color(l7, lv_color_white(), 0);
    lv_obj_set_style_text_font(l7, &lv_font_montserrat_32, 0);
    lv_obj_align(l7, LV_ALIGN_TOP_LEFT, 0, -10);

    label_alt_pow = lv_label_create(p7);
    lv_label_set_text_fmt(label_alt_pow, "%d W", alt_pow_limit);
    lv_obj_set_style_text_color(label_alt_pow, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_alt_pow, &lv_font_montserrat_32, 0);
    lv_obj_align(label_alt_pow, LV_ALIGN_TOP_RIGHT, 0, -10);

    slider_alt_pow = lv_slider_create(p7);
    lv_slider_set_range(slider_alt_pow, 100, 500);
    lv_slider_set_value(slider_alt_pow, alt_pow_limit, LV_ANIM_OFF);
    lv_obj_set_width(slider_alt_pow, 600);
    lv_obj_align(slider_alt_pow, LV_ALIGN_BOTTOM_MID, 0, -5);
    lv_obj_add_event_cb(slider_alt_pow, event_slider_alt_pow, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(slider_alt_pow, event_slider_alt_pow, LV_EVENT_RELEASED, NULL);
    lv_obj_add_event_cb(slider_alt_pow, event_calib_touch, LV_EVENT_CLICKED, NULL);
}

// --- Alternator Charger Popup ---
static void event_popup_alt_hide(lv_event_t * e) {
    lv_obj_add_flag(cont_popup_alt, LV_OBJ_FLAG_HIDDEN);
}

static void event_alt_toggle(lv_event_t * e) {
    lv_obj_t * btn = lv_event_get_target(e);
    bool state = lv_obj_has_state(btn, LV_STATE_CHECKED);
    // Send updated SET_VAL_ALT_ENABLE command (Value 9)
    UART_SendSetValue(SET_VAL_ALT_ENABLE, state ? 1 : 0);
}

static void event_alt_mode_click(lv_event_t * e) {
    int mode = (intptr_t)lv_event_get_user_data(e);
    UART_SendSetValue(SET_VAL_ALT_MODE, mode);
}

static void event_consume(lv_event_t * e) {
    // Consume click to prevent propagation
}

// Helper to open popup
static void UI_ShowAltChargerPopup(void) {
    if (!cont_popup_alt) {
        cont_popup_alt = lv_obj_create(scr_dash);
        lv_obj_set_size(cont_popup_alt, 800, 480);
        lv_obj_center(cont_popup_alt);
        lv_obj_set_style_bg_color(cont_popup_alt, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(cont_popup_alt, LV_OPA_70, 0);

        // Add click event to background to close
        lv_obj_add_event_cb(cont_popup_alt, event_popup_alt_hide, LV_EVENT_CLICKED, NULL);

        lv_obj_t * panel = lv_obj_create(cont_popup_alt);
        lv_obj_set_size(panel, 500, 300);
        lv_obj_center(panel);
        lv_obj_add_style(panel, &style_panel, 0);
        lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
        // Stop click propagation so panel clicks don't close popup
        lv_obj_add_flag(panel, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(panel, event_consume, LV_EVENT_CLICKED, NULL);

        // Header Switch
        lv_obj_t * sw_master = lv_switch_create(panel);
        lv_obj_align(sw_master, LV_ALIGN_TOP_RIGHT, -20, 20);
        lv_obj_add_event_cb(sw_master, event_alt_toggle, LV_EVENT_VALUE_CHANGED, NULL);
        lv_obj_add_style(sw_master, &style_btn_green, LV_PART_INDICATOR | LV_STATE_CHECKED);

        lv_obj_t * lbl_title = lv_label_create(panel);
        lv_label_set_text(lbl_title, "Alternator Charger");
        lv_obj_set_style_text_font(lbl_title, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(lbl_title, lv_color_white(), 0);
        lv_obj_align(lbl_title, LV_ALIGN_TOP_LEFT, 20, 25);

        // Buttons Container
        lv_obj_t * cont_btns = lv_obj_create(panel);
        lv_obj_set_size(cont_btns, 460, 180);
        lv_obj_align(cont_btns, LV_ALIGN_BOTTOM_MID, 0, -10);
        lv_obj_set_style_bg_opa(cont_btns, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(cont_btns, 0, 0);
        lv_obj_set_flex_flow(cont_btns, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(cont_btns, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        // Mode 1: Charging (Driving)
        lv_obj_t * btn1 = lv_btn_create(cont_btns);
        lv_obj_set_size(btn1, 100, 100);
        lv_obj_add_style(btn1, &style_btn_default, 0);
        lv_obj_add_style(btn1, &style_btn_green, LV_STATE_CHECKED); // Use Green style for checked/active
        lv_obj_add_event_cb(btn1, event_alt_mode_click, LV_EVENT_CLICKED, (void*)1); // Mode 1

        lv_obj_t * ico1 = lv_label_create(btn1);
        ui_set_icon(ico1, MDI_ICON_CHARGING_WIRELESS); // F0084
        lv_obj_align(ico1, LV_ALIGN_CENTER, 0, -20);

        lv_obj_t * txt1 = lv_label_create(btn1);
        lv_label_set_text(txt1, "Ric.\nEcoFlow");
        lv_obj_set_style_text_align(txt1, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(txt1, LV_ALIGN_CENTER, 0, 30);

        // Mode 3: Reverse (Parking? Or specialized reverse mode)
        // Memory said: 0=Idle, 1=Driving, 2=Maintenance, 3=Parking/Reverse
        // User text: Ric. Van (Reverse Charging) -> Mode 3
        lv_obj_t * btn2 = lv_btn_create(cont_btns);
        lv_obj_set_size(btn2, 100, 100);
        lv_obj_add_style(btn2, &style_btn_default, 0);
        lv_obj_add_style(btn2, &style_btn_green, LV_STATE_CHECKED);
        lv_obj_add_event_cb(btn2, event_alt_mode_click, LV_EVENT_CLICKED, (void*)3); // Mode 3

        lv_obj_t * ico2 = lv_label_create(btn2);
        ui_set_icon(ico2, MDI_ICON_VAN_UTILITY); // F05F1
        lv_obj_align(ico2, LV_ALIGN_CENTER, 0, -20);

        lv_obj_t * txt2 = lv_label_create(btn2);
        lv_label_set_text(txt2, "Ric.\nVan");
        lv_obj_set_style_text_align(txt2, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(txt2, LV_ALIGN_CENTER, 0, 30);

        // Mode 2: Maintenance
        lv_obj_t * btn3 = lv_btn_create(cont_btns);
        lv_obj_set_size(btn3, 100, 100);
        lv_obj_add_style(btn3, &style_btn_default, 0);
        lv_obj_add_style(btn3, &style_btn_green, LV_STATE_CHECKED);
        lv_obj_add_event_cb(btn3, event_alt_mode_click, LV_EVENT_CLICKED, (void*)2); // Mode 2

        lv_obj_t * ico3 = lv_label_create(btn3);
        ui_set_icon(ico3, MDI_ICON_CAR); // F010C
        lv_obj_align(ico3, LV_ALIGN_CENTER, 0, -20);

        lv_obj_t * txt3 = lv_label_create(btn3);
        lv_label_set_text(txt3, "Mant.\nVan");
        lv_obj_set_style_text_align(txt3, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_align(txt3, LV_ALIGN_CENTER, 0, 30);
    }

    // Refresh State
    DeviceStatus* dev = UI_GetDeviceCache(DEV_TYPE_ALT_CHARGER - 1);

    if (dev) {
        lv_obj_t * panel = lv_obj_get_child(cont_popup_alt, 0);
        lv_obj_t * sw = lv_obj_get_child(panel, 0); // 0th child is switch? Check creation order.
        // Order: Switch, Title, Cont
        // Safe way: get by type or index. Switch is created first.
        if(lv_obj_check_type(sw, &lv_switch_class)) {
            if (dev->data.ac.chargerOpen) lv_obj_add_state(sw, LV_STATE_CHECKED);
            else lv_obj_clear_state(sw, LV_STATE_CHECKED);
        }

        lv_obj_t * cont_btns = lv_obj_get_child(panel, 2);
        int mode = dev->data.ac.chargerMode;

        // Buttons are 0, 1, 2 children of cont_btns
        for(int i=0; i<3; i++) {
            lv_obj_t * btn = lv_obj_get_child(cont_btns, i);
            int btn_mode = 0;
            if(i==0) btn_mode = 1;
            if(i==1) btn_mode = 3;
            if(i==2) btn_mode = 2;

            if(mode == btn_mode) lv_obj_add_state(btn, LV_STATE_CHECKED);
            else lv_obj_clear_state(btn, LV_STATE_CHECKED);
        }
    }

    lv_obj_clear_flag(cont_popup_alt, LV_OBJ_FLAG_HIDDEN);
}

static void event_car_tile_click(lv_event_t * e) {
    UI_ShowAltChargerPopup();
}

static void create_dashboard(void) {
    scr_dash = lv_obj_create(NULL);
    lv_obj_add_style(scr_dash, &style_scr, 0);

    // --- Header ---
    lv_obj_t * title = lv_label_create(scr_dash);
    lv_label_set_text(title, "EcoFlow Controller");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_20, 0);
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 20, 10);

    // Battery Temp
    label_temp = lv_label_create(scr_dash);
    lv_label_set_text(label_temp, "Batt: -- C");
    lv_obj_set_style_text_font(label_temp, &lv_font_montserrat_20, 0);
    lv_obj_align(label_temp, LV_ALIGN_TOP_RIGHT, -20, 10);

    led_status_dot = lv_obj_create(scr_dash);
    lv_obj_set_size(led_status_dot, 15, 15);
    lv_obj_set_style_radius(led_status_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(led_status_dot, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_border_width(led_status_dot, 0, 0);
    lv_obj_align(led_status_dot, LV_ALIGN_TOP_RIGHT, -140, 15);

    // Ambient Temp (RP2040)
    label_amb_temp = lv_label_create(scr_dash);
    lv_label_set_text(label_amb_temp, "Amb: -- C");
    lv_obj_set_style_text_font(label_amb_temp, &lv_font_montserrat_20, 0);
    lv_obj_align(label_amb_temp, LV_ALIGN_TOP_RIGHT, -170, 10);

    led_rp2040_dot = lv_obj_create(scr_dash);
    lv_obj_set_size(led_rp2040_dot, 15, 15);
    lv_obj_set_style_radius(led_rp2040_dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(led_rp2040_dot, lv_palette_main(LV_PALETTE_YELLOW), 0); // Default Yellow (Disconnected)
    lv_obj_set_style_border_width(led_rp2040_dot, 0, 0);
    lv_obj_align(led_rp2040_dot, LV_ALIGN_TOP_RIGHT, -290, 15);

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
    lv_obj_add_flag(card_car.card, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(card_car.card, event_car_tile_click, LV_EVENT_CLICKED, NULL);

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
    btn_wave2 = lv_btn_create(scr_dash);
    lv_obj_set_size(btn_wave2, 120, 70); // Same Size as 12V/AC
    lv_obj_align(btn_wave2, LV_ALIGN_BOTTOM_MID, 210, btn_y);
    lv_obj_add_style(btn_wave2, &style_btn_default, 0);
    lv_obj_add_style(btn_wave2, &style_btn_green, LV_STATE_CHECKED);
    lv_obj_add_flag(btn_wave2, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_add_event_cb(btn_wave2, event_to_wave2, LV_EVENT_CLICKED, NULL); // Link to Wave 2
    lbl_wave_txt = lv_label_create(btn_wave2);
    lv_label_set_text(lbl_wave_txt, "Wave 2");
    lv_obj_set_style_text_align(lbl_wave_txt, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl_wave_txt);

    // Toggles (Center Bottom)
    btn_ac_toggle = lv_btn_create(scr_dash);
    lv_obj_set_size(btn_ac_toggle, btn_w, btn_h);
    lv_obj_align(btn_ac_toggle, LV_ALIGN_BOTTOM_MID, -70, btn_y);
    lv_obj_add_style(btn_ac_toggle, &style_btn_default, 0);
    lv_obj_add_style(btn_ac_toggle, &style_btn_green, LV_STATE_CHECKED);
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
    lv_obj_add_style(btn_dc_toggle, &style_btn_green, LV_STATE_CHECKED);
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


/**
 * @brief Initializes LVGL and creates the UI.
 */
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
    // Note: LV_EVENT_ALL is broad. We specifically want press/scroll.
    // Adding to layer_sys or layer_top captures all?
    // Often better to use an indev feedback callback in `lv_port_indev.c` but here we do it in UI layer.
    lv_obj_add_event_cb(lv_layer_top(), input_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(lv_layer_top(), input_event_cb, LV_EVENT_SCROLL, NULL);

    // Also add to active screens
    lv_obj_add_event_cb(scr_dash, input_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(scr_dash, input_event_cb, LV_EVENT_SCROLL, NULL);

    lv_obj_add_event_cb(scr_settings, input_event_cb, LV_EVENT_PRESSED, NULL);
    lv_obj_add_event_cb(scr_settings, input_event_cb, LV_EVENT_SCROLL, NULL);

    last_touch_time = xTaskGetTickCount();
}

/**
 * @brief Updates the UI with fresh data.
 *
 * Called from the Display Task whenever a new packet is received or
 * periodically for animations.
 *
 * @param dev Pointer to the device status data (can be NULL if no update).
 */
void UI_LVGL_Update(DeviceStatus* dev) {
    // Handle Sleep Logic (Dimming)
    uint32_t now = xTaskGetTickCount();
    uint8_t target_brightness = 100;

    // Update Animation Phase (0.0 to 1.0) for wave effect
    anim_phase += 0.05f;
    if (anim_phase > 1.0f) anim_phase = 0.0f;
    if (arc_batt) lv_obj_invalidate(arc_batt); // Trigger redraw for animation

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

    // Update RP2040 Fan/Temp Status (Independent of EcoFlow Device)
    FanStatus fanStatus;
    Fan_GetStatus(&fanStatus);
    if (fanStatus.connected) {
        lv_obj_set_style_bg_color(led_rp2040_dot, lv_palette_main(LV_PALETTE_BLUE), 0);
        lv_label_set_text_fmt(label_amb_temp, "Amb: %d C", (int)fanStatus.amb_temp);
    } else {
        lv_obj_set_style_bg_color(led_rp2040_dot, lv_palette_main(LV_PALETTE_YELLOW), 0);
        lv_label_set_text(label_amb_temp, "Amb: -- C");
    }

    // Blink RP2040 Dot if connected
    static bool blink_toggle = false;
    blink_toggle = !blink_toggle;

    if (fanStatus.connected) {
         if (blink_toggle) lv_obj_set_style_bg_color(led_rp2040_dot, lv_palette_main(LV_PALETTE_BLUE), 0);
         else lv_obj_set_style_bg_color(led_rp2040_dot, lv_palette_main(LV_PALETTE_GREEN), 0);
    }

    if (!dev) return;

    // Cache the device status
    if (dev->id > 0 && dev->id <= MAX_DEVICES) {
        memcpy(&device_cache[dev->id - 1], dev, sizeof(DeviceStatus));
    }

    // Process Wave 2 specific updates
    if (dev->id == DEV_TYPE_WAVE_2) {
        ui_view_wave2_update(&dev->data.w2);

        // Update Dashboard Button for Wave 2
        if (btn_wave2) {
            int32_t mode = get_int32_aligned(&dev->data.w2.powerMode);
            int32_t watts = get_int32_aligned(&dev->data.w2.batPwrWatt); // Using Battery Power as proxy for now?
            // Actually usually input power is what matters. Wave 2 has psdrPwrWatt?
            // User requested: "power draw reported from the wave 2"

            if (mode != 0) {
                 lv_obj_add_state(btn_wave2, LV_STATE_CHECKED);
                 lv_label_set_text_fmt(lbl_wave_txt, "Wave 2\n%d W", (int)watts);
            } else {
                 lv_obj_clear_state(btn_wave2, LV_STATE_CHECKED);
                 lv_label_set_text(lbl_wave_txt, "Wave 2");
            }
        }

        return; // Don't update dashboard main stats with Wave 2 data
    }

    // Check if Alternator Charger is present and updating
    if (dev->id == DEV_TYPE_ALT_CHARGER) {
         // We might want to trigger a dashboard update if Alt Charger data changed significantly?
         // But main loop below handles D3P updates.
         // We need to ensure we mix data if needed.
    }

    // Map data
    int soc = 0;
    int in_solar=0, in_ac=0, in_alt=0;
    int out_usb=0, out_12v=0, out_ac=0;
    float temp = 25.0f;
    bool is_main_device = false;
    bool ac_plugged_in = false;

    if (dev->id == DEV_TYPE_DELTA_PRO_3) {
        is_main_device = true;
        soc = safe_float_to_int(get_float_aligned(&dev->data.d3p.batteryLevel));
        // Use AC Input specifically for Grid
        in_ac = safe_float_to_int(get_float_aligned(&dev->data.d3p.acInputPower));
        // Sum Solar Inputs
        in_solar = safe_float_to_int(get_float_aligned(&dev->data.d3p.solarLvPower) + get_float_aligned(&dev->data.d3p.solarHvPower));

        // Use Expansion 1 + Expansion 2 for Car Tile on D3P
        in_alt = safe_float_to_int(get_float_aligned(&dev->data.d3p.expansion1Power) + get_float_aligned(&dev->data.d3p.expansion2Power));

        out_ac = safe_float_to_int(get_float_aligned(&dev->data.d3p.acLvOutputPower) + get_float_aligned(&dev->data.d3p.acHvOutputPower));
        out_12v = safe_float_to_int(get_float_aligned(&dev->data.d3p.dc12vOutputPower));
        out_usb = safe_float_to_int(get_float_aligned(&dev->data.d3p.usbaOutputPower) + get_float_aligned(&dev->data.d3p.usbcOutputPower));
        temp = (float)get_int32_aligned(&dev->data.d3p.cellTemperature);

        if (get_int32_aligned(&dev->data.d3p.acInputStatus) == 2) {
             ac_plugged_in = true;
        }
    } else if (dev->id == DEV_TYPE_DELTA_3) {
        is_main_device = true;
        soc = safe_float_to_int(get_float_aligned(&dev->data.d3.batteryLevel));
        in_ac = safe_float_to_int(get_float_aligned(&dev->data.d3.acInputPower));
        in_solar = safe_float_to_int(get_float_aligned(&dev->data.d3.solarInputPower));
        in_alt = safe_float_to_int(get_float_aligned(&dev->data.d3.dcPortInputPower));
        out_ac = safe_float_to_int(get_float_aligned(&dev->data.d3.acOutputPower));
        out_12v = safe_float_to_int(get_float_aligned(&dev->data.d3.dc12vOutputPower));
        out_usb = safe_float_to_int(get_float_aligned(&dev->data.d3.usbaOutputPower) + get_float_aligned(&dev->data.d3.usbcOutputPower));
        temp = (float)get_int32_aligned(&dev->data.d3.cellTemperature);
    }

    // Static variables to track state and minimize redraws
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

    // Only update Dashboard logic if it is a main device
    if (is_main_device) {
        // Simple filter: Ignore 0 if we already had a valid value
        if (temp_int == 0 && last_temp != -999 && last_temp != 0) {
             // Ignore glitch
        } else {
            if (first_run || temp_int != last_temp) {
                lv_label_set_text_fmt(label_temp, "Batt: %d C", temp_int);
                last_temp = temp_int;
            }
        }

        if (soc == 0 && last_soc > 0) {
             // Ignore glitch (jumping to 0%)
        } else {
            if (first_run || soc != last_soc) {
                lv_arc_set_value(arc_batt, soc);
                lv_label_set_text_fmt(label_soc, "%d%%", soc);
                last_soc = soc;
            }
        }

        if (first_run || in_solar != last_solar) {
            lv_label_set_text_fmt(label_solar_val, "%d W", in_solar);
            update_card_style(&card_solar, in_solar);
            last_solar = in_solar;
        }
        if (first_run || in_ac != last_grid) {
            lv_label_set_text_fmt(label_grid_val, "%d W", in_ac);
            if (dev->id == DEV_TYPE_DELTA_PRO_3) {
                update_card_style_active(&card_grid, ac_plugged_in);
            } else {
                update_card_style(&card_grid, in_ac);
            }
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
                lv_label_set_text(lbl_ac_t, "AC\nON");
                lv_obj_add_state(btn_ac_toggle, LV_STATE_CHECKED);
            } else {
                lv_label_set_text(lbl_ac_t, "AC\nOFF");
                lv_obj_clear_state(btn_ac_toggle, LV_STATE_CHECKED);
            }
            last_ac_on = ac_on;
        }

        if (first_run || dc_on != last_dc_on) {
            if (dc_on) {
                lv_label_set_text(lbl_dc_t, "12V\nON");
                lv_obj_add_state(btn_dc_toggle, LV_STATE_CHECKED);
            } else {
                lv_label_set_text(lbl_dc_t, "12V\nOFF");
                lv_obj_clear_state(btn_dc_toggle, LV_STATE_CHECKED);
            }
            last_dc_on = dc_on;
        }

        first_run = false;
    }

    // Update global charging flag
    if (is_main_device) {
        is_charging_active = (in_ac > 0 || in_solar > 0 || in_alt > 0);
    }

    // Settings Sync: Update state variables and UI if not being dragged
    if (is_main_device) {
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
            if (slider_lim_in && !lv_slider_is_dragged(slider_lim_in)) {
                lv_slider_set_value(slider_lim_in, lim_input_w, LV_ANIM_OFF);
            }
        }
        if (new_max_chg > 0 && new_max_chg != lim_charge_p) {
            lim_charge_p = new_max_chg;
            if (label_lim_chg_val) lv_label_set_text_fmt(label_lim_chg_val, "%d %%", lim_charge_p);
            if (slider_lim_chg && !lv_slider_is_dragged(slider_lim_chg)) {
                lv_slider_set_value(slider_lim_chg, lim_charge_p, LV_ANIM_OFF);
                lv_obj_invalidate(arc_batt);
            }
        }
        if (new_min_dsg >= 0 && new_min_dsg != lim_discharge_p) {
            lim_discharge_p = new_min_dsg;
            if (label_lim_out_val) lv_label_set_text_fmt(label_lim_out_val, "%d %%", lim_discharge_p);
            if (slider_lim_out && !lv_slider_is_dragged(slider_lim_out)) {
                lv_slider_set_value(slider_lim_out, lim_discharge_p, LV_ANIM_OFF);
                lv_obj_invalidate(arc_batt);
            }
        }
    } else if (dev->id == DEV_TYPE_ALT_CHARGER) {
        // Suppress updates for 4 seconds after user interaction
        bool ignore_updates = (HAL_GetTick() - last_alt_cmd_time) < 4000;

        // Update Alt Charger Settings
        if (!ignore_updates && dev->data.ac.startVoltage > 0) {
            int val = (int)(dev->data.ac.startVoltage * 10); // Float to Decivolt
            if (val != alt_start_v) {
                alt_start_v = val;
                if (label_alt_start_v) lv_label_set_text_fmt(label_alt_start_v, "%d.%d V", alt_start_v/10, alt_start_v%10);
                if (slider_alt_start_v && !lv_slider_is_dragged(slider_alt_start_v)) {
                    lv_slider_set_value(slider_alt_start_v, alt_start_v, LV_ANIM_OFF);
                }
            }
        }
        if (!ignore_updates && dev->data.ac.reverseChargingCurrentLimit > 0) {
            int val = (int)dev->data.ac.reverseChargingCurrentLimit;
            if (val != alt_rev_curr) {
                alt_rev_curr = val;
                if (label_alt_rev_curr) lv_label_set_text_fmt(label_alt_rev_curr, "%d A", alt_rev_curr);
                if (slider_alt_rev_curr && !lv_slider_is_dragged(slider_alt_rev_curr)) {
                    lv_slider_set_value(slider_alt_rev_curr, alt_rev_curr, LV_ANIM_OFF);
                }
            }
        }
        if (!ignore_updates && dev->data.ac.chargingCurrentLimit > 0) {
            int val = (int)dev->data.ac.chargingCurrentLimit;
            if (val != alt_chg_curr) {
                alt_chg_curr = val;
                if (label_alt_chg_curr) lv_label_set_text_fmt(label_alt_chg_curr, "%d A", alt_chg_curr);
                if (slider_alt_chg_curr && !lv_slider_is_dragged(slider_alt_chg_curr)) {
                    lv_slider_set_value(slider_alt_chg_curr, alt_chg_curr, LV_ANIM_OFF);
                }
            }
        }
        if (!ignore_updates && dev->data.ac.powerLimit > 0) {
            int val = dev->data.ac.powerLimit;
            if (val != alt_pow_limit) {
                alt_pow_limit = val;
                if (label_alt_pow) lv_label_set_text_fmt(label_alt_pow, "%d W", alt_pow_limit);
                if (slider_alt_pow && !lv_slider_is_dragged(slider_alt_pow)) {
                    lv_slider_set_value(slider_alt_pow, alt_pow_limit, LV_ANIM_OFF);
                }
            }
        }
    }

    if (is_main_device) {
        // Simple filter: Ignore 0 if we already had a valid value
        if (temp_int == 0 && last_temp != -999 && last_temp != 0) {
             // Ignore glitch
        } else {
            if (first_run || temp_int != last_temp) {
                lv_label_set_text_fmt(label_temp, "Batt: %d C", temp_int);
                last_temp = temp_int;
            }
        }

        if (soc == 0 && last_soc > 0) {
             // Ignore glitch (jumping to 0%)
        } else {
            if (first_run || soc != last_soc) {
                lv_arc_set_value(arc_batt, soc);
                lv_label_set_text_fmt(label_soc, "%d%%", soc);
                last_soc = soc;
            }
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
                lv_label_set_text(lbl_ac_t, "AC\nON");
                lv_obj_add_state(btn_ac_toggle, LV_STATE_CHECKED);
            } else {
                lv_label_set_text(lbl_ac_t, "AC\nOFF");
                lv_obj_clear_state(btn_ac_toggle, LV_STATE_CHECKED);
            }
            last_ac_on = ac_on;
        }

        if (first_run || dc_on != last_dc_on) {
            if (dc_on) {
                lv_label_set_text(lbl_dc_t, "12V\nON");
                lv_obj_add_state(btn_dc_toggle, LV_STATE_CHECKED);
            } else {
                lv_label_set_text(lbl_dc_t, "12V\nOFF");
                lv_obj_clear_state(btn_dc_toggle, LV_STATE_CHECKED);
            }
            last_dc_on = dc_on;
        }

        first_run = false;
    }

    // Update Disconnected State
    if (dev->connected) {
        lv_obj_add_flag(label_disconnected, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(led_status_dot, lv_palette_main(LV_PALETTE_GREEN), 0); // Green dot for data
    } else {
        lv_obj_clear_flag(label_disconnected, LV_OBJ_FLAG_HIDDEN);
        lv_obj_set_style_bg_color(led_status_dot, lv_palette_main(LV_PALETTE_RED), 0);
    }


    // Blink indicator
    static bool toggle = false;
    if (dev->connected) {
       if (toggle) lv_obj_set_style_bg_color(led_status_dot, lv_palette_main(LV_PALETTE_GREEN), 0);
       else lv_obj_set_style_bg_color(led_status_dot, lv_palette_main(LV_PALETTE_RED), 0);
       toggle = !toggle;
    }
}
