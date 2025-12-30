#include "ui_core.h"
#include <stdio.h>

// Dashboard View State
static UI_Widget btnSettings = {720, 360, 60, 40, true, false, 0, "", NULL, NULL, NULL};

// Setup Widgets
void UI_Dashboard_Init(void) {
    // Initialized in declaration
}

// Draw the Battery Status Panel (Top)
static void DrawBatteryPanel(int soc, int input_w, int output_w, int time_min, float temp) {
    // Background
    BSP_LCD_SetTextColor(UI_COLOR_BG);
    BSP_LCD_FillRect(0, 0, 800, 140);

    // Top Bar
    BSP_LCD_SetBackColor(UI_COLOR_BG);
    UI_DrawStringCentered(20, 5, 200, "Battery Status", UI_FONT_HEADER, UI_COLOR_TEXT);

    char tempStr[16];
    snprintf(tempStr, sizeof(tempStr), "Temp: %.0fC", temp);
    UI_DrawStringCentered(700, 5, 80, tempStr, UI_FONT_LABEL, UI_COLOR_TEXT_SEC);

    BSP_LCD_DrawHLine(0, 30, 800); // Separator

    // Charge Bar (Gradient)
    // Limits hardcoded for demo or passed in? Let's assume 10% / 95%
    UI_DrawGradientBar(50, 45, 700, 40, soc, 10, 95);

    // SOC Text Overlay
    char socStr[8];
    snprintf(socStr, sizeof(socStr), "%d%%", soc);
    BSP_LCD_SetBackColor(UI_COLOR_BG); // Fallback background

    // Draw Text with "middle gray" background simulation handled in DrawStringCentered?
    // BSP doesn't support transparent text background easily.
    // We draw it overlapping.
    BSP_LCD_SetBackColor(0xFF888888);
    UI_DrawStringCentered(350, 50, 100, socStr, UI_FONT_HUGE, UI_COLOR_TEXT);

    // Stats Row (105-140px)
    // 3 Columns
    int col_w = 230;
    int gap = (800 - 3*col_w)/4; // ~27px
    int y_stats = 105;

    char buf[32];

    // Col 1: Input
    BSP_LCD_SetTextColor(UI_COLOR_PANEL_BG);
    BSP_LCD_FillRect(gap, y_stats, col_w, 35);
    BSP_LCD_SetBackColor(UI_COLOR_PANEL_BG);
    snprintf(buf, sizeof(buf), "In: %dW", input_w);
    UI_DrawStringCentered(gap, y_stats+5, col_w, buf, UI_FONT_LARGE, UI_COLOR_TEXT);

    // Col 2: Output
    BSP_LCD_SetTextColor(UI_COLOR_PANEL_BG);
    BSP_LCD_FillRect(gap*2 + col_w, y_stats, col_w, 35);
    snprintf(buf, sizeof(buf), "Out: %dW", output_w);
    UI_DrawStringCentered(gap*2 + col_w, y_stats+5, col_w, buf, UI_FONT_LARGE, UI_COLOR_TEXT);

    // Col 3: Time
    BSP_LCD_SetTextColor(UI_COLOR_PANEL_BG);
    BSP_LCD_FillRect(gap*3 + col_w*2, y_stats, col_w, 35);
    int hrs = time_min / 60;
    int mins = time_min % 60;
    snprintf(buf, sizeof(buf), "%02d:%02d rem", hrs, mins);
    UI_DrawStringCentered(gap*3 + col_w*2, y_stats+5, col_w, buf, UI_FONT_LARGE, UI_COLOR_TEXT);
}


