#ifndef UI_CORE_H
#define UI_CORE_H

#include "stm32469i_discovery_lcd.h"
#include <stdbool.h>
#include <stdio.h>
#include <stdint.h>
#include "ecoflow_protocol.h" // Include this for DeviceStatus

// --- Color Palette (ARGB8888) ---
#define UI_COLOR_BG           0xFFFFFFFF  // Pure White
#define UI_COLOR_TEXT         0xFF000000  // Black
#define UI_COLOR_TEXT_SEC     0xFF333333  // Dark Gray
#define UI_COLOR_ACCENT_TEAL  0xFF00BCD4  // Charging / Teal
#define UI_COLOR_ACCENT_ORANGE 0xFFFF9800 // Discharging / Orange
#define UI_COLOR_WARN_RED     0xFFF44336  // Warning / Red
#define UI_COLOR_LIMIT_BLUE   0xFF2196F3  // Charging Limit
#define UI_COLOR_SUCCESS      0xFF4CAF50  // Success / Green
#define UI_COLOR_INACTIVE     0xFFCCCCCC  // Inactive / Light Gray
#define UI_COLOR_PANEL_BG     0xFFF5F5F5  // Light Gray Panel Background

// --- Fonts Mapping ---
#define UI_FONT_BODY    &Font16 // Target 15px
#define UI_FONT_LABEL   &Font12 // Target 14px
#define UI_FONT_HEADER  &Font16 // Target 16px (Semibold simulation?)
#define UI_FONT_LARGE   &Font20 // Target 20px
#define UI_FONT_HUGE    &Font24 // Target 24px+

// --- Icon Types ---
typedef enum {
    UI_ICON_SOLAR,
    UI_ICON_GRID,
    UI_ICON_CAR,
    UI_ICON_USB,
    UI_ICON_12V,
    UI_ICON_AC_OUT,
    UI_ICON_BATTERY,
    UI_ICON_SETTINGS,
    UI_ICON_BACK,
    UI_ICON_BLE
} UIIconType;

// --- Widget Types ---
typedef struct UI_Widget UI_Widget;
typedef void (*DrawFunc)(UI_Widget* w);
typedef bool (*TouchFunc)(UI_Widget* w, uint16_t x, uint16_t y, bool pressed);

struct UI_Widget {
    uint16_t x, y, w, h;
    bool visible;
    bool active;     // Pressed or Toggled state
    int  value;      // For sliders/toggles
    char label[32];
    DrawFunc draw;
    TouchFunc onTouch;
    void* user_data;
};

// --- Core Functions ---
void UI_Init(void);
void UI_DrawRoundedRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t r, uint32_t color);
void UI_DrawFilledRoundedRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h, uint16_t r, uint32_t color);
void UI_DrawVectorIcon(UIIconType icon, uint16_t x, uint16_t y, uint16_t size, uint32_t color, bool active);
void UI_DrawProgressBar(uint16_t x, uint16_t y, uint16_t w, uint16_t h, float percent, uint32_t color);
void UI_DrawGradientBar(uint16_t x, uint16_t y, uint16_t w, uint16_t h, int soc, int discharge_limit, int charge_limit);

// Helper for strings
void UI_DrawStringCentered(uint16_t x, uint16_t y, uint16_t w, const char* str, sFONT* font, uint32_t color);

#endif // UI_CORE_H
