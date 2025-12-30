#include "ui_icons.h"

const char * ui_get_icon_str(UIIconType type) {
    switch (type) {
        case UI_ICON_SOLAR: return MDI_SOLAR;
        case UI_ICON_GRID: return MDI_PLUG;
        case UI_ICON_CAR: return MDI_USB;
        case UI_ICON_USB: return MDI_USB;
        case UI_ICON_AC_OUT: return MDI_PLUG;
        case UI_ICON_BATTERY: return MDI_BATTERY_100;
        case UI_ICON_FLASH: return MDI_FLASH;
        case UI_ICON_SETTINGS: return MDI_COG;
        case UI_ICON_BACK: return MDI_CLOSE; // Close/Back
        case UI_ICON_BLE: return MDI_WAVE; // Signal
        default: return MDI_POWER;
    }
}

lv_obj_t * ui_create_icon(lv_obj_t * parent, UIIconType type, lv_coord_t size, lv_color_t color) {
    lv_obj_t * label = lv_label_create(parent);
    lv_obj_set_style_text_font(label, &font_mdi, 0);
    lv_obj_set_style_text_color(label, color, 0);
    lv_label_set_text(label, ui_get_icon_str(type));

    // Attempt to scale if size is very different?
    // For now rely on 32px base size.

    return label;
}
