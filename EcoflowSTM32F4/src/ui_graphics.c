#include "ui_graphics.h"
#include <stdio.h>
#include "font_mdi.h"
// #include "ui_vector.h" // Removed vector engine usage
// #include "mdi_icons.h" // Removed vector path usage

// Map types to bitmap arrays
static const uint8_t* GetIconBitmap(UI_IconType type) {
    switch (type) {
        case ICON_SOLAR: return icon_solar_48x48;
        case ICON_AC_PLUG: return icon_ac_plug_48x48;
        case ICON_CAR_BATTERY: return icon_car_battery_48x48;
        case ICON_BATTERY: return icon_battery_48x48;
        case ICON_USB: return icon_usb_48x48;
        case ICON_AC_SOCKET: return icon_ac_socket_48x48;
        case ICON_SETTINGS: return icon_settings_48x48;
        default: return NULL;
    }
}

/**
 * @brief  Draws a rounded rectangle outline.
 */
void UI_DrawRoundedRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t r, uint32_t color) {
    BSP_LCD_SetTextColor(color);

    BSP_LCD_DrawHLine(x + r, y, w - 2 * r);
    BSP_LCD_DrawHLine(x + r, y + h - 1, w - 2 * r);
    BSP_LCD_DrawVLine(x, y + r, h - 2 * r);
    BSP_LCD_DrawVLine(x + w - 1, y + r, h - 2 * r);

    int32_t f = 1 - r;
    int32_t ddF_x = 1;
    int32_t ddF_y = -2 * r;
    int32_t cx = 0;
    int32_t cy = r;

    while (cx < cy) {
        if (f >= 0) {
            cy--;
            ddF_y += 2;
            f += ddF_y;
        }
        cx++;
        ddF_x += 2;
        f += ddF_x;

        BSP_LCD_DrawPixel(x + r - cx, y + r - cy, color);
        BSP_LCD_DrawPixel(x + r - cy, y + r - cx, color);
        BSP_LCD_DrawPixel(x + w - r + cx - 1, y + r - cy, color);
        BSP_LCD_DrawPixel(x + w - r + cy - 1, y + r - cx, color);
        BSP_LCD_DrawPixel(x + w - r + cx - 1, y + h - r + cy - 1, color);
        BSP_LCD_DrawPixel(x + w - r + cy - 1, y + h - r + cx - 1, color);
        BSP_LCD_DrawPixel(x + r - cx, y + h - r + cy - 1, color);
        BSP_LCD_DrawPixel(x + r - cy, y + h - r + cx - 1, color);
    }
}

/**
 * @brief  Fills a rounded rectangle.
 */
void UI_FillRoundedRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t r, uint32_t color) {
    BSP_LCD_SetTextColor(color);

    BSP_LCD_FillRect(x + r, y, w - 2 * r, h);
    BSP_LCD_FillRect(x, y + r, r, h - 2 * r);
    BSP_LCD_FillRect(x + w - r, y + r, r, h - 2 * r);

    int32_t f = 1 - r;
    int32_t ddF_x = 1;
    int32_t ddF_y = -2 * r;
    int32_t cx = 0;
    int32_t cy = r;

    while (cx < cy) {
        if (f >= 0) {
            cy--;
            ddF_y += 2;
            f += ddF_y;
        }
        cx++;
        ddF_x += 2;
        f += ddF_x;

        BSP_LCD_DrawHLine(x + r - cx, y + r - cy, cx);
        BSP_LCD_DrawHLine(x + r - cy, y + r - cx, cy);
        BSP_LCD_DrawHLine(x + w - r, y + r - cy, cx);
        BSP_LCD_DrawHLine(x + w - r, y + r - cx, cy);
        BSP_LCD_DrawHLine(x + r - cx, y + h - r + cy - 1, cx);
        BSP_LCD_DrawHLine(x + r - cy, y + h - r + cx - 1, cy);
        BSP_LCD_DrawHLine(x + w - r, y + h - r + cy - 1, cx);
        BSP_LCD_DrawHLine(x + w - r, y + h - r + cx - 1, cy);
    }
}

/**
 * @brief  Draws a progress bar.
 */
void UI_DrawProgressBar(uint16_t x, uint16_t y, uint16_t w, uint16_t h, float percentage, uint32_t fg_color, uint32_t bg_color) {
    BSP_LCD_SetTextColor(bg_color);
    BSP_LCD_FillRect(x, y, w, h);

    if (percentage > 100.0f) percentage = 100.0f;
    if (percentage < 0.0f) percentage = 0.0f;

    uint16_t fill_w = (uint16_t)((w * percentage) / 100.0f);

    if (fill_w > 0) {
        BSP_LCD_SetTextColor(fg_color);
        BSP_LCD_FillRect(x, y, fill_w, h);
    }

    BSP_LCD_SetTextColor(GUI_COLOR_BORDER);
    BSP_LCD_DrawRect(x, y, w, h);
}


