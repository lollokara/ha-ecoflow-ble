#ifndef UI_ICONS_H
#define UI_ICONS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "lvgl.h"

// Icon Types
typedef enum {
    UI_ICON_SOLAR,
    UI_ICON_GRID,
    UI_ICON_CAR,
    UI_ICON_USB,
    UI_ICON_12V,
    UI_ICON_AC_OUT,
    UI_ICON_BATTERY,
    UI_ICON_SETTINGS
} ui_icon_type_t;

// Draw an icon into a canvas or object event context
// If canvas is NULL, assumes drawing in an event callback (LV_EVENT_DRAW_MAIN)
void ui_draw_icon(lv_obj_t * obj, lv_draw_ctx_t * draw_ctx, ui_icon_type_t type, const lv_area_t * area, lv_color_t color);

// Helper to create a canvas with the icon drawn (static allocation)
lv_obj_t * ui_create_icon_canvas(lv_obj_t * parent, ui_icon_type_t type, lv_coord_t size, lv_color_t color);

#ifdef __cplusplus
}
#endif

#endif /* UI_ICONS_H */
