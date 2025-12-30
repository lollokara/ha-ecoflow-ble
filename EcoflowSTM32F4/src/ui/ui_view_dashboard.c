#include "ui_core.h"
#include "ui_icons.h"
#include "ui_lvgl.h"
#include "lvgl.h"
#include <stdio.h>

// LVGL Objects
static lv_obj_t * scr_main;
static lv_obj_t * label_soc;
static lv_obj_t * label_in_power;
static lv_obj_t * label_out_power;
static lv_obj_t * label_time_rem;
static lv_obj_t * btn_ac;
static lv_obj_t * btn_dc;
static lv_obj_t * btn_wave2;
static lv_obj_t * btn_power_off;

static lv_obj_t * lbl_ac_pwr;
static lv_obj_t * lbl_dc_pwr;

// Popup Objects
static lv_obj_t * popup_power_off = NULL;

// Callbacks
static void btn_toggle_event_cb(lv_event_t * e);
static void btn_power_off_cb(lv_event_t * e);
static void popup_cb(lv_event_t * e);

// --- Layout Helpers ---

static lv_obj_t * create_stat_item(lv_obj_t * parent, const char * title, const char * value) {
    lv_obj_t * cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 240, 80);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * l_val = lv_label_create(cont);
    lv_label_set_text(l_val, value);
    lv_obj_set_style_text_font(l_val, &lv_font_montserrat_28, 0);
    lv_obj_set_style_text_color(l_val, lv_color_hex(0xFFFFFF), 0);

    lv_obj_t * l_title = lv_label_create(cont);
    lv_label_set_text(l_title, title);
    lv_obj_set_style_text_font(l_title, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(l_title, lv_color_hex(0xAAAAAA), 0);

    return l_val; // Return value label for updates
}

