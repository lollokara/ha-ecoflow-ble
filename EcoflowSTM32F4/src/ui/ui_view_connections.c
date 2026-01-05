#include "ui_view_connections.h"
#include "ui_view_debug.h"
#include "ui_core.h"
#include "ui_lvgl.h"
#include "uart_task.h"
#include <stdio.h>
#include "lvgl.h"

// Externs not needed if lvgl.h is included properly and fonts are enabled in lv_conf.h
// If needed, they should be const

static lv_obj_t * scr_connections = NULL;

typedef struct {
    lv_obj_t* panel;
    lv_obj_t* lbl_status;
    lv_obj_t* btn;
    lv_obj_t* lbl_btn;
    uint8_t type;
} DevPanel;

// Store panels for updates
// Index 0: Delta 3, 1: Delta Pro 3, 2: Wave 2, 3: Alt Charger
static DevPanel panels[4];

static void event_connections_cleanup(lv_event_t * e) {
    scr_connections = NULL;
    // Clear panel pointers
    for(int i=0; i<4; i++) {
        panels[i].panel = NULL;
        panels[i].lbl_status = NULL;
        panels[i].btn = NULL;
        panels[i].lbl_btn = NULL;
    }
}

static void event_back_to_debug(lv_event_t * e) {
    UI_CreateDebugView();
}

static void event_connect_device(lv_event_t * e) {
    uint8_t type = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    UART_SendConnectDevice(type);
}

static void event_forget_device(lv_event_t * e) {
    uint8_t type = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    UART_SendForgetDevice(type);
}

static void update_panel_state(DevPanel* p, bool connected, bool paired) {
    if (!p->lbl_status || !p->btn || !p->lbl_btn) return;

    // Remove old events first to avoid duplicates or wrong context if we were reusing buttons differently
    // But LVGL v8 replace callback is tricky.
    // Actually, we registered both events with different user_data? No, we need to swap behavior.
    // The easiest way is to remove all events and add the correct one, OR use a single handler that checks logic.
    // Given the previous code used distinct handlers, let's stick to swapping.
    // To be safe, we will just remove all events of type CLICKED.
    lv_obj_remove_event_cb(p->btn, event_forget_device);
    lv_obj_remove_event_cb(p->btn, event_connect_device);

    if (connected && paired) {
        lv_label_set_text(p->lbl_status, "Status: Connected");
        lv_obj_set_style_text_color(p->lbl_status, lv_palette_main(LV_PALETTE_GREEN), 0);

        lv_obj_set_style_bg_color(p->btn, lv_palette_main(LV_PALETTE_RED), 0);
        lv_label_set_text(p->lbl_btn, "Forget");
        lv_obj_add_event_cb(p->btn, event_forget_device, LV_EVENT_CLICKED, (void*)(uintptr_t)p->type);
    } else if (paired) {
        lv_label_set_text(p->lbl_status, "Status: Paired (Offline)");
        lv_obj_set_style_text_color(p->lbl_status, lv_palette_main(LV_PALETTE_ORANGE), 0);

        lv_obj_set_style_bg_color(p->btn, lv_palette_main(LV_PALETTE_RED), 0);
        lv_label_set_text(p->lbl_btn, "Forget");
        lv_obj_add_event_cb(p->btn, event_forget_device, LV_EVENT_CLICKED, (void*)(uintptr_t)p->type);
    } else {
        lv_label_set_text(p->lbl_status, "Status: Disconnected");
        lv_obj_set_style_text_color(p->lbl_status, lv_palette_main(LV_PALETTE_GREY), 0);

        lv_obj_set_style_bg_color(p->btn, lv_palette_main(LV_PALETTE_GREEN), 0);
        lv_label_set_text(p->lbl_btn, "Connect");
        lv_obj_add_event_cb(p->btn, event_connect_device, LV_EVENT_CLICKED, (void*)(uintptr_t)p->type);
    }
}

