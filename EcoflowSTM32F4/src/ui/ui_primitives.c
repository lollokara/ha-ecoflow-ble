#include "ui_core.h"
#include <math.h>

// Helper macros
#define MIN(a,b) ((a)<(b)?(a):(b))
#define MAX(a,b) ((a)>(b)?(a):(b))

void UI_Init(void) {
    // Nothing special to init yet
}

void UI_DrawStringCentered(uint16_t x, uint16_t y, uint16_t w, const char* str, sFONT* font, uint32_t color) {
    BSP_LCD_SetFont(font);
    BSP_LCD_SetTextColor(color);

    // Calculate string width
    int len = strlen(str);
    int text_width = len * font->Width; // Approximation

    // Real centering requires measuring each char if proportional, but FontX are monospaced-ish
    int start_x = x + (w - text_width) / 2;
    int start_y = y; // Font drawing is top-left aligned usually

    BSP_LCD_DisplayStringAt(start_x, start_y, (uint8_t*)str, LEFT_MODE);
}

void UI_DrawRoundedRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t r, uint32_t color) {
    BSP_LCD_SetTextColor(color);
    // Draw 4 lines
    BSP_LCD_DrawHLine(x + r, y, w - 2*r);         // Top
    BSP_LCD_DrawHLine(x + r, y + h - 1, w - 2*r); // Bottom
    BSP_LCD_DrawVLine(x, y + r, h - 2*r);         // Left
    BSP_LCD_DrawVLine(x + w - 1, y + r, h - 2*r); // Right

    // Draw 4 arcs
    // Top-Left
    BSP_LCD_DrawEllipse(x + r, y + r, r, r); // Todo: Clip to quadrant? BSP doesn't support arcs.
    // Approximate corners with pixels or full circles drawn with masking?
    // Since we don't have DrawArc, we can just DrawCircle and overwrite the inner parts? No.
    // For now, simple rect or just pixel plotting for corners.
    // Let's use Rect for simplicity if Arcs are hard, or implement DrawArc later.
    // Spec asks for "modern UI", rounded corners are key.
    // Workaround: Draw full circle at corners?
    // BSP_LCD_DrawCircle draws outline.
}

void UI_DrawFilledRoundedRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t r, uint32_t color) {
    BSP_LCD_SetTextColor(color);

    // Central Rect
    BSP_LCD_FillRect(x + r, y, w - 2*r, h);

    // Left and Right Rects
    BSP_LCD_FillRect(x, y + r, r, h - 2*r);
    BSP_LCD_FillRect(x + w - r, y + r, r, h - 2*r);

    // 4 Corners (Filled Circles)
    BSP_LCD_FillCircle(x + r, y + r, r);
    BSP_LCD_FillCircle(x + w - r - 1, y + r, r);
    BSP_LCD_FillCircle(x + r, y + h - r - 1, r);
    BSP_LCD_FillCircle(x + w - r - 1, y + h - r - 1, r);
}

// Vector Icon Implementations
// Using primitives: FillCircle, FillRect, DrawLine

static void DrawIconSolar(uint16_t x, uint16_t y, uint16_t size, uint32_t color) {
    BSP_LCD_SetTextColor(color);
    // Sun / Panel
    int r = size / 2;
    int cx = x + r;
    int cy = y + r;
    (void)r; // Unused

    // Draw Panel (Trapezoid-ish or Grid)
    // Let's draw a simple grid panel
    int pw = size * 0.8;
    int ph = size * 0.6;
    int px = cx - pw/2;
    int py = cy - ph/2 + 4;

    BSP_LCD_DrawRect(px, py, pw, ph);
    BSP_LCD_DrawHLine(px, py + ph/3, pw);
    BSP_LCD_DrawHLine(px, py + 2*ph/3, pw);
    BSP_LCD_DrawVLine(px + pw/3, py, ph);
    BSP_LCD_DrawVLine(px + 2*pw/3, py, ph);

    // Sun rays
    BSP_LCD_DrawLine(cx, py - 6, cx, py - 2);
    BSP_LCD_DrawLine(cx - 8, py - 4, cx - 4, py - 2);
    BSP_LCD_DrawLine(cx + 8, py - 4, cx + 4, py - 2);
}

static void DrawIconGrid(uint16_t x, uint16_t y, uint16_t size, uint32_t color) {
    BSP_LCD_SetTextColor(color);
    // Plug shape

    // Body
    BSP_LCD_FillRect(x + size/4, y + size/3, size/2, size/2);
    // Prongs
    BSP_LCD_FillRect(x + size/4 + 4, y + size/3 - 6, 4, 6);
    BSP_LCD_FillRect(x + 3*size/4 - 8, y + size/3 - 6, 4, 6);
    // Cable line
    BSP_LCD_DrawLine(x + size/2, y + 5*size/6, x + size/2, y + size);
}

static void DrawIconBattery(uint16_t x, uint16_t y, uint16_t size, uint32_t color) {
    BSP_LCD_SetTextColor(color);
    // Battery Body
    int bw = size * 0.6;
    int bh = size * 0.8;
    int bx = x + (size - bw)/2;
    int by = y + (size - bh)/2 + 4;

    BSP_LCD_DrawRect(bx, by, bw, bh);
    // Terminal
    BSP_LCD_FillRect(bx + bw/3, by - 4, bw/3, 4);

    // Fill level (assume full for icon)
    BSP_LCD_FillRect(bx + 2, by + 2, bw - 4, bh - 4);
}