static lv_obj_t * create_big_toggle(lv_obj_t * parent, const char * label_text, ui_icon_type_t icon, lv_color_t color, lv_obj_t ** lbl_pwr_out) {
    lv_obj_t * btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 360, 160);
    lv_obj_add_flag(btn, LV_OBJ_FLAG_CHECKABLE);
    lv_obj_clear_flag(btn, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x333333), 0);
    lv_obj_set_style_bg_color(btn, color, LV_STATE_CHECKED);

    lv_obj_t * l_icon = ui_create_icon(btn, icon, 48, lv_color_hex(0xFFFFFF));
    lv_obj_align(l_icon, LV_ALIGN_LEFT_MID, 20, 0);

    // Container for text to center vertically
    lv_obj_t * txt_cont = lv_obj_create(btn);
    lv_obj_set_size(txt_cont, 200, 100);
    lv_obj_align(txt_cont, LV_ALIGN_CENTER, 20, 0);
    lv_obj_set_style_bg_opa(txt_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(txt_cont, 0, 0);
    lv_obj_clear_flag(txt_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(txt_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(txt_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_t * l_text = lv_label_create(txt_cont);
    lv_label_set_text(l_text, label_text);
    lv_obj_set_style_text_font(l_text, &lv_font_montserrat_32, 0); // Bigger font
    lv_obj_set_style_text_color(l_text, lv_color_hex(0xFFFFFF), 0);

    // Power Label (Hidden if 0W/Off typically, or "0W")
    if (lbl_pwr_out) {
        *lbl_pwr_out = lv_label_create(txt_cont);
        lv_label_set_text(*lbl_pwr_out, "0 W");
        lv_obj_set_style_text_font(*lbl_pwr_out, &lv_font_montserrat_20, 0);
        lv_obj_set_style_text_color(*lbl_pwr_out, lv_color_hex(0xDDDDDD), 0);
    }

    lv_obj_add_event_cb(btn, btn_toggle_event_cb, LV_EVENT_VALUE_CHANGED, NULL);

    return btn;
}

// --- Init ---

void UI_Dashboard_Init(void) {
    scr_main = lv_scr_act();
    lv_obj_set_style_bg_color(scr_main, lv_color_hex(0x000000), 0);
    lv_obj_clear_flag(scr_main, LV_OBJ_FLAG_SCROLLABLE);

    // 1. Header (Battery & Stats)
    lv_obj_t * header = lv_obj_create(scr_main);
    lv_obj_set_size(header, 800, 140);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0x111111), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

    // SOC (Left)
    label_soc = lv_label_create(header);
    lv_label_set_text(label_soc, "0%");
    lv_obj_set_style_text_font(label_soc, &lv_font_montserrat_48, 0); // Need big font
    lv_obj_set_style_text_color(label_soc, lv_color_hex(0xFFFFFF), 0); // Explicit White
    lv_obj_align(label_soc, LV_ALIGN_LEFT_MID, 30, 0);

    // Stats Container (Right)
    lv_obj_t * stats = lv_obj_create(header);
    lv_obj_set_size(stats, 500, 120);
    lv_obj_align(stats, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_set_style_bg_opa(stats, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(stats, 0, 0);
    lv_obj_clear_flag(stats, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(stats, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(stats, LV_FLEX_ALIGN_SPACE_AROUND, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    label_in_power = create_stat_item(stats, "INPUT", "0 W");
    label_out_power = create_stat_item(stats, "OUTPUT", "0 W");
    // label_time_rem = create_stat_item(stats, "TIME", "00:00"); // Optional if space allows

    // 2. Body (Toggles)
    lv_obj_t * body = lv_obj_create(scr_main);
    lv_obj_set_size(body, 800, 240);
    lv_obj_align(body, LV_ALIGN_CENTER, 0, 20);
    lv_obj_set_style_bg_opa(body, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body, 0, 0);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(body, 40, 0);

    // AC Toggle (220V)
    btn_ac = create_big_toggle(body, "AC 220V", UI_ICON_FLASH, lv_color_hex(0xFF9800), &lbl_ac_pwr); // Orange for AC
    // DC Toggle (12V)
    btn_dc = create_big_toggle(body, "DC 12V", UI_ICON_CAR, lv_color_hex(0x03A9F4), &lbl_dc_pwr); // Blue for DC

    // 3. Footer (Wave 2 & Power Off)
    lv_obj_t * footer = lv_obj_create(scr_main);
    lv_obj_set_size(footer, 800, 80);
    lv_obj_align(footer, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_color(footer, lv_color_hex(0x111111), 0);
    lv_obj_set_style_border_width(footer, 0, 0);
    lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);

    // Wave 2 Button (Bottom Left 1)
    btn_wave2 = lv_btn_create(footer);
    lv_obj_set_size(btn_wave2, 160, 60);
    lv_obj_align(btn_wave2, LV_ALIGN_LEFT_MID, 20, 0);
    lv_obj_t * l_w2 = lv_label_create(btn_wave2);
    lv_label_set_text(l_w2, "Wave 2");
    lv_obj_set_style_text_color(l_w2, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(l_w2);

    // Power Off Button (Bottom Left 2, next to Wave 2)
    btn_power_off = lv_btn_create(footer);
    lv_obj_set_size(btn_power_off, 160, 60);
    lv_obj_align(btn_power_off, LV_ALIGN_LEFT_MID, 200, 0);
    lv_obj_set_style_bg_color(btn_power_off, lv_color_hex(0xF44336), 0); // Red
    lv_obj_t * l_po = lv_label_create(btn_power_off);
    lv_label_set_text(l_po, "Power Off");
    lv_obj_set_style_text_color(l_po, lv_color_hex(0xFFFFFF), 0);
    lv_obj_center(l_po);
    lv_obj_add_event_cb(btn_power_off, btn_power_off_cb, LV_EVENT_CLICKED, NULL);
}

// --- Updates ---

void UI_Render_Dashboard(DeviceStatus* dev) {
    if (!label_soc) return; // Not init

    char buf[32];
    int soc = 0, in_w = 0, out_w = 0;
    int out_ac_w = 0, out_dc_w = 0;
    bool ac_on = false, dc_on = false;

    // Map Data
    if (dev->id == DEV_TYPE_DELTA_PRO_3) {
        soc = (int)dev->data.d3p.batteryLevel;
        in_w = (int)dev->data.d3p.inputPower;
        out_w = (int)dev->data.d3p.outputPower;

        // Detailed Output
        out_ac_w = (int)(dev->data.d3p.acLvOutputPower + dev->data.d3p.acHvOutputPower);
        out_dc_w = (int)(dev->data.d3p.dc12vOutputPower + dev->data.d3p.usbaOutputPower + dev->data.d3p.usbcOutputPower); // Sum DC/USB

        ac_on = dev->data.d3p.acLvOutputPower > 0; // Rough heuristic
        dc_on = dev->data.d3p.dc12vOutputPower > 0;
    } else if (dev->id == DEV_TYPE_DELTA_3) {
        soc = (int)dev->data.d3.batteryLevel;
        in_w = (int)dev->data.d3.inputPower;
        out_w = (int)dev->data.d3.outputPower;

        out_ac_w = (int)dev->data.d3.acOutputPower;
        out_dc_w = (int)(dev->data.d3.dc12vOutputPower + dev->data.d3.usbaOutputPower + dev->data.d3.usbcOutputPower);

        // Switches mapping depends on struct fields, assuming existence:
        // ac_on = dev->data.d3.invSwitch;
        // dc_on = dev->data.d3.dcSwitch;
    }

    snprintf(buf, sizeof(buf), "%d%%", soc);
    lv_label_set_text(label_soc, buf);

    snprintf(buf, sizeof(buf), "%d W", in_w);
    lv_label_set_text(label_in_power, buf);

    snprintf(buf, sizeof(buf), "%d W", out_w);
    lv_label_set_text(label_out_power, buf);

    // Update Button Power Labels
    if (lbl_ac_pwr) {
        snprintf(buf, sizeof(buf), "%d W", out_ac_w);
        lv_label_set_text(lbl_ac_pwr, buf);
    }
    if (lbl_dc_pwr) {
        snprintf(buf, sizeof(buf), "%d W", out_dc_w);
        lv_label_set_text(lbl_dc_pwr, buf);
    }

    // Update Toggle States (Avoid loopback if user just pressed)
    // Only update if not pressed recently?
    // For now, straightforward update:
    // if (ac_on) lv_obj_add_state(btn_ac, LV_STATE_CHECKED);
    // else lv_obj_clear_state(btn_ac, LV_STATE_CHECKED);
    // Same for DC
}

bool UI_CheckTouch_Dashboard(uint16_t x, uint16_t y) {
    // Legacy touch handler - no longer needed with LVGL input driver working?
    // If LVGL handles touch via driver, we don't need this manual hit testing.
    return false;
}

// --- Event Handlers ---

static void btn_toggle_event_cb(lv_event_t * e) {
    // Send command to device
}

static void btn_power_off_cb(lv_event_t * e) {
    // Show Popup
    if (popup_power_off) return; // Already shown

    popup_power_off = lv_obj_create(lv_scr_act());
    lv_obj_set_size(popup_power_off, 800, 480);
    lv_obj_set_style_bg_color(popup_power_off, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(popup_power_off, LV_OPA_80, 0); // Semi-transparent
    lv_obj_center(popup_power_off);

    lv_obj_t * box = lv_obj_create(popup_power_off);
    lv_obj_set_size(box, 400, 240);
    lv_obj_center(box);
    lv_obj_set_style_bg_color(box, lv_color_hex(0x222222), 0);

    lv_obj_t * lbl = lv_label_create(box);
    lv_label_set_text(lbl, "Really power off?");
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_MID, 0, 40);

    lv_obj_t * btn_yes = lv_btn_create(box);
    lv_obj_set_size(btn_yes, 120, 60);
    lv_obj_align(btn_yes, LV_ALIGN_BOTTOM_LEFT, 20, -20);
    lv_obj_set_style_bg_color(btn_yes, lv_color_hex(0xF44336), 0);
    lv_obj_t * l_yes = lv_label_create(btn_yes);
    lv_label_set_text(l_yes, "YES");
    lv_obj_center(l_yes);
    lv_obj_add_event_cb(btn_yes, popup_cb, LV_EVENT_CLICKED, (void*)1);

    lv_obj_t * btn_no = lv_btn_create(box);
    lv_obj_set_size(btn_no, 120, 60);
    lv_obj_align(btn_no, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_obj_set_style_bg_color(btn_no, lv_color_hex(0x666666), 0);
    lv_obj_t * l_no = lv_label_create(btn_no);
    lv_label_set_text(l_no, "NO");
    lv_obj_center(l_no);
    lv_obj_add_event_cb(btn_no, popup_cb, LV_EVENT_CLICKED, (void*)0);
}

static void popup_cb(lv_event_t * e) {
    intptr_t choice = (intptr_t)lv_event_get_user_data(e);
    if (choice == 1) {
        // YES clicked - for now just close
    }
    // Close popup
    if (popup_power_off) {
        lv_obj_del(popup_power_off);
        popup_power_off = NULL;
    }
}