static void create_device_panel(lv_obj_t * parent, const char * name, uint8_t type, int index) {
    lv_obj_t * panel = lv_obj_create(parent);
    lv_obj_set_size(panel, 350, 150);
    lv_obj_set_style_bg_color(panel, lv_color_hex(0xFF282828), 0);
    lv_obj_set_style_border_width(panel, 0, 0);

    lv_obj_t * l_name = lv_label_create(panel);
    lv_label_set_text(l_name, name);
    lv_obj_set_style_text_font(l_name, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(l_name, lv_color_white(), 0);
    lv_obj_align(l_name, LV_ALIGN_TOP_LEFT, 0, 0);

    // Status
    lv_obj_t * l_status = lv_label_create(panel);
    lv_label_set_text(l_status, "Status: Disconnected");
    lv_obj_set_style_text_color(l_status, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_align(l_status, LV_ALIGN_LEFT_MID, 0, -10);

    // Button
    lv_obj_t * btn = lv_btn_create(panel);
    lv_obj_set_size(btn, 120, 40);
    lv_obj_align(btn, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    lv_obj_t * lbl_btn = lv_label_create(btn);
    lv_obj_center(lbl_btn);

    // Store in struct
    panels[index].panel = panel;
    panels[index].lbl_status = l_status;
    panels[index].btn = btn;
    panels[index].lbl_btn = lbl_btn;
    panels[index].type = type;
}

void UI_UpdateConnectionsView(DeviceList *list) {
    if (!scr_connections || !list) return;

    // Note: panels array is fixed: 0=D3, 1=D3P, 2=W2, 3=ALT
    // We update each panel based on list content

    int idx;

    // 0: Delta 3
    idx = -1;
    for(int i=0; i<list->count; i++) { if(list->devices[i].id == DEV_TYPE_DELTA_3) { idx = i; break; } }
    update_panel_state(&panels[0], idx >= 0 && list->devices[idx].connected, idx >= 0 && list->devices[idx].paired);

    // 1: Delta Pro 3
    idx = -1;
    for(int i=0; i<list->count; i++) { if(list->devices[i].id == DEV_TYPE_DELTA_PRO_3) { idx = i; break; } }
    update_panel_state(&panels[1], idx >= 0 && list->devices[idx].connected, idx >= 0 && list->devices[idx].paired);

    // 2: Wave 2
    idx = -1;
    for(int i=0; i<list->count; i++) { if(list->devices[i].id == DEV_TYPE_WAVE_2) { idx = i; break; } }
    update_panel_state(&panels[2], idx >= 0 && list->devices[idx].connected, idx >= 0 && list->devices[idx].paired);

    // 3: Alt Charger
    idx = -1;
    for(int i=0; i<list->count; i++) { if(list->devices[i].id == DEV_TYPE_ALT_CHARGER) { idx = i; break; } }
    update_panel_state(&panels[3], idx >= 0 && list->devices[idx].connected, idx >= 0 && list->devices[idx].paired);
}

void UI_CreateConnectionsView(void) {
    if (scr_connections) {
        lv_scr_load(scr_connections);
        return;
    }

    scr_connections = lv_obj_create(NULL);
    lv_obj_set_style_bg_color(scr_connections, lv_color_hex(0xFF121212), 0);
    lv_obj_set_style_text_color(scr_connections, lv_color_white(), 0);

    // Header
    lv_obj_t * header = lv_obj_create(scr_connections);
    lv_obj_set_size(header, lv_pct(100), 60);
    lv_obj_set_style_bg_color(header, lv_color_hex(0xFF202020), 0);
    lv_obj_set_style_border_width(header, 0, 0);
    lv_obj_align(header, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t * title = lv_label_create(header);
    lv_label_set_text(title, "Manage Connections");
    lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
    lv_obj_align(title, LV_ALIGN_CENTER, 0, 0);

    lv_obj_t * btn_back = lv_btn_create(header);
    lv_obj_set_size(btn_back, 100, 40);
    lv_obj_align(btn_back, LV_ALIGN_LEFT_MID, 10, 0);
    lv_obj_set_style_bg_color(btn_back, lv_palette_main(LV_PALETTE_GREY), 0);
    lv_obj_add_event_cb(btn_back, event_back_to_debug, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_back = lv_label_create(btn_back);
    lv_label_set_text(lbl_back, "Back");
    lv_obj_center(lbl_back);

    // Content Grid
    lv_obj_t * grid = lv_obj_create(scr_connections);
    lv_obj_set_size(grid, lv_pct(100), lv_pct(85));
    lv_obj_align(grid, LV_ALIGN_BOTTOM_MID, 0, 0);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    // Create Panels (populate static array)
    create_device_panel(grid, "Delta 3", DEV_TYPE_DELTA_3, 0);
    create_device_panel(grid, "Delta Pro 3", DEV_TYPE_DELTA_PRO_3, 1);
    create_device_panel(grid, "Wave 2", DEV_TYPE_WAVE_2, 2);
    create_device_panel(grid, "Alternator Chg", DEV_TYPE_ALT_CHARGER, 3);

    // Initial Update
    DeviceList list;
    UART_GetKnownDevices(&list);
    UI_UpdateConnectionsView(&list);

    lv_obj_add_event_cb(scr_connections, event_connections_cleanup, LV_EVENT_DELETE, NULL);
    lv_scr_load(scr_connections);
}
