#include "ui_view_log_manager.h"
#include "ui_view_debug.h"
#include "log_manager.h"
#include "lvgl.h"
#include <stdio.h>

static lv_obj_t * scr_log = NULL;
static lv_obj_t * label_status = NULL;
static lv_obj_t * label_space = NULL;
static lv_obj_t * sw_logging = NULL;
static lv_timer_t * log_timer = NULL;

static void refresh_sd_info(lv_timer_t * timer) {
    if (!scr_log) return;

    uint32_t total = 0, free = 0;
    LogManager_GetSpace(&total, &free);

    if (total == 0) {
        lv_label_set_text(label_status, "SD Status: Not Mounted / Error");
        lv_label_set_text(label_space, "Space: -- / --");
    } else {
        lv_label_set_text(label_status, "SD Status: Mounted");
        lv_label_set_text_fmt(label_space, "Space: %lu MB / %lu MB", free, total);
    }
}

static void event_cleanup(lv_event_t * e) {
    if (log_timer) {
        lv_timer_del(log_timer);
        log_timer = NULL;
    }
    scr_log = NULL;
}

static void event_close(lv_event_t * e) {
    UI_CreateDebugView();
}

static void event_toggle_logging(lv_event_t * e) {
    bool state = lv_obj_has_state(sw_logging, LV_STATE_CHECKED);
    LogManager_SetLogging(state);
}

static void event_format(lv_event_t * e) {
    // Show confirmation? For now just do it.
    LogManager_HandleFormat();
    refresh_sd_info(NULL);
}

static void event_delete_all(lv_event_t * e) {
    LogManager_HandleDeleteAll();
    refresh_sd_info(NULL);
}

void UI_CreateLogManagerView(void) {
    if (scr_log) {
        lv_scr_load(scr_log);
        return;
    }

    scr_log = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_log, lv_color_hex(0xFF121212), 0);
    lv_obj_set_style_text_color(scr_log, lv_color_white(), 0);

    // Header
    lv_obj_t * header = lv_obj_create(scr_log);
    lv_obj_set_size(header, lv_pct(100), 60);
    lv_obj_set_style_bg_color(header, lv_color_hex(0xFF202020), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t * title = lv_label_create(header);
    lv_label_set_text(title, "Log Manager");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_24, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    // Close Button
    lv_obj_t * btn_close = lv_btn_create(header);
    lv_obj_set_size(btn_close, 40, 40);
    lv_obj_align(btn_close, LV_ALIGN_RIGHT_MID, -10, 0);
    lv_obj_set_style_bg_color(btn_close, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_add_event_cb(btn_close, event_close, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_x = lv_label_create(btn_close);
    lv_label_set_text(lbl_x, "X");
    lv_obj_center(lbl_x);

    // Content
    lv_obj_t * cont = lv_obj_create(scr_log);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(85));
    lv_obj_align(cont, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Logging Toggle
    lv_obj_t * cont_sw = lv_obj_create(cont);
    lv_obj_set_size(cont_sw, lv_pct(90), 60);
    lv_obj_set_style_bg_opa(cont_sw, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont_sw, 0, 0);

    lv_obj_t * lbl_sw = lv_label_create(cont_sw);
    lv_label_set_text(lbl_sw, "Logging Enabled");
    lv_obj_align(lbl_sw, LV_ALIGN_LEFT_MID, 10, 0);

    sw_logging = lv_switch_create(cont_sw);
    lv_obj_align(sw_logging, LV_ALIGN_RIGHT_MID, -10, 0);
    if (LogManager_IsLogging()) lv_obj_add_state(sw_logging, LV_STATE_CHECKED);
    lv_obj_add_event_cb(sw_logging, event_toggle_logging, LV_EVENT_VALUE_CHANGED, NULL);

    // Status Info
    label_status = lv_label_create(cont);
    lv_label_set_text(label_status, "SD Status: Checking...");
    lv_obj_set_style_pad_top(label_status, 20, 0);

    label_space = lv_label_create(cont);
    lv_label_set_text(label_space, "Space: -- / --");
    lv_obj_set_style_pad_bottom(label_space, 20, 0);

    // Buttons
    lv_obj_t * btn_format = lv_btn_create(cont);
    lv_obj_set_size(btn_format, 200, 50);
    lv_obj_set_style_bg_color(btn_format, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_obj_add_event_cb(btn_format, event_format, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_fmt = lv_label_create(btn_format);
    lv_label_set_text(lbl_fmt, "Format SD Card");
    lv_obj_center(lbl_fmt);

    lv_obj_t * btn_del = lv_btn_create(cont);
    lv_obj_set_size(btn_del, 200, 50);
    lv_obj_set_style_bg_color(btn_del, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_add_event_cb(btn_del, event_delete_all, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_del = lv_label_create(btn_del);
    lv_label_set_text(lbl_del, "Delete All Logs");
    lv_obj_center(lbl_del);

    lv_obj_add_event_cb(scr_log, event_cleanup, LV_EVENT_DELETE, NULL);

    refresh_sd_info(NULL);
    log_timer = lv_timer_create(refresh_sd_info, 2000, NULL);

    lv_scr_load(scr_log);
}
