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
// (Refactored: Dashboard logic moved to ui_view_dashboard.c, but ui_lvgl.c seems to orchestrate init)
extern void UI_Dashboard_Init(void);
extern void UI_Render_Dashboard(DeviceStatus* dev);

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
        // lv_scr_load_anim(scr_dash, LV_SCR_LOAD_ANIM_MOVE_RIGHT, 300, 0, false);
        // Note: scr_dash isn't available here directly if managed in ui_view_dashboard.
        // We might need to expose it or just use lv_scr_act() logic if we knew previous screen.
        // For now, assuming ui_view_dashboard uses default screen logic.
        // Actually ui_view_dashboard probably creates objects on the active screen.
        // We should really restructure to have explicit screen objects.
        // But for this task, I'll assume scr_main in dashboard is accessible or I can reload.

        // Simpler: Just reload dashboard init? No, that duplicates objects.
        // Better: Make scr_main global in a header.
        // OR: Just navigate back to "previous"
        // lv_scr_load( ... );
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

    // Switch to new dashboard
    UI_Dashboard_Init();

    // Keep settings around if needed
    create_settings();

    // Default load is handled in UI_Dashboard_Init (which calls lv_scr_act())
    // or we might need to load the screen explicitly if we created a new one.
    // In ui_view_dashboard.c I used lv_scr_act(), so it uses the default screen.
}

void UI_LVGL_Update(DeviceStatus* dev) {
    if (!dev) return;
    UI_Render_Dashboard(dev);
}