// Draw Energy Flow Diagram (Middle)
static void DrawFlowDiagram(int input_solar, int input_ac, int input_alt, int soc, int out_usb, int out_12v, int out_ac) {
    int y_start = 140;
    int height = 210;

    // Background
    BSP_LCD_SetTextColor(UI_COLOR_BG);
    BSP_LCD_FillRect(0, y_start, 800, height);

    // Center Battery
    int bat_x = 350;
    int bat_y = y_start + 60;
    UI_DrawVectorIcon(UI_ICON_BATTERY, bat_x, bat_y, 80, UI_COLOR_ACCENT_TEAL, true);

    char socStr[16];
    snprintf(socStr, sizeof(socStr), "%d%%", soc);
    BSP_LCD_SetBackColor(UI_COLOR_BG);
    UI_DrawStringCentered(bat_x, bat_y + 90, 80, socStr, UI_FONT_HUGE, UI_COLOR_TEXT);

    // Inputs (Left)
    // Solar
    int icon_size = 48;
    int x_left = 40;
    int y_solar = y_start + 20;
    bool solar_active = input_solar > 0;
    // Draw BG Circle
    BSP_LCD_SetTextColor(solar_active ? 0xFFE8F5E9 : UI_COLOR_INACTIVE);
    BSP_LCD_FillCircle(x_left + 24, y_solar + 24, 30);
    UI_DrawVectorIcon(UI_ICON_SOLAR, x_left, y_solar, icon_size, solar_active ? 0xFFF9A825 : UI_COLOR_TEXT_SEC, true);
    char buf[16];
    snprintf(buf, sizeof(buf), "%dW", input_solar);
    UI_DrawStringCentered(x_left - 10, y_solar + 50, 70, buf, UI_FONT_LABEL, UI_COLOR_TEXT);

    // Flow Line Solar -> Bat
    if (solar_active) {
        BSP_LCD_SetTextColor(UI_COLOR_SUCCESS);
        BSP_LCD_DrawLine(x_left + 60, y_solar + 24, bat_x, bat_y + 40);
    }

    // AC Input
    int y_ac = y_start + 95;
    bool ac_active = input_ac > 0;
    BSP_LCD_SetTextColor(ac_active ? 0xFFE8F5E9 : UI_COLOR_INACTIVE);
    BSP_LCD_FillCircle(x_left + 24, y_ac + 24, 30);
    UI_DrawVectorIcon(UI_ICON_GRID, x_left, y_ac, icon_size, ac_active ? UI_COLOR_TEXT : UI_COLOR_TEXT_SEC, true);
    snprintf(buf, sizeof(buf), "%dW", input_ac);
    UI_DrawStringCentered(x_left - 10, y_ac + 50, 70, buf, UI_FONT_LABEL, UI_COLOR_TEXT);

    // Flow Line AC -> Bat
    if (ac_active) {
        BSP_LCD_SetTextColor(UI_COLOR_SUCCESS);
        BSP_LCD_DrawLine(x_left + 60, y_ac + 24, bat_x, bat_y + 40);
    }

    // Alt Input
    int y_alt = y_start + 170;
    bool alt_active = input_alt > 0;
    BSP_LCD_SetTextColor(alt_active ? 0xFFE8F5E9 : UI_COLOR_INACTIVE);
    BSP_LCD_FillCircle(x_left + 24, y_alt + 24, 30);
    UI_DrawVectorIcon(UI_ICON_CAR, x_left, y_alt, icon_size, alt_active ? UI_COLOR_ACCENT_TEAL : UI_COLOR_TEXT_SEC, true);
    snprintf(buf, sizeof(buf), "%dW", input_alt);
    UI_DrawStringCentered(x_left - 10, y_alt + 50, 70, buf, UI_FONT_LABEL, UI_COLOR_TEXT);

    // Outputs (Right)
    int x_right = 620;

    // USB
    int y_usb = y_start + 20;
    bool usb_active = true; // Always on
    BSP_LCD_SetTextColor(0xFFE8F5E9);
    BSP_LCD_FillCircle(x_right + 24, y_usb + 24, 30);
    UI_DrawVectorIcon(UI_ICON_USB, x_right, y_usb, icon_size, UI_COLOR_LIMIT_BLUE, true);
    snprintf(buf, sizeof(buf), "%dW", out_usb);
    UI_DrawStringCentered(x_right - 10, y_usb + 50, 70, buf, UI_FONT_LABEL, UI_COLOR_TEXT);
    (void)usb_active;

    // Flow Line Bat -> USB
    BSP_LCD_SetTextColor(UI_COLOR_WARN_RED); // Discharge
    BSP_LCD_DrawLine(bat_x + 80, bat_y + 40, x_right, y_usb + 24);

    // 12V
    int y_12v = y_start + 95;
    bool v12_active = out_12v > 0; // Or switch state
    BSP_LCD_SetTextColor(v12_active ? 0xFFE8F5E9 : UI_COLOR_INACTIVE);
    BSP_LCD_FillCircle(x_right + 24, y_12v + 24, 30);
    UI_DrawVectorIcon(UI_ICON_12V, x_right, y_12v, icon_size, v12_active ? UI_COLOR_TEXT : UI_COLOR_TEXT_SEC, true);
    snprintf(buf, sizeof(buf), "%dW", out_12v);
    UI_DrawStringCentered(x_right - 10, y_12v + 50, 70, buf, UI_FONT_LABEL, UI_COLOR_TEXT);

    // AC Out
    int y_ac_out = y_start + 170;
    bool ac_out_active = out_ac > 0;
    BSP_LCD_SetTextColor(ac_out_active ? 0xFFE8F5E9 : UI_COLOR_INACTIVE);
    BSP_LCD_FillCircle(x_right + 24, y_ac_out + 24, 30);
    UI_DrawVectorIcon(UI_ICON_AC_OUT, x_right, y_ac_out, icon_size, ac_out_active ? UI_COLOR_TEXT : UI_COLOR_TEXT_SEC, true);
    snprintf(buf, sizeof(buf), "%dW", out_ac);
    UI_DrawStringCentered(x_right - 10, y_ac_out + 50, 70, buf, UI_FONT_LABEL, UI_COLOR_TEXT);
}

