#include "ui_lvgl.h"
#include "ui_icons.h"
#include "lvgl.h"
#include <stdio.h>
#include <math.h>

// --- Styles ---
static lv_style_t style_scr;
static lv_style_t style_panel;
static lv_style_t style_bar_bg;
static lv_style_t style_bar_indic;
static lv_style_t style_title;
static lv_style_t style_text_large;
static lv_style_t style_text_small;
static lv_style_t style_btn;
static lv_style_t style_icon_active;
static lv_style_t style_icon_inactive;

// --- Dashboard Widgets ---
static lv_obj_t * scr_dash;
static lv_obj_t * label_title;
static lv_obj_t * label_temp;
static lv_obj_t * bar_batt;
static lv_obj_t * label_soc;
static lv_obj_t * btn_settings;

// Flow Widgets
static lv_obj_t * cont_flow;
static lv_obj_t * icon_solar;
static lv_obj_t * label_solar_pwr;
static lv_obj_t * icon_grid;
static lv_obj_t * label_grid_pwr;
static lv_obj_t * icon_alt;
static lv_obj_t * label_alt_pwr;

static lv_obj_t * icon_usb;
static lv_obj_t * label_usb_pwr;
static lv_obj_t * icon_12v;
static lv_obj_t * label_12v_pwr;
static lv_obj_t * icon_ac_out;
static lv_obj_t * label_ac_out_pwr;

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

    // Battery Bar
    lv_style_init(&style_bar_bg);
    lv_style_set_bg_color(&style_bar_bg, lv_palette_lighten(LV_PALETTE_GREY, 3));
    lv_style_set_border_width(&style_bar_bg, 1);
    lv_style_set_border_color(&style_bar_bg, lv_palette_main(LV_PALETTE_GREY));

    lv_style_init(&style_bar_indic);
    lv_style_set_bg_opa(&style_bar_indic, LV_OPA_COVER);
    lv_style_set_bg_color(&style_bar_indic, lv_palette_main(LV_PALETTE_GREEN));
    lv_style_set_bg_grad_color(&style_bar_indic, lv_palette_main(LV_PALETTE_RED));
    lv_style_set_bg_grad_dir(&style_bar_indic, LV_GRAD_DIR_HOR);

    lv_style_init(&style_title);
    lv_style_set_text_font(&style_title, &lv_font_montserrat_20);

    lv_style_init(&style_text_large);
    lv_style_set_text_font(&style_text_large, &lv_font_montserrat_24);

    lv_style_init(&style_text_small);
    lv_style_set_text_font(&style_text_small, &lv_font_montserrat_14);
    lv_style_set_text_color(&style_text_small, lv_palette_main(LV_PALETTE_GREY));

    lv_style_init(&style_icon_active);
    lv_style_set_bg_opa(&style_icon_active, LV_OPA_COVER);
    lv_style_set_bg_color(&style_icon_active, lv_palette_lighten(LV_PALETTE_GREEN, 4)); // Light Green BG
    lv_style_set_text_color(&style_icon_active, lv_palette_darken(LV_PALETTE_GREEN, 2)); // Dark Green Icon
    lv_style_set_radius(&style_icon_active, LV_RADIUS_CIRCLE);

    lv_style_init(&style_icon_inactive);
    lv_style_set_bg_opa(&style_icon_inactive, LV_OPA_COVER);
    lv_style_set_bg_color(&style_icon_inactive, lv_palette_lighten(LV_PALETTE_GREY, 4));
    lv_style_set_text_color(&style_icon_inactive, lv_palette_main(LV_PALETTE_GREY));
    lv_style_set_radius(&style_icon_inactive, LV_RADIUS_CIRCLE);
}

// Event Handlers
static void event_settings_btn(lv_event_t * e) {
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
        lv_scr_load_anim(scr_settings, LV_SCR_LOAD_ANIM_MOVE_LEFT, 300, 0, false);
    }
}

