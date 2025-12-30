#include "ui_lvgl.h"
#include "ui_icons.h"
#include "lvgl.h"
#include <stdio.h>

// --- Styles ---
static lv_style_t style_scr;
static lv_style_t style_panel;
static lv_style_t style_text_large;
static lv_style_t style_text_small;
static lv_style_t style_icon_label;
static lv_style_t style_btn_red;
static lv_style_t style_btn_default;
static lv_style_t style_btn_toggle_off;
static lv_style_t style_btn_toggle_on;

// --- Dashboard Widgets ---
static lv_obj_t * scr_dash;
static lv_obj_t * label_temp;
static lv_obj_t * arc_batt;
static lv_obj_t * label_soc;

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

    lv_style_init(&style_btn_toggle_off);
    lv_style_set_bg_color(&style_btn_toggle_off, lv_palette_darken(LV_PALETTE_GREY, 3));
    lv_style_set_text_color(&style_btn_toggle_off, lv_palette_main(LV_PALETTE_GREY));

    lv_style_init(&style_btn_toggle_on);
    lv_style_set_bg_color(&style_btn_toggle_on, lv_palette_main(LV_PALETTE_GREEN));
    lv_style_set_text_color(&style_btn_toggle_on, lv_color_white());
}

// --- Popup Handlers ---
static void event_power_off_click(lv_event_t * e) {
    lv_obj_clear_flag(cont_popup, LV_OBJ_FLAG_HIDDEN);
}

static void event_popup_no(lv_event_t * e) {
    lv_obj_add_flag(cont_popup, LV_OBJ_FLAG_HIDDEN);
}