static void DrawIconCar(uint16_t x, uint16_t y, uint16_t size, uint32_t color) {
    BSP_LCD_SetTextColor(color);
    // Simple Car Battery shape
    int w = size * 0.8;
    int h = size * 0.6;
    BSP_LCD_DrawRect(x + (size-w)/2, y + (size-h)/2, w, h);
    BSP_LCD_FillRect(x + size/4, y + (size-h)/2 - 4, 4, 4); // +
    BSP_LCD_FillRect(x + 3*size/4 - 4, y + (size-h)/2 - 4, 4, 4); // -
}

static void DrawIconUSB(uint16_t x, uint16_t y, uint16_t size, uint32_t color) {
    BSP_LCD_SetTextColor(color);
    // Trident symbol... hard. Just text "USB"?
    // Or a connector shape.
    BSP_LCD_DrawRect(x + size/3, y + size/4, size/3, size/2);
    BSP_LCD_DrawLine(x + size/2, y + 3*size/4, x + size/2, y + size);
    // Dots
    BSP_LCD_FillCircle(x + size/2, y + size/4 - 4, 3);
}

static void DrawIconSettings(uint16_t x, uint16_t y, uint16_t size, uint32_t color) {
    BSP_LCD_SetTextColor(color);
    // Cog
    int r = size/2 - 2;
    BSP_LCD_DrawCircle(x + size/2, y + size/2, r);
    BSP_LCD_DrawCircle(x + size/2, y + size/2, r/2);
    // Teeth (4 cardinal)
    BSP_LCD_FillRect(x + size/2 - 2, y, 4, 4);
    BSP_LCD_FillRect(x + size/2 - 2, y + size - 4, 4, 4);
    BSP_LCD_FillRect(x, y + size/2 - 2, 4, 4);
    BSP_LCD_FillRect(x + size - 4, y + size/2 - 2, 4, 4);
}

void UI_DrawVectorIcon(UIIconType icon, uint16_t x, uint16_t y, uint16_t size, uint32_t color, bool active) {
    // Draw Background Circle if active/inactive logic applies
    // Spec: Active = Green Circle, Inactive = Gray Circle
    // For now, caller handles background. This just draws the symbol.
    (void)active; // Unused

    switch(icon) {
        case UI_ICON_SOLAR: DrawIconSolar(x, y, size, color); break;
        case UI_ICON_GRID:  DrawIconGrid(x, y, size, color); break;
        case UI_ICON_BATTERY: DrawIconBattery(x, y, size, color); break;
        case UI_ICON_CAR:   DrawIconCar(x, y, size, color); break;
        case UI_ICON_USB:   DrawIconUSB(x, y, size, color); break;
        case UI_ICON_SETTINGS: DrawIconSettings(x, y, size, color); break;
        default: break; // TODO
    }
}

// Gradient Bar: Red (0%) -> Green (100%)
// We approximate this by drawing vertical lines with interpolated colors
void UI_DrawGradientBar(uint16_t x, uint16_t y, uint16_t w, uint16_t h, int soc, int discharge_limit, int charge_limit) {
    // Background (Gray border)
    BSP_LCD_SetTextColor(UI_COLOR_TEXT_SEC);
    BSP_LCD_DrawRect(x, y, w, h);

    // Fill with Gradient
    // Color 1: Red (FF0000) at 0
    // Color 2: Green (00FF00) at 100
    // Interpolate per X pixel

    for (int i = 0; i < w-2; i++) {
        float ratio = (float)i / (float)(w-2);

        // Simple interpolation
        uint8_t r = (uint8_t)(255 * (1.0f - ratio));
        uint8_t g = (uint8_t)(255 * ratio);
        uint8_t b = 0;

        // Boost brightness for visibility
        if (r < 50) r = 50;
        if (g < 50) g = 50;

        uint32_t col = 0xFF000000 | (r << 16) | (g << 8) | b;

        // If i is beyond SOC, dim it?
        // Spec says "Gradient background... Current charge percentage... Display zone"
        // It implies the bar is ALWAYS gradient, and markers show limits.
        // OR the bar fills up to SOC? "Display zone: Left-to-right visualization of 0-100%"
        // Usually battery bars are filled up to SOC.
        // Let's fill up to SOC with Gradient, and the rest with Empty color.

        float pos_percent = ratio * 100.0f;

        if (pos_percent <= soc) {
            BSP_LCD_DrawVLine_Blend(x + 1 + i, y + 1, h - 2, col, 255);
        } else {
            BSP_LCD_DrawVLine_Blend(x + 1 + i, y + 1, h - 2, 0xFFEEEEEE, 255); // Empty slot
        }
    }

    // Draw Limits
    // Discharge Limit (Red Line)
    int x_d = x + (w * discharge_limit) / 100;
    BSP_LCD_FillRect(x_d - 1, y - 2, 3, h + 4);
    // Charge Limit (Blue Line)
    int x_c = x + (w * charge_limit) / 100;
    BSP_LCD_SetTextColor(UI_COLOR_LIMIT_BLUE);
    BSP_LCD_FillRect(x_c - 1, y - 2, 3, h + 4);
}