// Helper to blend color: result = alpha * fg + (1-alpha) * bg
// Optimized for speed?
// alpha is 0..255.
static inline uint32_t BlendColor(uint32_t fg, uint32_t bg, uint8_t alpha) {
    if (alpha == 0) return bg;
    if (alpha == 255) return fg;

    uint32_t r_fg = (fg >> 16) & 0xFF;
    uint32_t g_fg = (fg >> 8) & 0xFF;
    uint32_t b_fg = fg & 0xFF;

    uint32_t r_bg = (bg >> 16) & 0xFF;
    uint32_t g_bg = (bg >> 8) & 0xFF;
    uint32_t b_bg = bg & 0xFF;

    uint32_t r = (r_fg * alpha + r_bg * (255 - alpha)) / 255;
    uint32_t g = (g_fg * alpha + g_bg * (255 - alpha)) / 255;
    uint32_t b = (b_fg * alpha + b_bg * (255 - alpha)) / 255;

    return 0xFF000000 | (r << 16) | (g << 8) | b;
}

/**
 * @brief  Draws an Anti-Aliased Icon (48x48)
 *         Blends with the provided bg_color.
 */
void UI_DrawIconAA(uint16_t x, uint16_t y, const uint8_t* bitmap, uint32_t color, uint32_t bg_color) {
    // Iterate 48x48 pixels
    // Optimization: We could read current pixel from LCD to blend if bg is not solid,
    // but ReadPixel is slow. We assume solid background here as passed in arg.

    for (int iy = 0; iy < 48; iy++) {
        for (int ix = 0; ix < 48; ix++) {
            uint8_t alpha = bitmap[iy * 48 + ix];
            if (alpha > 0) {
                uint32_t final_color = BlendColor(color, bg_color, alpha);
                BSP_LCD_DrawPixel(x + ix, y + iy, final_color);
            }
        }
    }
}

/**
 * @brief  Draws an icon. Now uses Anti-Aliased Bitmaps where available.
 */
void UI_DrawIcon(uint16_t x, uint16_t y, uint16_t size, UI_IconType type, uint32_t color, uint32_t bg_color, bool active) {
    // Background Circle
    if (active) {
        BSP_LCD_SetTextColor(bg_color);
        BSP_LCD_FillCircle(x + size / 2, y + size / 2, size / 2);
    } else {
        BSP_LCD_SetTextColor(GUI_COLOR_INACTIVE);
        BSP_LCD_FillCircle(x + size / 2, y + size / 2, size / 2);
    }

    uint32_t iconColor = active ? color : GUI_COLOR_TEXT_SEC;

    // Check for Bitmap
    const uint8_t* bmp = GetIconBitmap(type);
    if (bmp) {
        // Center the 48x48 icon in the target size
        int16_t off_x = x + (size - 48) / 2;
        int16_t off_y = y + (size - 48) / 2;

        UI_DrawIconAA(off_x, off_y, bmp, iconColor, active ? bg_color : GUI_COLOR_INACTIVE);
        return;
    }

    // Fallback Procedural
    BSP_LCD_SetTextColor(iconColor);
    uint16_t cx = x + size / 2;
    uint16_t cy = y + size / 2;
    uint16_t s = size / 3;

    switch (type) {
        case ICON_ARROW_LEFT:
            BSP_LCD_DrawLine(cx + s/2, cy - s/2, cx - s/2, cy);
            BSP_LCD_DrawLine(cx - s/2, cy, cx + s/2, cy + s/2);
            break;
        case ICON_TOGGLE_ON:
             UI_FillRoundedRect(x, y + size/4, size, size/2, size/4, GUI_COLOR_SUCCESS);
             BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
             BSP_LCD_FillCircle(x + size - size/4, y + size/2, size/5);
             break;
        case ICON_TOGGLE_OFF:
             UI_FillRoundedRect(x, y + size/4, size, size/2, size/4, GUI_COLOR_INACTIVE);
             BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
             BSP_LCD_FillCircle(x + size/4, y + size/2, size/5);
             break;
        case ICON_BLE:
            BSP_LCD_DrawLine(cx, cy - s, cx, cy + s);
            break;
        default:
            break;
    }
}
