#ifndef UI_GRAPHICS_H
#define UI_GRAPHICS_H

#include "stm32469i_discovery_lcd.h"
#include <stdint.h>
#include <stdbool.h>

// UI Colors
#define GUI_COLOR_BG        LCD_COLOR_WHITE
#define GUI_COLOR_TEXT      LCD_COLOR_BLACK
#define GUI_COLOR_TEXT_SEC  0xFF333333
#define GUI_COLOR_ACCENT    0xFF00BCD4 // Teal/Cyan
#define GUI_COLOR_WARN      0xFFF44336 // Red
#define GUI_COLOR_CHARGE    0xFF2196F3 // Blue
#define GUI_COLOR_SUCCESS   0xFF4CAF50 // Green
#define GUI_COLOR_INACTIVE  0xFFCCCCCC // Light Gray
#define GUI_COLOR_DISCHARGE 0xFFFF9800 // Orange
#define GUI_COLOR_PANEL     0xFFF5F5F5 // Light Background
#define GUI_COLOR_BORDER    0xFF9E9E9E // Dark Gray Border

// Icon Types
typedef enum {
    ICON_SOLAR,
    ICON_AC_PLUG,
    ICON_CAR_BATTERY,
    ICON_BATTERY,
    ICON_USB,
    ICON_CAR_DOOR,
    ICON_AC_SOCKET,
    ICON_SETTINGS,
    ICON_ARROW_LEFT,
    ICON_TOGGLE_ON,
    ICON_TOGGLE_OFF,
    ICON_BLE
} UI_IconType;

// Function Prototypes
void UI_DrawRoundedRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t r, uint32_t color);
void UI_FillRoundedRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t r, uint32_t color);
void UI_DrawProgressBar(uint16_t x, uint16_t y, uint16_t w, uint16_t h, float percentage, uint32_t fg_color, uint32_t bg_color);
void UI_DrawIcon(uint16_t x, uint16_t y, uint16_t size, UI_IconType type, uint32_t color, uint32_t bg_color, bool active);

#endif // UI_GRAPHICS_H
