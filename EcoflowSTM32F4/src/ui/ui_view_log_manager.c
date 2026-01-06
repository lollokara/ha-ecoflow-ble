#include "ui_view_log_manager.h"
#include "ui_view_debug.h"
#include "ui_core.h"
#include "ui_lvgl.h"
#include "log_manager.h"
#include "lvgl.h"
#include <stdio.h>

static lv_obj_t * scr_log_manager = NULL;
static lv_obj_t * label_space = NULL;

static void refresh_space(void) {
    if (!label_space) return;

    uint32_t total_kb = LogManager_GetTotalSpace();
    uint32_t free_kb = LogManager_GetFreeSpace();

    // Format to MB
    uint32_t total_mb = total_kb / 1024;
    uint32_t free_mb = free_kb / 1024;

    lv_label_set_text_fmt(label_space, "Space Available: %lu MB / %lu MB", free_mb, total_mb);
}

static void event_back(lv_event_t * e) {
    UI_CreateDebugView(); // Return to Debug Menu
}

static void event_delete_all(lv_event_t * e) {
    LogManager_HandleManagerOp(LOG_OP_DELETE_ALL);
    refresh_space();
}

static void event_format(lv_event_t * e) {
    LogManager_HandleManagerOp(LOG_OP_FORMAT_SD);
    refresh_space();
}

static void event_cleanup(lv_event_t * e) {
    scr_log_manager = NULL;
    label_space = NULL;
}

void UI_CreateLogManagerView(void) {
    if (scr_log_manager) {
        lv_scr_load(scr_log_manager);
        refresh_space();
        return;
    }

    scr_log_manager = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_log_manager, lv_color_hex(0xFF121212), 0);
    lv_obj_set_style_text_color(scr_log_manager, lv_color_white(), 0);

    // Title
    lv_obj_t * title = lv_label_create(scr_log_manager);
    lv_label_set_text(title, "Log Manager");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    // Space Label
    label_space = lv_label_create(scr_log_manager);
    lv_obj_set_style_text_font(label_space, &lv_font_montserrat_20, 0);
    lv_obj_align(label_space, LV_ALIGN_TOP_MID, 0, 80);
    refresh_space();

    // Delete All Button
    lv_obj_t * btn_del = lv_btn_create(scr_log_manager);
    lv_obj_set_size(btn_del, 200, 60);
    lv_obj_align(btn_del, LV_ALIGN_CENTER, -120, 0);
    lv_obj_set_style_bg_color(btn_del, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_add_event_cb(btn_del, event_delete_all, LV_EVENT_CLICKED, NULL);

    lv_obj_t * lbl_del = lv_label_create(btn_del);
    lv_label_set_text(lbl_del, "Delete All Logs");
    lv_obj_center(lbl_del);

    // Format Button
    lv_obj_t * btn_fmt = lv_btn_create(scr_log_manager);
    lv_obj_set_size(btn_fmt, 200, 60);
    lv_obj_align(btn_fmt, LV_ALIGN_CENTER, 120, 0);
    lv_obj_set_style_bg_color(btn_fmt, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_obj_add_event_cb(btn_fmt, event_format, LV_EVENT_CLICKED, NULL);

    lv_obj_t * lbl_fmt = lv_label_create(btn_fmt);
    lv_label_set_text(lbl_fmt, "Format SD Card");
    lv_obj_center(lbl_fmt);

    // Back Button
    lv_obj_t * btn_back = lv_btn_create(scr_log_manager);
    lv_obj_set_size(btn_back, 100, 50);
    lv_obj_align(btn_back, LV_ALIGN_TOP_LEFT, 20, 20);
    lv_obj_add_event_cb(btn_back, event_back, LV_EVENT_CLICKED, NULL);

    lv_obj_t * lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "Back");
    lv_obj_center(lbl_back);

    lv_obj_add_event_cb(scr_log_manager, event_cleanup, LV_EVENT_DELETE, NULL);
    lv_scr_load(scr_log_manager);
}
