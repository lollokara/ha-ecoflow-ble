#ifndef UI_ICONS_H
#define UI_ICONS_H

#include "lvgl.h"

// Font Declaration
extern const lv_font_t ui_font_mdi;

// MDI Icon Macros (UTF-8 Encoded via Universal Character Name)
// Current mapping in ui_font_mdi.c (approximate hex values):
// 985714 -> F0A72 (Solar)
// 984741 -> F06A5 (Plug)
// 983308 -> F010C (Car)
// 990399 -> F1CBF (USB)
// 984211 -> F0493 (Settings)
// 983117 -> F004D (Back)
// 983161 -> F0079 (Battery)
// 987399 -> F1107 (AC)

#define MDI_ICON_SOLAR      "\U000F0A72" // 󰩲
#define MDI_ICON_PLUG       "\U000F06A5" // 󰚥
#define MDI_ICON_CAR        "\U000F010C" // 󰄌
#define MDI_ICON_USB        "\U000F1CBF" // 󱲿
#define MDI_ICON_SETTINGS   "\U000F0493" // 󰒓
#define MDI_ICON_BACK       "\U000F004D" // 󰁍
#define MDI_ICON_BATTERY    "\U000F0079" // 󰁹
#define MDI_ICON_AC         "\U000F1107" // 󱄇

// New Icons for Wave 2
// Note: These will only render if the font is regenerated with these glyphs.
#define MDI_ICON_FAN        "\U000F0210" // 󰈐
#define MDI_ICON_THERMOMETER "\U000F050F" // 󰔏
#define MDI_ICON_FIRE       "\U000F18D6" // 󱣖 (New Hot Icon)
#define MDI_ICON_SNOWFLAKE  "\U000F1A71" // 󱩱 (New Cold Icon)
#define MDI_ICON_LEAF       "\U000F030D" // 󰌍
#define MDI_ICON_MOON       "\U000F0594" // 󰖔
#define MDI_ICON_POWER      "\U000F0425" // 󰐥

// Helper to set label to icon
static inline void ui_set_icon(lv_obj_t * label, const char * icon_code) {
    lv_obj_set_style_text_font(label, &ui_font_mdi, 0);
    lv_label_set_text(label, icon_code);
}

#endif // UI_ICONS_H