static void event_back_btn(lv_event_t * e) {
    if(lv_event_get_code(e) == LV_EVENT_CLICKED) {
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

// Battery Bar Draw Event for Markers
static void event_bar_draw(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    if(code == LV_EVENT_DRAW_PART_END) {
        lv_obj_draw_part_dsc_t * dsc = lv_event_get_draw_part_dsc(e);
        if(dsc->part == LV_PART_INDICATOR) {
            lv_obj_t * obj = lv_event_get_target(e);
            lv_draw_ctx_t * draw_ctx = dsc->draw_ctx;
            const lv_area_t * coords = &obj->coords;

            // Assume fixed limits for demo, or fetch from user_data
            int min_soc = 10; // Red
            int max_soc = 95; // Blue

            lv_coord_t w = lv_area_get_width(coords);
            // lv_coord_t h = lv_area_get_height(coords); // Unused

            lv_draw_line_dsc_t line_dsc;
            lv_draw_line_dsc_init(&line_dsc);
            line_dsc.width = 3;

            // Red Line (Min Discharge)
            line_dsc.color = lv_palette_main(LV_PALETTE_RED);
            lv_coord_t x_min = coords->x1 + (w * min_soc) / 100;
            lv_point_t p1 = {x_min, coords->y1 - 5};
            lv_point_t p2 = {x_min, coords->y2 + 5};
            lv_draw_line(draw_ctx, &line_dsc, &p1, &p2);

            // Blue Line (Max Charge)
            line_dsc.color = lv_palette_main(LV_PALETTE_BLUE);
            lv_coord_t x_max = coords->x1 + (w * max_soc) / 100;
            lv_point_t p3 = {x_max, coords->y1 - 5};
            lv_point_t p4 = {x_max, coords->y2 + 5};
            lv_draw_line(draw_ctx, &line_dsc, &p3, &p4);
        }
    }
}

static lv_obj_t * create_flow_item(lv_obj_t * parent, ui_icon_type_t icon_type, const char* name, lv_coord_t x, lv_coord_t y, lv_obj_t ** label_p) {
    // Container for icon + labels
    lv_obj_t * cont = lv_obj_create(parent);
    lv_obj_set_size(cont, 100, 120);
    lv_obj_set_pos(cont, x, y);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    // Icon (Custom Canvas)
    lv_obj_t * icon = ui_create_icon_canvas(cont, icon_type, 60, lv_palette_main(LV_PALETTE_GREY));
    lv_obj_align(icon, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_add_style(icon, &style_icon_inactive, 0); // Default inactive

    // Power Label
    *label_p = lv_label_create(cont);
    lv_label_set_text(*label_p, "0W");
    lv_obj_add_style(*label_p, &style_text_small, 0);
    // lv_obj_set_style_text_weight(*label_p, LV_FONT_WEIGHT_BOLD, 0); // Not supported in standard fonts
    lv_obj_align(*label_p, LV_ALIGN_BOTTOM_MID, 0, -20);

    // Name Label
    lv_obj_t * lbl_name = lv_label_create(cont);
    lv_label_set_text(lbl_name, name);
    lv_obj_add_style(lbl_name, &style_text_small, 0);
    lv_obj_align(lbl_name, LV_ALIGN_BOTTOM_MID, 0, 0);

    return icon; // Return icon obj to update style
}

static void update_flow_item(lv_obj_t* icon, lv_obj_t* lbl, int pwr) {
    lv_label_set_text_fmt(lbl, "%dW", pwr);
    if (pwr > 0) {
        lv_obj_add_style(icon, &style_icon_active, 0);
        lv_obj_remove_style(icon, &style_icon_inactive, 0);
    } else {
        lv_obj_add_style(icon, &style_icon_inactive, 0);
        lv_obj_remove_style(icon, &style_icon_active, 0);
    }
}

static void create_dashboard(void) {
    scr_dash = lv_obj_create(NULL);
    lv_obj_add_style(scr_dash, &style_scr, 0);

    // Header
    lv_obj_t * header = lv_obj_create(scr_dash);
    lv_obj_set_size(header, 800, 50);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);
    lv_obj_set_style_bg_color(header, lv_color_hex(0xFF202020), 0);
    lv_obj_set_style_border_width(header, 0, 0);

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
    lv_obj_add_style(bar_batt, &style_bar_bg, LV_PART_MAIN);
    lv_obj_add_style(bar_batt, &style_bar_indic, LV_PART_INDICATOR);
    lv_bar_set_value(bar_batt, 50, LV_ANIM_OFF);
    lv_bar_set_range(bar_batt, 0, 100);
    lv_obj_add_event_cb(bar_batt, event_bar_draw, LV_EVENT_DRAW_PART_END, NULL);

    label_soc = lv_label_create(scr_dash);
    lv_label_set_text(label_soc, "50%");
    lv_obj_add_style(label_soc, &style_text_large, 0);
    lv_obj_align_to(label_soc, bar_batt, LV_ALIGN_CENTER, 0, 0);

    // Flow Container
    cont_flow = lv_obj_create(scr_dash);
    lv_obj_set_size(cont_flow, 800, 250);
    lv_obj_align(cont_flow, LV_ALIGN_TOP_MID, 0, 130);
    lv_obj_set_style_border_width(cont_flow, 0, 0);
    lv_obj_set_style_bg_opa(cont_flow, LV_OPA_TRANSP, 0);

    // Center Battery
    lv_obj_t * icon_bat = ui_create_icon_canvas(cont_flow, UI_ICON_BATTERY, 100, lv_palette_main(LV_PALETTE_TEAL));
    lv_obj_align(icon_bat, LV_ALIGN_CENTER, 0, 0);

    // Left Column (Inputs)
    icon_solar = create_flow_item(cont_flow, UI_ICON_SOLAR, "Solar", 50, 20, &label_solar_pwr);
    icon_grid = create_flow_item(cont_flow, UI_ICON_GRID, "Grid", 50, 120, &label_grid_pwr);
    icon_alt = create_flow_item(cont_flow, UI_ICON_CAR, "Alt", 200, 120, &label_alt_pwr); // Offset? Spec says Left. Let's stack 3 on left.
    // Layout adjust: Solar Top-Left, Grid Mid-Left, Alt Bottom-Left?
    // 250px height. Items are 120px tall. Too tight for 3 vertical.
    // Let's do grid 2x3?
    // Left side:
    lv_obj_set_pos(lv_obj_get_parent(icon_solar), 40, 10);
    lv_obj_set_pos(lv_obj_get_parent(icon_grid), 40, 120);
    lv_obj_set_pos(lv_obj_get_parent(icon_alt), 160, 65); // Between

    // Right Column (Outputs)
    icon_usb = create_flow_item(cont_flow, UI_ICON_USB, "USB", 660, 10, &label_usb_pwr);
    icon_12v = create_flow_item(cont_flow, UI_ICON_12V, "12V", 660, 120, &label_12v_pwr);
    icon_ac_out = create_flow_item(cont_flow, UI_ICON_AC_OUT, "AC Out", 540, 65, &label_ac_out_pwr);

    // Footer / Settings
    btn_settings = lv_btn_create(scr_dash);
    lv_obj_align(btn_settings, LV_ALIGN_BOTTOM_RIGHT, -20, -20);
    lv_obj_t * lbl = lv_label_create(btn_settings);
    lv_label_set_text(lbl, LV_SYMBOL_SETTINGS " Settings");
    lv_obj_add_event_cb(btn_settings, event_settings_btn, LV_EVENT_CLICKED, NULL);
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

    btn_save = lv_btn_create(header);
    lv_obj_align(btn_save, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_t * lbls = lv_label_create(btn_save);
    lv_label_set_text(lbls, "Save");
    lv_obj_add_event_cb(btn_save, event_back_btn, LV_EVENT_CLICKED, NULL); // Placeholder action

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


    // 3. AC Input (400-3000W)
    lv_obj_t * row3 = lv_obj_create(cont);
    lv_obj_set_size(row3, 600, 80);
    lv_obj_set_style_border_width(row3, 0, 0);

    lv_obj_t * l3 = lv_label_create(row3);
    lv_label_set_text(l3, "AC Input Limit");
    lv_obj_align(l3, LV_ALIGN_TOP_LEFT, 0, 0);

    slider_ac_in = lv_slider_create(row3);
    lv_slider_set_range(slider_ac_in, 400, 3000); // 400W start per constraint
    lv_slider_set_value(slider_ac_in, 2000, LV_ANIM_OFF);
    lv_obj_set_width(slider_ac_in, 400);
    lv_obj_align(slider_ac_in, LV_ALIGN_BOTTOM_LEFT, 0, -10);

    label_ac_in_val = lv_label_create(row3);
    lv_label_set_text(label_ac_in_val, "2000W");
    lv_obj_align(label_ac_in_val, LV_ALIGN_BOTTOM_RIGHT, -20, -10);
    lv_obj_add_event_cb(slider_ac_in, event_slider_cb, LV_EVENT_VALUE_CHANGED, label_ac_in_val);
}

void UI_LVGL_Init(void) {
    lv_init();

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
    int in_solar=0, in_ac=0, in_alt=0;
    int out_usb=0, out_12v=0, out_ac=0;
    float temp = 25.0f;

    if (dev->id == DEV_TYPE_DELTA_PRO_3) {
        soc = (int)dev->data.d3p.batteryLevel;
        in_ac = (int)dev->data.d3p.acInputPower;
        in_solar = (int)(dev->data.d3p.solarLvPower + dev->data.d3p.solarHvPower);
        // Alt?? D3P has DC input?
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

    // Update Battery
    lv_bar_set_value(bar_batt, soc, LV_ANIM_ON);
    lv_label_set_text_fmt(label_soc, "%d%%", soc);
    lv_label_set_text_fmt(label_temp, "Temp: %.1f C", temp);

    // Update Flow
    update_flow_item(icon_solar, label_solar_pwr, in_solar);
    update_flow_item(icon_grid, label_grid_pwr, in_ac);
    update_flow_item(icon_alt, label_alt_pwr, in_alt);
    update_flow_item(icon_usb, label_usb_pwr, out_usb);
    update_flow_item(icon_12v, label_12v_pwr, out_12v);
    update_flow_item(icon_ac_out, label_ac_out_pwr, out_ac);
}