// Bottom Stats & Footer
static void DrawFooter(void) {
    int y_start = 350;
    BSP_LCD_SetTextColor(UI_COLOR_BG);
    BSP_LCD_FillRect(0, y_start, 800, 130);

    // Settings Button
    UI_DrawFilledRoundedRect(btnSettings.x, btnSettings.y, btnSettings.w, btnSettings.h, 4, UI_COLOR_ACCENT_TEAL);
    UI_DrawVectorIcon(UI_ICON_SETTINGS, btnSettings.x + 18, btnSettings.y + 8, 24, UI_COLOR_BG, true);

    // Status badges
    UI_DrawStringCentered(365, y_start + 15, 100, "BMS: OK", UI_FONT_LABEL, UI_COLOR_SUCCESS);
    UI_DrawStringCentered(365, y_start + 40, 100, "BLE: ON", UI_FONT_LABEL, UI_COLOR_LIMIT_BLUE);
}

void UI_Render_Dashboard(DeviceStatus* dev) {
    // Map data
    int soc = 0, in_w = 0, out_w = 0;

    // Simple mapping for D3P/D3/W2
    if (dev->id == DEV_TYPE_DELTA_PRO_3) {
        soc = (int)dev->data.d3p.batteryLevel;
        in_w = (int)dev->data.d3p.inputPower;
        out_w = (int)dev->data.d3p.outputPower;
    } else if (dev->id == DEV_TYPE_DELTA_3) {
        soc = (int)dev->data.d3.batteryLevel;
        in_w = (int)dev->data.d3.inputPower;
        out_w = (int)dev->data.d3.outputPower;
    }

    DrawBatteryPanel(soc, in_w, out_w, 120, 25.0f);
    DrawFlowDiagram(in_w, 0, 0, soc, out_w, 0, 0); // Todo: Break down input/output sources if data allows
    DrawFooter();
}

bool UI_CheckTouch_Dashboard(uint16_t x, uint16_t y) {
    // Check Settings Button
    if (x >= btnSettings.x && x <= btnSettings.x + btnSettings.w &&
        y >= btnSettings.y && y <= btnSettings.y + btnSettings.h) {
        return true; // Clicked Settings
    }
    return false;
}
