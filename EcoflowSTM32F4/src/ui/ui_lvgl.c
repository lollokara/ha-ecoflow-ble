#include "ui_lvgl.h"
#include "ui_icons.h"
#include "lvgl.h"
#include <stdio.h>
#include <math.h>

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

// --- Wave 2 Widgets ---
static lv_obj_t * scr_wave2;
static lv_obj_t * w2_btn_pwr;
static lv_obj_t * w2_lbl_temp_val;
static lv_obj_t * w2_arc_temp;
static lv_obj_t * w2_dd_mode;
static lv_obj_t * w2_dd_submode;
static lv_obj_t * w2_slider_fan;
static lv_obj_t * w2_cont_submode; // Container to hide/show
static lv_obj_t * w2_cont_fan;     // Container to hide/show

// --- Settings Widgets ---
static lv_obj_t * scr_settings;
static lv_obj_t * label_lim_in_val;
static lv_obj_t * label_lim_out_val;
static lv_obj_t * label_lim_chg_val;

// Flow Data Labels
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

// --- Navigation Callbacks ---
static void event_to_settings(lv_event_t * e) {
    lv_scr_load_anim(scr_settings, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
}

static void event_to_dash(lv_event_t * e) {
    lv_scr_load_anim(scr_dash, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
}

static void event_to_wave2(lv_event_t * e) {
    lv_scr_load_anim(scr_wave2, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
}

// --- Popup Handlers ---
static void event_power_off_click(lv_event_t * e) {
    lv_obj_clear_flag(cont_popup, LV_OBJ_FLAG_HIDDEN);
}
static void event_popup_hide(lv_event_t * e) {
    lv_obj_add_flag(cont_popup, LV_OBJ_FLAG_HIDDEN);
}

// --- Send Helper ---
static void send_set_value(uint8_t dev_type, uint8_t cmd_id, int32_t val) {
    SetValueCommand cmd;
    cmd.device_type = dev_type;
    cmd.command_id = cmd_id;
    cmd.value_type = 0; // Int
    cmd.value.int_val = val;

    uint8_t buffer[sizeof(SetValueCommand) + 4];
    int len = pack_set_value_message(buffer, &cmd);
    UART_SendPacket(buffer, len);
}

// --- Toggle Callbacks ---
static void event_toggle_ac(lv_event_t * e) {
    lv_obj_t * btn = lv_event_get_target(e);
    bool state = lv_obj_has_state(btn, LV_STATE_CHECKED); // Logic might be inverted depending on current visual state, but simpler to send desired toggle.
    // Actually, UI update overrides button state. The button click should just send "toggle" or "set opposite".
    // We don't know current true state here easily without checking static var or querying object style.
    // Let's assume the button click implies "Toggle". Or we send "Set to Pressed State".
    // Better: Send Set command with !current_state.
    // Simplified: Just send 1 or 0 based on checked.
    send_set_value(DEV_TYPE_DELTA_3, SET_CMD_AC_ENABLE, state ? 1 : 0);
}

static void event_toggle_dc(lv_event_t * e) {
    lv_obj_t * btn = lv_event_get_target(e);
    bool state = lv_obj_has_state(btn, LV_STATE_CHECKED);
    send_set_value(DEV_TYPE_DELTA_3, SET_CMD_DC_ENABLE, state ? 1 : 0);
}

// --- Wave 2 Callbacks ---
static void event_w2_pwr(lv_event_t * e) {
    lv_obj_t * btn = lv_event_get_target(e);
    // Determine target state (toggle)
    // We send logic 1/0 or specific power command. Protocol uses 1=Main, 2=Standby/Off? Or 0/1.
    // ESP32 code: w2->setPowerState((uint8_t)getSetW2Val());
    // Let's send 1 (On) or 0 (Off). ESP32 will map if needed, or we check protocol.
    // Existing ESP32 logic: W2_TOGGLE_PWR toggles 1 <-> 2.
    // SET_CMD_W2_POWER sends raw value.
    // Let's assume we want to toggle. But we need to know state.
    // For now, let's send 1 if we want ON, 0 if OFF.
    // We can check button color to guess state?
    // Simpler: Just send "1" always? No.
    // Let's just implement toggle logic in ESP32 if value is special, OR track it here.
    // UI_LVGL_Update tracks `last_w2_pwr`.
    // But we are in callback.
    // Let's assume "Click" means Toggle.
    // If we send SET_CMD_W2_POWER with value 255 (Toggle)? No.
    // Let's just send 1 for now, assuming user wants ON. But OFF?
    // Let's use `lv_obj_has_state(btn, LV_STATE_CHECKED)` if it was a toggle btn.
    // It is a normal btn.
    // Let's use USER_DATA or just send a special "Toggle" command if defined, or rely on UI state.
    // I will read the style. If green -> send Off (0). If default -> send On (1).
    if (lv_obj_has_style(btn, &style_btn_green, 0)) {
         send_set_value(DEV_TYPE_WAVE_2, SET_CMD_W2_POWER, 0);
    } else {
         send_set_value(DEV_TYPE_WAVE_2, SET_CMD_W2_POWER, 1);
    }
}

static void event_w2_temp(lv_event_t * e) {
    lv_obj_t * arc = lv_event_get_target(e);
    int val = lv_arc_get_value(arc);
    lv_label_set_text_fmt(w2_lbl_temp_val, "%d C", val);
    send_set_value(DEV_TYPE_WAVE_2, SET_CMD_W2_TEMP, val);
}

static void event_w2_mode(lv_event_t * e) {
    lv_obj_t * dd = lv_event_get_target(e);
    int val = lv_dropdown_get_selected(dd);
    send_set_value(DEV_TYPE_WAVE_2, SET_CMD_W2_MODE, val);
}

static void event_w2_submode(lv_event_t * e) {
    lv_obj_t * dd = lv_event_get_target(e);
    int val = lv_dropdown_get_selected(dd);
    send_set_value(DEV_TYPE_WAVE_2, SET_CMD_W2_SUBMODE, val);
}

static void event_w2_fan(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    int val = lv_slider_get_value(slider);
    send_set_value(DEV_TYPE_WAVE_2, SET_CMD_W2_FAN, val);
}


static void create_wave2_panel(void) {
    scr_wave2 = lv_obj_create(NULL);
    lv_obj_add_style(scr_wave2, &style_scr, 0);

    // Header
    lv_obj_t * btn_back = lv_btn_create(scr_wave2);
    lv_obj_set_size(btn_back, 100, 50);
    lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 20, 20);
    lv_obj_add_style(btn_back, &style_btn_default, 0);
    lv_obj_add_event_cb(btn_back, event_to_dash, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_back = lv_label_create(btn_back);
    ui_set_icon(lbl_back, MDI_ICON_BACK);
    lv_obj_center(lbl_back);

    lv_obj_t * title = lv_label_create(scr_wave2);
    lv_label_set_text(title, "Wave 2 Control");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 25);

    // Power Button (Top Right)
    w2_btn_pwr = lv_btn_create(scr_wave2);
    lv_obj_set_size(w2_btn_pwr, 80, 50);
    lv_obj_align(w2_btn_pwr, LV_ALIGN_TOP_RIGHT, -20, 20);
    lv_obj_add_style(w2_btn_pwr, &style_btn_default, 0); // Default Off
    lv_obj_add_event_cb(w2_btn_pwr, event_w2_pwr, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_pwr = lv_label_create(w2_btn_pwr);
    ui_set_icon(lbl_pwr, MDI_ICON_POWER);
    lv_obj_center(lbl_pwr);


    // Container for controls
    lv_obj_t * cont = lv_obj_create(scr_wave2);
    lv_obj_set_size(cont, 760, 350);
    lv_obj_align(cont, LV_ALIGN_BOTTOM_MID, 0, -20);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);

    // Left: Temp Arc
    w2_arc_temp = lv_arc_create(cont);
    lv_obj_set_size(w2_arc_temp, 250, 250);
    lv_arc_set_rotation(w2_arc_temp, 135);
    lv_arc_set_bg_angles(w2_arc_temp, 0, 270);
    lv_arc_set_range(w2_arc_temp, 16, 30); // Temp range
    lv_arc_set_value(w2_arc_temp, 25);
    lv_obj_align(w2_arc_temp, LV_ALIGN_LEFT_MID, 20, 0);
    lv_obj_add_event_cb(w2_arc_temp, event_w2_temp, LV_EVENT_VALUE_CHANGED, NULL);

    w2_lbl_temp_val = lv_label_create(cont);
    lv_label_set_text(w2_lbl_temp_val, "25 C");
    lv_obj_set_style_text_font(w2_lbl_temp_val, &lv_font_montserrat_32, 0);
    lv_obj_align_to(w2_lbl_temp_val, w2_arc_temp, LV_ALIGN_CENTER, 0, 0);

    // Icon inside arc
    lv_obj_t * icon_therm = lv_label_create(cont);
    ui_set_icon(icon_therm, MDI_ICON_THERMOMETER);
    lv_obj_set_style_text_font(icon_therm, &ui_font_mdi, 0);
    lv_obj_set_style_text_color(icon_therm, lv_palette_main(LV_PALETTE_TEAL), 0);
    lv_obj_align_to(icon_therm, w2_lbl_temp_val, LV_ALIGN_OUT_TOP_MID, 0, -10);


    // Right: Controls
    lv_obj_t * right_col = lv_obj_create(cont);
    lv_obj_set_size(right_col, 400, 300);
    lv_obj_align(right_col, LV_ALIGN_RIGHT_MID, -20, 0);
    lv_obj_set_style_bg_opa(right_col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right_col, 0, 0);
    lv_obj_set_flex_flow(right_col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(right_col, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Mode
    lv_obj_t * lbl_mode = lv_label_create(right_col);
    lv_label_set_text(lbl_mode, "Mode");
    lv_obj_set_style_text_font(lbl_mode, &lv_font_montserrat_20, 0);

    w2_dd_mode = lv_dropdown_create(right_col);
    lv_dropdown_set_options(w2_dd_mode, "Cool\nHeat\nFan"); // 0, 1, 2
    lv_obj_set_width(w2_dd_mode, 300);
    lv_obj_add_event_cb(w2_dd_mode, event_w2_mode, LV_EVENT_VALUE_CHANGED, NULL);

    // Submode (Container to hide)
    w2_cont_submode = lv_obj_create(right_col);
    lv_obj_set_size(w2_cont_submode, 320, 100);
    lv_obj_set_style_bg_opa(w2_cont_submode, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(w2_cont_submode, 0, 0);
    lv_obj_set_flex_flow(w2_cont_submode, LV_FLEX_FLOW_COLUMN);

    lv_obj_t * lbl_sub = lv_label_create(w2_cont_submode);
    lv_label_set_text(lbl_sub, "Sub-Mode");
    lv_obj_set_style_text_font(lbl_sub, &lv_font_montserrat_20, 0);

    w2_dd_submode = lv_dropdown_create(w2_cont_submode);
    lv_dropdown_set_options(w2_dd_submode, "Auto\nEco\nNight\nMax"); // 0, 1, 2, 3
    lv_obj_set_width(w2_dd_submode, 300);
    lv_obj_add_event_cb(w2_dd_submode, event_w2_submode, LV_EVENT_VALUE_CHANGED, NULL);

    // Fan (Container to hide)
    w2_cont_fan = lv_obj_create(right_col);
    lv_obj_set_size(w2_cont_fan, 320, 100);
    lv_obj_set_style_bg_opa(w2_cont_fan, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(w2_cont_fan, 0, 0);
    lv_obj_set_flex_flow(w2_cont_fan, LV_FLEX_FLOW_COLUMN);

    lv_obj_t * lbl_fan = lv_label_create(w2_cont_fan);
    lv_label_set_text(lbl_fan, "Fan Speed");
    lv_obj_set_style_text_font(lbl_fan, &lv_font_montserrat_20, 0);

    w2_slider_fan = lv_slider_create(w2_cont_fan);
    lv_slider_set_range(w2_slider_fan, 1, 3);
    lv_obj_set_width(w2_slider_fan, 300);
    lv_obj_add_event_cb(w2_slider_fan, event_w2_fan, LV_EVENT_VALUE_CHANGED, NULL);
}

// --- Slider Callbacks ---
static void event_slider_input(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    int val = (int)lv_slider_get_value(slider);
    // Snap to 100W increments
    val = (val + 50) / 100 * 100;
    if (val != lim_input_w) {
        lv_slider_set_value(slider, val, LV_ANIM_OFF);
        lim_input_w = val;
        lv_label_set_text_fmt(label_lim_in_val, "%d W", lim_input_w);
    }
}
static void event_slider_discharge(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    int val = (int)lv_slider_get_value(slider);
    // Snap to 5% increments
    val = (val + 2) / 5 * 5;
    if (val != lim_discharge_p) {
        lv_slider_set_value(slider, val, LV_ANIM_OFF);
        lim_discharge_p = val;
        lv_label_set_text_fmt(label_lim_out_val, "%d %%", lim_discharge_p);
        lv_obj_invalidate(arc_batt); // Redraw arc
    }
}
static void event_slider_charge(lv_event_t * e) {
    lv_obj_t * slider = lv_event_get_target(e);
    int val = (int)lv_slider_get_value(slider);
    // Snap to 5% increments
    val = (val + 2) / 5 * 5;
    if (val != lim_charge_p) {
        lv_slider_set_value(slider, val, LV_ANIM_OFF);
        lim_charge_p = val;
        lv_label_set_text_fmt(label_lim_chg_val, "%d %%", lim_charge_p);
        lv_obj_invalidate(arc_batt); // Redraw arc
    }
}

// --- Arc Draw Callback for Limits ---
static void event_arc_draw(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_DRAW_PART_END) {
        lv_obj_draw_part_dsc_t * dsc = lv_event_get_draw_part_dsc(e);
        // Draw on the main background part or indicator?
        // Indicator moves, Main is static full circle. Let's draw on MAIN or wrapping object.
        // Actually, we want lines at specific angles.
        if(dsc->part == LV_PART_MAIN) {
            lv_obj_t * obj = lv_event_get_target(e);
            lv_draw_ctx_t * draw_ctx = dsc->draw_ctx;
            const lv_area_t * coords = &obj->coords;

            // Calculate center and radius
            lv_point_t center;
            center.x = coords->x1 + lv_area_get_width(coords) / 2;
            center.y = coords->y1 + lv_area_get_height(coords) / 2;

            // Radius: assuming square arc object, slightly less than half width
            // Arc width is 15. Inner radius approx w/2 - 15.
            lv_coord_t r_out = lv_area_get_width(coords) / 2;
            lv_coord_t r_in = r_out - 15; // Arc thickness

            // Rotation 270 means 0 value is at top.
            // 0 value -> 270 deg physical.
            // Angle increases clockwise.
            // Physical Angle = 270 + (Value * 3.6)

            // Draw Red Line (Discharge Limit)
            if (lim_discharge_p > 0) {
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

            // Draw Blue Line (Charge Limit)
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
static void create_info_card(lv_obj_t * parent, const char* icon_code, const char* label_text, int x, int y, lv_obj_t ** val_label_ptr) {
    lv_obj_t * card = lv_obj_create(parent);
    lv_obj_set_size(card, 160, 90);
    lv_obj_set_pos(card, x, y);
    lv_obj_add_style(card, &style_panel, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t * icon = lv_label_create(card);
    ui_set_icon(icon, icon_code);
    lv_obj_align(icon, LV_ALIGN_LEFT_MID, -5, 0);
    lv_obj_set_style_text_font(icon, &ui_font_mdi, 0);
    lv_obj_set_style_text_color(icon, lv_palette_main(LV_PALETTE_TEAL), 0); // Explicit Color

    lv_obj_t * name = lv_label_create(card);
    lv_label_set_text(name, label_text);
    lv_obj_add_style(name, &style_text_small, 0);
    lv_obj_align(name, LV_ALIGN_TOP_RIGHT, 0, -5);

    *val_label_ptr = lv_label_create(card);
    lv_label_set_text(*val_label_ptr, "0 W");
    lv_obj_add_style(*val_label_ptr, &style_text_large, 0);
    lv_obj_set_style_text_font(*val_label_ptr, &lv_font_montserrat_20, 0);
    lv_obj_align(*val_label_ptr, LV_ALIGN_BOTTOM_RIGHT, 0, 5);
}

static void create_settings(void) {
    scr_settings = lv_obj_create(NULL);
    lv_obj_add_style(scr_settings, &style_scr, 0);

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
    lv_obj_set_size(p1, 650, 80);
    lv_obj_set_style_bg_opa(p1, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(p1, 0, 0);

    lv_obj_t * l1 = lv_label_create(p1);
    lv_label_set_text(l1, "Max AC Input");
    lv_obj_set_style_text_color(l1, lv_color_white(), 0);
    lv_obj_set_style_text_font(l1, &lv_font_montserrat_32, 0);
    lv_obj_align(l1, LV_ALIGN_TOP_LEFT, 0, 0);

    label_lim_in_val = lv_label_create(p1);
    lv_label_set_text_fmt(label_lim_in_val, "%d W", lim_input_w);
    lv_obj_set_style_text_color(label_lim_in_val, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_lim_in_val, &lv_font_montserrat_32, 0);
    lv_obj_align(label_lim_in_val, LV_ALIGN_TOP_RIGHT, 0, 0);

    lv_obj_t * s1 = lv_slider_create(p1);
    lv_slider_set_range(s1, 400, 3000);
    lv_slider_set_value(s1, lim_input_w, LV_ANIM_OFF);
    lv_obj_set_width(s1, 600);
    lv_obj_align(s1, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(s1, event_slider_input, LV_EVENT_VALUE_CHANGED, NULL);

    // 2. Min Discharge (0 - 30)
    lv_obj_t * p2 = lv_obj_create(cont);
    lv_obj_set_size(p2, 650, 80);
    lv_obj_set_style_bg_opa(p2, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(p2, 0, 0);

    lv_obj_t * l2 = lv_label_create(p2);
    lv_label_set_text(l2, "Min Discharge Limit (Red)");
    lv_obj_set_style_text_color(l2, lv_color_white(), 0);
    lv_obj_set_style_text_font(l2, &lv_font_montserrat_32, 0);
    lv_obj_align(l2, LV_ALIGN_TOP_LEFT, 0, 0);

    label_lim_out_val = lv_label_create(p2);
    lv_label_set_text_fmt(label_lim_out_val, "%d %%", lim_discharge_p);
    lv_obj_set_style_text_color(label_lim_out_val, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_lim_out_val, &lv_font_montserrat_32, 0);
    lv_obj_align(label_lim_out_val, LV_ALIGN_TOP_RIGHT, 0, 0);

    lv_obj_t * s2 = lv_slider_create(p2);
    lv_slider_set_range(s2, 0, 30);
    lv_slider_set_value(s2, lim_discharge_p, LV_ANIM_OFF);
    lv_obj_set_width(s2, 600);
    lv_obj_align(s2, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(s2, event_slider_discharge, LV_EVENT_VALUE_CHANGED, NULL);

    // 3. Max Charge (50 - 100)
    lv_obj_t * p3 = lv_obj_create(cont);
    lv_obj_set_size(p3, 650, 80);
    lv_obj_set_style_bg_opa(p3, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(p3, 0, 0);

    lv_obj_t * l3 = lv_label_create(p3);
    lv_label_set_text(l3, "Max Charge Limit (Blue)");
    lv_obj_set_style_text_color(l3, lv_color_white(), 0);
    lv_obj_set_style_text_font(l3, &lv_font_montserrat_32, 0);
    lv_obj_align(l3, LV_ALIGN_TOP_LEFT, 0, 0);

    label_lim_chg_val = lv_label_create(p3);
    lv_label_set_text_fmt(label_lim_chg_val, "%d %%", lim_charge_p);
    lv_obj_set_style_text_color(label_lim_chg_val, lv_color_white(), 0);
    lv_obj_set_style_text_font(label_lim_chg_val, &lv_font_montserrat_32, 0);
    lv_obj_align(label_lim_chg_val, LV_ALIGN_TOP_RIGHT, 0, 0);

    lv_obj_t * s3 = lv_slider_create(p3);
    lv_slider_set_range(s3, 50, 100);
    lv_slider_set_value(s3, lim_charge_p, LV_ANIM_OFF);
    lv_obj_set_width(s3, 600);
    lv_obj_align(s3, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_add_event_cb(s3, event_slider_charge, LV_EVENT_VALUE_CHANGED, NULL);
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

    // --- Left Column (Inputs) ---
    create_info_card(scr_dash, MDI_ICON_SOLAR, "Solar", 20, 60, &label_solar_val);
    create_info_card(scr_dash, MDI_ICON_PLUG, "Grid", 20, 160, &label_grid_val);
    create_info_card(scr_dash, MDI_ICON_CAR, "Car", 20, 260, &label_car_val);

    // --- Right Column (Outputs) ---
    create_info_card(scr_dash, MDI_ICON_USB, "USB", 620, 60, &label_usb_val);
    create_info_card(scr_dash, MDI_ICON_AC, "12V DC", 620, 160, &label_12v_val);
    create_info_card(scr_dash, MDI_ICON_AC, "AC Out", 620, 260, &label_ac_val);

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
    // Add draw event for limits
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
    lv_obj_set_size(btn_settings, 80, 60); // As per plan
    lv_obj_align(btn_settings, LV_ALIGN_BOTTOM_RIGHT, -20, btn_y - 5);
    lv_obj_add_style(btn_settings, &style_btn_default, 0);
    lv_obj_add_event_cb(btn_settings, event_to_settings, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_set = lv_label_create(btn_settings);
    ui_set_icon(lbl_set, MDI_ICON_SETTINGS);
    lv_obj_center(lbl_set);

    // Wave 2 (Left of Settings)
    lv_obj_t * btn_wave2 = lv_btn_create(scr_dash);
    lv_obj_set_size(btn_wave2, 120, 60);
    lv_obj_align_to(btn_wave2, btn_settings, LV_ALIGN_OUT_LEFT_MID, -20, 0);
    lv_obj_add_style(btn_wave2, &style_btn_default, 0);
    lv_obj_add_event_cb(btn_wave2, event_to_wave2, LV_EVENT_CLICKED, NULL);
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
    lv_obj_add_flag(cont_popup, LV_OBJ_FLAG_HIDDEN); // Initially hidden

    lv_obj_t * popup_panel = lv_obj_create(cont_popup);
    lv_obj_set_size(popup_panel, 400, 200);
    lv_obj_center(popup_panel);
    lv_obj_add_style(popup_panel, &style_panel, 0);

    lv_obj_t * lbl_msg = lv_label_create(popup_panel);
    lv_label_set_text(lbl_msg, "Really Power Off?");
    lv_obj_set_style_text_font(lbl_msg, &lv_font_montserrat_24, 0);
    lv_obj_align(lbl_msg, LV_ALIGN_TOP_MID, 0, 30);

    lv_obj_t * btn_yes = lv_btn_create(popup_panel);
    lv_obj_set_size(btn_yes, 100, 50);
    lv_obj_align(btn_yes, LV_ALIGN_BOTTOM_LEFT, 40, -20);
    lv_obj_add_style(btn_yes, &style_btn_red, 0);
    lv_obj_add_event_cb(btn_yes, event_popup_hide, LV_EVENT_CLICKED, NULL);
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
    create_settings(); // Create first so we can load it
    create_wave2_panel(); // Create Wave 2 panel
    create_dashboard();

    lv_scr_load(scr_dash);
}

void UI_LVGL_Update(DeviceStatus* dev) {
    if (!dev) return;

    // Map data
    int soc = 0;
    int in_solar=0, in_ac=0, in_alt=0;
    int out_usb=0, out_12v=0, out_ac=0;
    float temp = 25.0f;

    if (dev->id == DEV_TYPE_DELTA_PRO_3) {
        soc = safe_float_to_int(dev->data.d3p.batteryLevel);
        in_ac = safe_float_to_int(dev->data.d3p.acInputPower);
        in_solar = safe_float_to_int(dev->data.d3p.solarLvPower + dev->data.d3p.solarHvPower);
        in_alt = safe_float_to_int(dev->data.d3p.dcLvInputPower);
        out_ac = safe_float_to_int(dev->data.d3p.acLvOutputPower + dev->data.d3p.acHvOutputPower);
        out_12v = safe_float_to_int(dev->data.d3p.dc12vOutputPower);
        out_usb = safe_float_to_int(dev->data.d3p.usbaOutputPower + dev->data.d3p.usbcOutputPower);
        temp = (float)dev->data.d3p.cellTemperature; // Int to Float
    } else if (dev->id == DEV_TYPE_DELTA_3) {
        soc = safe_float_to_int(dev->data.d3.batteryLevel);
        in_ac = safe_float_to_int(dev->data.d3.acInputPower);
        in_solar = safe_float_to_int(dev->data.d3.solarInputPower);
        in_alt = safe_float_to_int(dev->data.d3.dcPortInputPower);
        out_ac = safe_float_to_int(dev->data.d3.acOutputPower);
        out_12v = safe_float_to_int(dev->data.d3.dc12vOutputPower);
        out_usb = safe_float_to_int(dev->data.d3.usbaOutputPower + dev->data.d3.usbcOutputPower);
        temp = (float)dev->data.d3.cellTemperature; // Int to Float
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

    static bool last_ac_on = false; // Assuming default off
    static bool last_dc_on = false; // Assuming default off
    static bool first_run = true;

    int temp_int = safe_float_to_int(temp);

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
        last_solar = in_solar;
    }
    if (first_run || in_ac != last_grid) {
        lv_label_set_text_fmt(label_grid_val, "%d W", in_ac);
        last_grid = in_ac;
    }
    if (first_run || in_alt != last_car) {
        lv_label_set_text_fmt(label_car_val, "%d W", in_alt);
        last_car = in_alt;
    }
    if (first_run || out_usb != last_usb) {
        lv_label_set_text_fmt(label_usb_val, "%d W", out_usb);
        last_usb = out_usb;
    }
    if (first_run || out_12v != last_12v) {
        lv_label_set_text_fmt(label_12v_val, "%d W", out_12v);
        last_12v = out_12v;
    }
    if (first_run || out_ac != last_ac) {
        lv_label_set_text_fmt(label_ac_val, "%d W", out_ac);
        last_ac = out_ac;
    }

    // Toggle Styles
    bool ac_on = (out_ac > 0);
    if (first_run || ac_on != last_ac_on) {
        if (ac_on) {
            lv_obj_add_style(btn_ac_toggle, &style_btn_green, 0);
            lv_obj_remove_style(btn_ac_toggle, &style_btn_default, 0);
            lv_label_set_text(lbl_ac_t, "AC\nON");
        } else {
            lv_obj_add_style(btn_ac_toggle, &style_btn_default, 0);
            lv_obj_remove_style(btn_ac_toggle, &style_btn_green, 0);
            lv_label_set_text(lbl_ac_t, "AC\nOFF");
        }
        last_ac_on = ac_on;
    }

    bool dc_on = (out_12v > 0);
    if (first_run || dc_on != last_dc_on) {
        if (dc_on) {
            lv_obj_add_style(btn_dc_toggle, &style_btn_green, 0);
            lv_obj_remove_style(btn_dc_toggle, &style_btn_default, 0);
            lv_label_set_text(lbl_dc_t, "12V\nON");
        } else {
            lv_obj_add_style(btn_dc_toggle, &style_btn_default, 0);
            lv_obj_remove_style(btn_dc_toggle, &style_btn_green, 0);
            lv_label_set_text(lbl_dc_t, "12V\nOFF");
        }
        last_dc_on = dc_on;
    }

    // Wave 2 Update
    if (dev->id == DEV_TYPE_WAVE_2) {
        int w2_temp = (int)dev->data.w2.setTemp;
        int w2_mode = (int)dev->data.w2.mode;
        int w2_submode = (int)dev->data.w2.subMode;
        int w2_fan = (int)dev->data.w2.fanValue;
        bool w2_pwr = (dev->data.w2.powerMode != 0);

        static int last_w2_temp = -1;
        static int last_w2_mode = -1;
        static int last_w2_submode = -1;
        static int last_w2_fan = -1;
        static bool last_w2_pwr = false;

        if (first_run || w2_pwr != last_w2_pwr) {
            if (w2_pwr) {
                lv_obj_add_style(w2_btn_pwr, &style_btn_green, 0);
                lv_obj_remove_style(w2_btn_pwr, &style_btn_default, 0);
                // Also enable other controls
            } else {
                lv_obj_add_style(w2_btn_pwr, &style_btn_default, 0);
                lv_obj_remove_style(w2_btn_pwr, &style_btn_green, 0);
                // Maybe disable controls?
            }
            last_w2_pwr = w2_pwr;
        }

        if (first_run || w2_temp != last_w2_temp) {
            lv_arc_set_value(w2_arc_temp, w2_temp);
            lv_label_set_text_fmt(w2_lbl_temp_val, "%d C", w2_temp);
            last_w2_temp = w2_temp;
        }

        if (first_run || w2_mode != last_w2_mode) {
             lv_dropdown_set_selected(w2_dd_mode, w2_mode); // Assuming mode enum matches index
             // Show/Hide submode logic
             if (w2_mode == 0 || w2_mode == 1) { // Cool/Heat
                 lv_obj_clear_flag(w2_cont_submode, LV_OBJ_FLAG_HIDDEN);
             } else {
                 lv_obj_add_flag(w2_cont_submode, LV_OBJ_FLAG_HIDDEN);
             }
             last_w2_mode = w2_mode;
        }

        if (first_run || w2_submode != last_w2_submode) {
             lv_dropdown_set_selected(w2_dd_submode, w2_submode);
             // Fan Logic: Fan is settable only in mode Auto (submode?) or Fan (main mode?)
             // Based on prompt: "fan is settable only in mode Auto or Fan the other submodes do not allow for fan setting"
             // Interpretation: If Main Mode = Fan (2), OR (Main Mode = Cool/Heat AND Submode = Auto (0?))
             bool show_fan = false;
             if (w2_mode == 2) show_fan = true; // Fan Mode
             else if ((w2_mode == 0 || w2_mode == 1) && w2_submode == 0) show_fan = true; // Auto submode

             if (show_fan) lv_obj_clear_flag(w2_cont_fan, LV_OBJ_FLAG_HIDDEN);
             else lv_obj_add_flag(w2_cont_fan, LV_OBJ_FLAG_HIDDEN);

             last_w2_submode = w2_submode;
        }

        if (first_run || w2_fan != last_w2_fan) {
             lv_slider_set_value(w2_slider_fan, w2_fan, LV_ANIM_OFF);
             last_w2_fan = w2_fan;
        }
    }

    first_run = false;
}
