#ifndef UI_ICONS_H
#define UI_ICONS_H

#include "lvgl.h"
#include "font_mdi.h"
#include "ui_core.h" // Use shared UIIconType

// Map UI_ICON types to MDI font strings
#define UI_ICON_SOLAR_STR MDI_SOLAR
#define UI_ICON_GRID_STR MDI_PLUG
#define UI_ICON_CAR_STR MDI_USB
#define UI_ICON_USB_STR MDI_USB
#define UI_ICON_AC_OUT_STR MDI_PLUG
#define UI_ICON_BATTERY_STR MDI_BATTERY_100
#define UI_ICON_FLASH_STR MDI_FLASH

// Backwards compatibility alias if needed, or just use UIIconType
typedef UIIconType ui_icon_type_t;

/**
 * @brief Creates an icon object using the MDI font
 * @param parent Parent object
 * @param type Icon type enum
 * @param size Font size (height)
 * @param color Text color
 * @return lv_obj_t* Pointer to the label object
 */
lv_obj_t * ui_create_icon(lv_obj_t * parent, UIIconType type, lv_coord_t size, lv_color_t color);

/**
 * @brief Helper to get the MDI string for a type
 */
const char * ui_get_icon_str(UIIconType type);

#endif