static void event_popup_yes(lv_event_t * e) {
    // For now, just close popup as requested
    lv_obj_add_flag(cont_popup, LV_OBJ_FLAG_HIDDEN);
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
    lv_obj_set_style_text_font(icon, &ui_font_mdi, 0); // Re-apply if helper fails context? No, helper is fine.
    // Override font size? The font is generated at 28px.
    // ui_font_mdi size is 28.

    lv_obj_t * name = lv_label_create(card);
    lv_label_set_text(name, label_text);
    lv_obj_add_style(name, &style_text_small, 0);
    lv_obj_align(name, LV_ALIGN_TOP_RIGHT, 0, -5);

    *val_label_ptr = lv_label_create(card);
    lv_label_set_text(*val_label_ptr, "0 W");
    lv_obj_add_style(*val_label_ptr, &style_text_large, 0);
    // Scale down text large for card
    lv_obj_set_style_text_font(*val_label_ptr, &lv_font_montserrat_20, 0);
    lv_obj_align(*val_label_ptr, LV_ALIGN_BOTTOM_RIGHT, 0, 5);
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
    create_info_card(scr_dash, MDI_ICON_AC, "AC Out", 620, 260, &label_ac_val); // Reusing AC icon for AC Out

    // --- Center (Battery) ---
    arc_batt = lv_arc_create(scr_dash);
    lv_obj_set_size(arc_batt, 220, 220);
    lv_arc_set_rotation(arc_batt, 270);
    lv_arc_set_bg_angles(arc_batt, 0, 360);
    lv_arc_set_value(arc_batt, 50);
    lv_obj_align(arc_batt, LV_ALIGN_TOP_MID, 0, 80);
    lv_obj_remove_style(arc_batt, NULL, LV_PART_KNOB);   /*Be sure the knob is not displayed*/
    lv_obj_clear_flag(arc_batt, LV_OBJ_FLAG_CLICKABLE);  /*To not allow adjusting by click*/
    lv_obj_set_style_arc_color(arc_batt, lv_palette_main(LV_PALETTE_TEAL), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_batt, 15, LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc_batt, 15, LV_PART_MAIN);

    lv_obj_t * icon_bat = lv_label_create(scr_dash);
    ui_set_icon(icon_bat, MDI_ICON_BATTERY);
    lv_obj_set_style_text_font(icon_bat, &ui_font_mdi, 0);
    lv_obj_align(icon_bat, LV_ALIGN_CENTER, 0, -80); // Inside arc, top

    label_soc = lv_label_create(scr_dash);
    lv_label_set_text(label_soc, "50%");
    lv_obj_set_style_text_font(label_soc, &lv_font_montserrat_32, 0); // Bigger font if avail
    lv_obj_align(label_soc, LV_ALIGN_CENTER, 0, -30);

    // --- Footer Controls ---

    // Power Off (Bottom Left)
    lv_obj_t * btn_pwr = lv_btn_create(scr_dash);
    lv_obj_set_size(btn_pwr, 100, 50);
    lv_obj_align(btn_pwr, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_add_style(btn_pwr, &style_btn_red, 0);
    lv_obj_add_event_cb(btn_pwr, event_power_off_click, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_pwr = lv_label_create(btn_pwr);
    lv_label_set_text(lbl_pwr, "OFF");
    lv_obj_center(lbl_pwr);

    // Wave 2 (Bottom Left of Settings - Assuming Settings is Bottom Right)
    // "Bottom left of settings button" -> If Settings is at (700, 430), Wave 2 is at (580, 430)
    lv_obj_t * btn_settings = lv_btn_create(scr_dash);
    lv_obj_set_size(btn_settings, 60, 50);
    lv_obj_align(btn_settings, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_obj_add_style(btn_settings, &style_btn_default, 0);
    lv_obj_t * lbl_set = lv_label_create(btn_settings);
    ui_set_icon(lbl_set, MDI_ICON_SETTINGS);
    lv_obj_center(lbl_set);

    lv_obj_t * btn_wave2 = lv_btn_create(scr_dash);
    lv_obj_set_size(btn_wave2, 100, 50);
    lv_obj_align_to(btn_wave2, btn_settings, LV_ALIGN_OUT_LEFT_MID, -20, 0);
    lv_obj_add_style(btn_wave2, &style_btn_default, 0);
    lv_obj_t * lbl_wave = lv_label_create(btn_wave2);
    lv_label_set_text(lbl_wave, "Wave 2");
    lv_obj_center(lbl_wave);

    // Toggles (Center Bottom)
    lv_obj_t * btn_ac_toggle = lv_btn_create(scr_dash);
    lv_obj_set_size(btn_ac_toggle, 100, 60);
    lv_obj_align(btn_ac_toggle, LV_ALIGN_BOTTOM_MID, -60, -20);
    lv_obj_add_style(btn_ac_toggle, &style_btn_default, 0);
    lv_obj_add_flag(btn_ac_toggle, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_t * lbl_ac_t = lv_label_create(btn_ac_toggle);
    lv_label_set_text(lbl_ac_t, "AC\nOFF");
    lv_obj_set_style_text_align(lbl_ac_t, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(lbl_ac_t);

    lv_obj_t * btn_dc_toggle = lv_btn_create(scr_dash);
    lv_obj_set_size(btn_dc_toggle, 100, 60);
    lv_obj_align(btn_dc_toggle, LV_ALIGN_BOTTOM_MID, 60, -20);
    lv_obj_add_style(btn_dc_toggle, &style_btn_default, 0);
    lv_obj_add_flag(btn_dc_toggle, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_t * lbl_dc_t = lv_label_create(btn_dc_toggle);
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
    // Blur simulation: not easy on F4. Just opacity.

    lv_obj_t * lbl_msg = lv_label_create(popup_panel);
    lv_label_set_text(lbl_msg, "Really Power Off?");
    lv_obj_set_style_text_font(lbl_msg, &lv_font_montserrat_24, 0);
    lv_obj_align(lbl_msg, LV_ALIGN_TOP_MID, 0, 30);

    lv_obj_t * btn_yes = lv_btn_create(popup_panel);
    lv_obj_set_size(btn_yes, 100, 50);
    lv_obj_align(btn_yes, LV_ALIGN_BOTTOM_LEFT, 40, -20);
    lv_obj_add_style(btn_yes, &style_btn_red, 0);
    lv_obj_add_event_cb(btn_yes, event_popup_yes, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_yes = lv_label_create(btn_yes);
    lv_label_set_text(lbl_yes, "YES");
    lv_obj_center(lbl_yes);

    lv_obj_t * btn_no = lv_btn_create(popup_panel);
    lv_obj_set_size(btn_no, 100, 50);
    lv_obj_align(btn_no, LV_ALIGN_BOTTOM_RIGHT, -40, -20);
    lv_obj_add_style(btn_no, &style_btn_default, 0);
    lv_obj_add_event_cb(btn_no, event_popup_no, LV_EVENT_CLICKED, NULL);
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
    create_dashboard();

    lv_scr_load(scr_dash);
}

void UI_LVGL_Update(DeviceStatus* dev) {
    if (!dev) return;

    // Map data (Simplified)
    int soc = 0;
    int in_solar=0, in_ac=0, in_alt=0;
    int out_usb=0, out_12v=0, out_ac=0;
    float temp = 25.0f;

    // Mapping logic
    if (dev->id == DEV_TYPE_DELTA_PRO_3) {
        soc = (int)dev->data.d3p.batteryLevel;
        in_ac = (int)dev->data.d3p.acInputPower;
        in_solar = (int)(dev->data.d3p.solarLvPower + dev->data.d3p.solarHvPower);
        in_alt = (int)dev->data.d3p.dcLvInputPower; // Approx
        out_ac = (int)(dev->data.d3p.acLvOutputPower + dev->data.d3p.acHvOutputPower);
        out_12v = (int)dev->data.d3p.dc12vOutputPower;
        out_usb = (int)(dev->data.d3p.usbaOutputPower + dev->data.d3p.usbcOutputPower);
        temp = dev->data.d3p.cellTemperature;
    } else if (dev->id == DEV_TYPE_DELTA_3) {
        soc = (int)dev->data.d3.batteryLevel;
        in_ac = (int)dev->data.d3.acInputPower;
        in_solar = (int)dev->data.d3.solarInputPower;
        in_alt = (int)dev->data.d3.dcPortInputPower;
        out_ac = (int)dev->data.d3.acOutputPower;
        out_12v = (int)dev->data.d3.dc12vOutputPower;
        out_usb = (int)(dev->data.d3.usbaOutputPower + dev->data.d3.usbcOutputPower);
        temp = dev->data.d3.cellTemperature;
    }

    lv_label_set_text_fmt(label_temp, "%.1f C", temp);
    lv_arc_set_value(arc_batt, soc);
    lv_label_set_text_fmt(label_soc, "%d%%", soc);

    lv_label_set_text_fmt(label_solar_val, "%d W", in_solar);
    lv_label_set_text_fmt(label_grid_val, "%d W", in_ac);
    lv_label_set_text_fmt(label_car_val, "%d W", in_alt);
    lv_label_set_text_fmt(label_usb_val, "%d W", out_usb);
    lv_label_set_text_fmt(label_12v_val, "%d W", out_12v);
    lv_label_set_text_fmt(label_ac_val, "%d W", out_ac);
}
