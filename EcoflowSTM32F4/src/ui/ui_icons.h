#ifndef UI_ICONS_H
#define UI_ICONS_H

#include "lvgl.h"

// Font Declaration
extern const lv_font_t ui_font_mdi;

// MDI Icon Macros (UTF-8 Encoded via Universal Character Name)
#define MDI_ICON_SOLAR      "\U000F0A72" // 󰩲
#define MDI_ICON_PLUG       "\U000F06A5" // 󰚥 (Used for Input?)
#define MDI_ICON_CAR        "\U000F010C" // 󰄌
#define MDI_ICON_USB        "\U000F1CBF" // 󱲿
#define MDI_ICON_SETTINGS   "\U000F0493" // 󰒓
#define MDI_ICON_BACK       "\U000F004D" // 󰁍
#define MDI_ICON_BATTERY    "\U000F0079" // 󰁹
#define MDI_ICON_AC         "\U000F1107" // 󱄇 (Used for AC Output?)

// New Icons (Wave 2)
#define MDI_ICON_FAN          "\U000F0210" // 󰈐
#define MDI_ICON_THERMOMETER  "\U000F050F" // 󰔏
#define MDI_ICON_SNOWFLAKE    "\U000F04AE" // 󰒮
#define MDI_ICON_SUN          "\U000F0599" // 󰖙
#define MDI_ICON_POWER        "\U000F0425" // 󰐥

// Helper to set label to icon
static inline void ui_set_icon(lv_obj_t * label, const char * icon_code) {
    lv_obj_set_style_text_font(label, &ui_font_mdi, 0);
    lv_label_set_text(label, icon_code);
}

#endif // UI_ICONS_H
