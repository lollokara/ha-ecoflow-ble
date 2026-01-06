#include "ui_log_manager.h"
#include "ui_core.h"
#include "ui_lvgl.h"
#include "log_manager.h"
#include "ui_view_debug.h" // To return to debug view
#include <stdio.h>

static lv_obj_t * scr_log_mgr = NULL;
static lv_obj_t * label_status = NULL;
static lv_obj_t * label_space = NULL;

static void update_space_info(void) {
    if (!label_space) return;

    uint32_t total = 0, free = 0;
    if (LogManager_GetFreeSpace(&total, &free)) {
        lv_label_set_text_fmt(label_space, "Total: %lu MB\nFree: %lu MB", total, free);
    } else {
        lv_label_set_text(label_space, "SD Card Error or Not Mounted");
    }
}

static void event_close(lv_event_t * e) {
    UI_CreateDebugView(); // Return to Debug View
}

static void event_delete_all(lv_event_t * e) {
    if (label_status) lv_label_set_text(label_status, "Deleting...");
    LogManager_DeleteAllLogs();
    if (label_status) lv_label_set_text(label_status, "All logs deleted.");
    update_space_info();
}

static void event_format(lv_event_t * e) {
    if (label_status) lv_label_set_text(label_status, "Formatting... (Please Wait)");
    // Formatting can take time, ideally should be async or show spinner
    // For now blocking is okay as it's a maintenance task
    if (LogManager_FormatSD()) {
        if (label_status) lv_label_set_text(label_status, "Format Success.");
    } else {
        if (label_status) lv_label_set_text(label_status, "Format Failed.");
    }
    update_space_info();
}

static void event_cleanup(lv_event_t * e) {
    scr_log_mgr = NULL;
    label_status = NULL;
    label_space = NULL;
}

void UI_CreateLogManagerView(void) {
    if (scr_log_mgr) {
        lv_scr_load(scr_log_mgr);
        update_space_info();
        return;
    }

    scr_log_mgr = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_log_mgr, lv_color_hex(0xFF121212), 0);
    lv_obj_set_style_text_color(scr_log_mgr, lv_color_white(), 0);

    // Header
    lv_obj_t * header = lv_obj_create(scr_log_mgr);
    lv_obj_set_size(header, lv_pct(100), 60);
    lv_obj_set_style_bg_color(header, lv_color_hex(0xFF202020), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t * title = lv_label_create(header);
    lv_label_set_text(title, "Log Manager");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
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
    lv_obj_t * cont = lv_obj_create(scr_log_mgr);
    lv_obj_set_size(cont, lv_pct(100), lv_pct(85));
    lv_obj_align(cont, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(cont, 20, 0);
    lv_obj_set_style_pad_row(cont, 20, 0);

    // Space Info
    label_space = lv_label_create(cont);
    lv_label_set_text(label_space, "Checking space...");
    lv_obj_set_style_text_font(label_space, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_align(label_space, LV_TEXT_ALIGN_CENTER, 0);

    // Buttons
    lv_obj_t * btn_del = lv_btn_create(cont);
    lv_obj_set_size(btn_del, 250, 60);
    lv_obj_set_style_bg_color(btn_del, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_obj_add_event_cb(btn_del, event_delete_all, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_del = lv_label_create(btn_del);
    lv_label_set_text(lbl_del, "Delete All Logs");
    lv_obj_center(lbl_del);

    lv_obj_t * btn_fmt = lv_btn_create(cont);
    lv_obj_set_size(btn_fmt, 250, 60);
    lv_obj_set_style_bg_color(btn_fmt, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_add_event_cb(btn_fmt, event_format, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_fmt = lv_label_create(btn_fmt);
    lv_label_set_text(lbl_fmt, "Format SD Card");
    lv_obj_center(lbl_fmt);

    // Status
    label_status = lv_label_create(cont);
    lv_label_set_text(label_status, "Ready");
    lv_obj_set_style_text_color(label_status, lv_palette_main(LV_PALETTE_GREY), 0);

    lv_obj_add_event_cb(scr_log_mgr, event_cleanup, LV_EVENT_DELETE, NULL);

    lv_scr_load(scr_log_mgr);

    update_space_info();
}
