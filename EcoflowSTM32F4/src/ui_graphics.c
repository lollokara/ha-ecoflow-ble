#include "ui_graphics.h"
#include <stdio.h>
#include "ui_vector.h"
#include "mdi_icons.h"

// Define MDI Path mapping
static const char* GetIconPath(UI_IconType type) {
    switch (type) {
        case ICON_SOLAR: return PATH_MDI_SOLAR;
        case ICON_AC_PLUG: return PATH_MDI_PLUG;
        case ICON_CAR_BATTERY: return PATH_MDI_CAR_BAT;
        case ICON_BATTERY: return PATH_MDI_BATTERY;
        case ICON_USB: return PATH_MDI_USB;
        case ICON_AC_SOCKET: return PATH_MDI_SOCKET;
        case ICON_SETTINGS: return PATH_MDI_SETTINGS;
        // Fallbacks for missing vector paths
        default: return NULL;
    }
}

/**
 * @brief  Draws a rounded rectangle outline.
 */
void UI_DrawRoundedRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t r, uint32_t color) {
    BSP_LCD_SetTextColor(color);

    // Draw straight lines
    BSP_LCD_DrawHLine(x + r, y, w - 2 * r);         // Top
    BSP_LCD_DrawHLine(x + r, y + h - 1, w - 2 * r); // Bottom
    BSP_LCD_DrawVLine(x, y + r, h - 2 * r);         // Left
    BSP_LCD_DrawVLine(x + w - 1, y + r, h - 2 * r); // Right

    // Draw corners (Midpoint circle algorithm)
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

        // Top Left
        BSP_LCD_DrawPixel(x + r - cx, y + r - cy, color);
        BSP_LCD_DrawPixel(x + r - cy, y + r - cx, color);

        // Top Right
        BSP_LCD_DrawPixel(x + w - r + cx - 1, y + r - cy, color);
        BSP_LCD_DrawPixel(x + w - r + cy - 1, y + r - cx, color);

        // Bottom Right
        BSP_LCD_DrawPixel(x + w - r + cx - 1, y + h - r + cy - 1, color);
        BSP_LCD_DrawPixel(x + w - r + cy - 1, y + h - r + cx - 1, color);

        // Bottom Left
        BSP_LCD_DrawPixel(x + r - cx, y + h - r + cy - 1, color);
        BSP_LCD_DrawPixel(x + r - cy, y + h - r + cx - 1, color);
    }
}

/**
 * @brief  Fills a rounded rectangle.
 *         Uses DMA2D for the center rectangles via BSP_LCD_FillRect.
 */
void UI_FillRoundedRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t r, uint32_t color) {
    BSP_LCD_SetTextColor(color);

    // Center block
    BSP_LCD_FillRect(x + r, y, w - 2 * r, h);

    // Left and Right blocks (minus corners)
    BSP_LCD_FillRect(x, y + r, r, h - 2 * r);
    BSP_LCD_FillRect(x + w - r, y + r, r, h - 2 * r);

    // Fill corners (Midpoint circle algorithm)
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

        // Draw horizontal lines to fill corners
        // Top Left & Top Right
        BSP_LCD_DrawHLine(x + r - cx, y + r - cy, cx); // TL
        BSP_LCD_DrawHLine(x + r - cy, y + r - cx, cy); // TL
        BSP_LCD_DrawHLine(x + w - r, y + r - cy, cx); // TR
        BSP_LCD_DrawHLine(x + w - r, y + r - cx, cy); // TR

        // Bottom Left & Bottom Right
        BSP_LCD_DrawHLine(x + r - cx, y + h - r + cy - 1, cx); // BL
        BSP_LCD_DrawHLine(x + r - cy, y + h - r + cx - 1, cy); // BL
        BSP_LCD_DrawHLine(x + w - r, y + h - r + cy - 1, cx); // BR
        BSP_LCD_DrawHLine(x + w - r, y + h - r + cx - 1, cy); // BR
    }
}

/**
 * @brief  Draws a progress bar.
 */
void UI_DrawProgressBar(uint16_t x, uint16_t y, uint16_t w, uint16_t h, float percentage, uint32_t fg_color, uint32_t bg_color) {
    // Background
    BSP_LCD_SetTextColor(bg_color);
    BSP_LCD_FillRect(x, y, w, h);

    // Foreground
    if (percentage > 100.0f) percentage = 100.0f;
    if (percentage < 0.0f) percentage = 0.0f;

    uint16_t fill_w = (uint16_t)((w * percentage) / 100.0f);

    if (fill_w > 0) {
        BSP_LCD_SetTextColor(fg_color);
        BSP_LCD_FillRect(x, y, fill_w, h);
    }

    // Border
    BSP_LCD_SetTextColor(GUI_COLOR_BORDER);
    BSP_LCD_DrawRect(x, y, w, h);
}


/**
 * @brief  Draws an icon using Vector SVG Paths where available, or procedural fallback.
 */
void UI_DrawIcon(uint16_t x, uint16_t y, uint16_t size, UI_IconType type, uint32_t color, uint32_t bg_color, bool active) {
    // Draw background circle if active
    if (active) {
        BSP_LCD_SetTextColor(bg_color);
        BSP_LCD_FillCircle(x + size / 2, y + size / 2, size / 2);
    } else {
        BSP_LCD_SetTextColor(GUI_COLOR_INACTIVE); // Or bg_color if gray is desired
        BSP_LCD_FillCircle(x + size / 2, y + size / 2, size / 2);
    }

    uint32_t iconColor = active ? color : GUI_COLOR_TEXT_SEC;

    // Check if we have an SVG path for this icon
    const char* path = GetIconPath(type);
    if (path) {
        // MDI icons are typically 24x24. Calculate scale.
        float target_size = size * 0.7f;
        float scale = target_size / 24.0f;

        // Offset to center
        int16_t off_x = x + (size - 24 * scale) / 2;
        int16_t off_y = y + (size - 24 * scale) / 2;

        UI_DrawSVGPath(path, off_x, off_y, scale, iconColor);
        return;
    }

    // Fallback to procedural drawing for icons without paths yet
    BSP_LCD_SetTextColor(iconColor);

    // Simple drawing logic for icons
    uint16_t cx = x + size / 2;
    uint16_t cy = y + size / 2;
    uint16_t s = size / 3; // Scale factor

    switch (type) {
        case ICON_CAR_DOOR: // Representing 12V output
             BSP_LCD_DrawRect(cx - s, cy, s*2, s/2);
             BSP_LCD_DrawLine(cx - s, cy, cx - s/2, cy - s/2);
             BSP_LCD_DrawLine(cx + s, cy, cx + s/2, cy - s/2);
             BSP_LCD_DrawLine(cx - s/2, cy - s/2, cx + s/2, cy - s/2);
             break;

        case ICON_ARROW_LEFT:
            BSP_LCD_DrawLine(cx + s/2, cy - s/2, cx - s/2, cy);
            BSP_LCD_DrawLine(cx - s/2, cy, cx + s/2, cy + s/2);
            break;

        case ICON_TOGGLE_ON:
             // Use our own helper, not BSP
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
            BSP_LCD_DrawLine(cx, cy - s, cx + s/2, cy - s/2);
            BSP_LCD_DrawLine(cx + s/2, cy - s/2, cx - s/2, cy + s/4);
            BSP_LCD_DrawLine(cx - s/2, cy + s/4, cx + s/2, cy + s/2);
            BSP_LCD_DrawLine(cx + s/2, cy + s/2, cx, cy + s);
            break;

        default:
            break;
    }
}
