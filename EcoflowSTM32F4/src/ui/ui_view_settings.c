#include "ui_core.h"

// --- Widget Implementations ---

void UI_DrawSlider(UI_Widget* w) {
    // Background Track
    BSP_LCD_SetTextColor(UI_COLOR_INACTIVE);
    BSP_LCD_FillRect(w->x, w->y + w->h/2 - 4, w->w, 8); // Track height 8px

    // Active Track
    int handle_x = w->x + (w->w * w->value) / 100;
    BSP_LCD_SetTextColor(UI_COLOR_ACCENT_TEAL);
    BSP_LCD_FillRect(w->x, w->y + w->h/2 - 4, handle_x - w->x, 8);

    // Handle
    BSP_LCD_FillCircle(handle_x, w->y + w->h/2, 12);

    // Label
    UI_DrawStringCentered(w->x, w->y - 20, w->w, w->label, UI_FONT_LABEL, UI_COLOR_TEXT);

    // Value
    char buf[8];
    snprintf(buf, sizeof(buf), "%d%%", w->value);
    UI_DrawStringCentered(w->x + w->w + 10, w->y + w->h/2 - 10, 50, buf, UI_FONT_LABEL, UI_COLOR_TEXT);
}

void UI_DrawToggle(UI_Widget* w) {
    // iOS Style Switch
    uint32_t bg = w->active ? UI_COLOR_SUCCESS : UI_COLOR_INACTIVE;
    UI_DrawFilledRoundedRect(w->x, w->y, 50, 30, 15, bg);

    // Knob
    int kx = w->active ? (w->x + 50 - 28) : (w->x + 2);
    BSP_LCD_SetTextColor(UI_COLOR_BG);
    BSP_LCD_FillCircle(kx + 13, w->y + 15, 13);

    // Label
    BSP_LCD_SetBackColor(UI_COLOR_BG);
    BSP_LCD_DisplayStringAt(w->x + 60, w->y + 5, (uint8_t*)w->label, LEFT_MODE);
}

// --- Settings View ---

static UI_Widget sldChargeLimit = {40, 60, 600, 40, true, false, 95, "Charge Limit", NULL, NULL, NULL};
static UI_Widget sldDischargeLimit = {40, 130, 600, 40, true, false, 10, "Discharge Limit", NULL, NULL, NULL};
static UI_Widget sldACInput = {40, 200, 600, 40, true, false, 80, "AC Input Max", NULL, NULL, NULL}; // 80% of 3000W
static UI_Widget btnBack = {10, 10, 40, 40, true, false, 0, "", NULL, NULL, NULL};

void UI_Render_Settings(void) {
    // BG
    BSP_LCD_Clear(UI_COLOR_BG);

    // Header
    BSP_LCD_DrawHLine(0, 50, 800);
    UI_DrawStringCentered(60, 15, 200, "Device Settings", UI_FONT_HEADER, UI_COLOR_TEXT);

    // Back Button
    UI_DrawVectorIcon(UI_ICON_BACK, 15, 15, 24, UI_COLOR_TEXT, true);
    (void)btnBack; // Silence unused warning if we only use coordinates via touch handler

    // Sliders
    UI_DrawSlider(&sldChargeLimit);
    UI_DrawSlider(&sldDischargeLimit);
    UI_DrawSlider(&sldACInput);

    // Save/Cancel Buttons
    UI_DrawFilledRoundedRect(40, 420, 350, 50, 5, UI_COLOR_INACTIVE); // Cancel
    UI_DrawStringCentered(40, 435, 350, "Cancel", UI_FONT_LARGE, UI_COLOR_TEXT);

    UI_DrawFilledRoundedRect(410, 420, 350, 50, 5, UI_COLOR_ACCENT_TEAL); // Save
    UI_DrawStringCentered(410, 435, 350, "Save", UI_FONT_LARGE, UI_COLOR_BG);
}

// Simple Touch Logic for Sliders
bool UI_HandleTouch_Settings(uint16_t x, uint16_t y, bool pressed) {
    if (pressed) {
        // Back Button
        if (x < 60 && y < 60) return true; // Signal Exit

        // Sliders
        UI_Widget* sliders[] = {&sldChargeLimit, &sldDischargeLimit, &sldACInput};
        for(int i=0; i<3; i++) {
            UI_Widget* w = sliders[i];
            if (x >= w->x && x <= w->x + w->w && y >= w->y && y <= w->y + w->h) {
                // Update Value
                int val = (x - w->x) * 100 / w->w;
                if(val < 0) val = 0;
                if(val > 100) val = 100;
                w->value = val;
                // Redraw immediately or set flag?
                // For simplicity, we just updated state. Main loop redraws.
            }
        }
    }
    return false;
}
