#include "ui_layout.h"
#include <stdio.h>

extern void UI_DrawBatteryStatus(DeviceStatus *status);
extern void UI_DrawEnergyFlow(DeviceStatus *status);

/**
 * @brief  Draws Section 1: Battery Status Panel (Top)
 */
void UI_DrawBatteryStatus(DeviceStatus *status) {
    // 1. Top Bar
    BSP_LCD_SetTextColor(GUI_COLOR_BG);
    BSP_LCD_FillRect(0, 0, 800, 30);
    BSP_LCD_SetTextColor(GUI_COLOR_BORDER);
    BSP_LCD_DrawHLine(0, 30, 800);

    BSP_LCD_SetBackColor(GUI_COLOR_BG);
    BSP_LCD_SetTextColor(GUI_COLOR_TEXT);
    BSP_LCD_SetFont(&Font16);
    BSP_LCD_DisplayStringAt(20, 7, (uint8_t *)"Battery Status", LEFT_MODE);

    char tempBuf[16];
    // Placeholder temp, assuming data available or hardcoded for now
    snprintf(tempBuf, sizeof(tempBuf), "Temp: 25C");
    BSP_LCD_SetFont(&Font12);
    BSP_LCD_DisplayStringAt(700, 9, (uint8_t *)tempBuf, LEFT_MODE);

    // 2. Charge Bar
    // Calculate battery percentage
    int soc = 0;
    if (status->id == DEV_TYPE_DELTA_PRO_3) soc = (int)status->data.d3p.batteryLevel;
    else if (status->id == DEV_TYPE_DELTA_3) soc = (int)status->data.d3.batteryLevel;
    else if (status->id == DEV_TYPE_WAVE_2) soc = status->data.w2.batSoc;
    else if (status->id == DEV_TYPE_ALT_CHARGER) soc = (int)status->data.ac.batteryLevel;

    uint32_t barColor = GUI_COLOR_SUCCESS;
    if (soc < 20) barColor = GUI_COLOR_WARN;
    else if (soc < 50) barColor = GUI_COLOR_DISCHARGE; // Orange

    UI_DrawProgressBar(50, 40, 700, 60, (float)soc, barColor, GUI_COLOR_PANEL);

    // Draw Limits Lines (Static for now as per spec example)
    // Red Line: 10%
    uint16_t redX = 50 + (700 * 10 / 100);
    BSP_LCD_SetTextColor(GUI_COLOR_WARN);
    BSP_LCD_FillRect(redX, 40, 3, 60);
    BSP_LCD_SetTextColor(GUI_COLOR_TEXT);
    BSP_LCD_SetBackColor(GUI_COLOR_BG);
    BSP_LCD_SetFont(&Font12);
    BSP_LCD_DisplayStringAt(redX - 10, 102, (uint8_t *)"10%", LEFT_MODE);

    // Blue Line: 95%
    uint16_t blueX = 50 + (700 * 95 / 100);
    BSP_LCD_SetTextColor(GUI_COLOR_CHARGE);
    BSP_LCD_FillRect(blueX, 40, 3, 60);
    BSP_LCD_SetTextColor(GUI_COLOR_TEXT);
    BSP_LCD_DisplayStringAt(blueX - 10, 102, (uint8_t *)"95%", LEFT_MODE);

    // Centered Percentage
    char socBuf[16];
    snprintf(socBuf, sizeof(socBuf), "%d%%", soc);
    BSP_LCD_SetFont(&Font24);
    BSP_LCD_SetBackColor(GUI_COLOR_BG); // White
    // Or clear a small rect in middle
    BSP_LCD_FillRect(375, 55, 50, 30);
    BSP_LCD_SetTextColor(GUI_COLOR_TEXT);
    BSP_LCD_DisplayStringAt(385, 60, (uint8_t *)socBuf, LEFT_MODE);

    // 3. Stats Row
    BSP_LCD_SetTextColor(GUI_COLOR_PANEL);
    BSP_LCD_FillRect(0, 105, 800, 35); // Clear row area

    // Columns
    // 1. Input Power
    int inputWatts = 0; // Extract from data
    if (status->id == DEV_TYPE_DELTA_PRO_3) inputWatts = (int)status->data.d3p.inputPower;

    char pwrBuf[32];
    snprintf(pwrBuf, sizeof(pwrBuf), "In: %dW", inputWatts);
    BSP_LCD_SetTextColor(GUI_COLOR_TEXT);
    BSP_LCD_SetBackColor(GUI_COLOR_PANEL);
    BSP_LCD_SetFont(&Font16);
    BSP_LCD_DisplayStringAt(50, 115, (uint8_t *)pwrBuf, LEFT_MODE);

    // 2. Output Power
    int outputWatts = 0;
    if (status->id == DEV_TYPE_DELTA_PRO_3) outputWatts = (int)status->data.d3p.outputPower;

    snprintf(pwrBuf, sizeof(pwrBuf), "Out: %dW", outputWatts);
    BSP_LCD_DisplayStringAt(350, 115, (uint8_t *)pwrBuf, LEFT_MODE);

    // 3. Time Remaining
    int mins = 0;
    if (status->id == DEV_TYPE_DELTA_PRO_3) mins = (int)status->data.d3p.remainingTime;
    snprintf(pwrBuf, sizeof(pwrBuf), "%dh %dm left", mins/60, mins%60);
    BSP_LCD_DisplayStringAt(600, 115, (uint8_t *)pwrBuf, LEFT_MODE);

    // Borders for stats
    BSP_LCD_SetTextColor(GUI_COLOR_BORDER);
    BSP_LCD_DrawRect(10, 105, 250, 35);
    BSP_LCD_DrawRect(275, 105, 250, 35);
    BSP_LCD_DrawRect(540, 105, 250, 35);
}

/**
 * @brief  Draws Section 2: Energy Flow Diagram (Middle)
 */
void UI_DrawEnergyFlow(DeviceStatus *status) {
    // Clear Area
    BSP_LCD_SetTextColor(GUI_COLOR_BG);
    BSP_LCD_FillRect(0, 140, 800, 210);

    // Left Column (Inputs)
    int solarW = 0;
    int acW = 0;
    int altW = 0;

    // Center (Battery)
    int batSoc = 0;

    // Right Column (Outputs)
    int usbW = 0;
    int dcW = 0;
    int acOutW = 0;

    if (status->id == DEV_TYPE_DELTA_PRO_3) {
        solarW = (int)status->data.d3p.solarPower;
        acW = (int)status->data.d3p.acPower; // Assuming acInPower
        // altW?
        batSoc = (int)status->data.d3p.batteryLevel;
        usbW = (int)status->data.d3p.usbPower;
        dcW = (int)status->data.d3p.dcPower;
        acOutW = (int)status->data.d3p.acPowerOut; // simplified
    }

    // Solar
    UI_DrawIcon(20, 160, 48, ICON_SOLAR, solarW > 0 ? GUI_COLOR_SUCCESS : GUI_COLOR_TEXT_SEC, GUI_COLOR_PANEL, true);
    char buf[16];
    snprintf(buf, sizeof(buf), "%dW", solarW);
    BSP_LCD_SetFont(&Font12);
    BSP_LCD_DisplayStringAt(20, 210, (uint8_t*)buf, LEFT_MODE);

    // AC Input
    UI_DrawIcon(20, 235, 48, ICON_AC_PLUG, acW > 0 ? GUI_COLOR_SUCCESS : GUI_COLOR_TEXT_SEC, GUI_COLOR_PANEL, true);
    snprintf(buf, sizeof(buf), "%dW", acW);
    BSP_LCD_DisplayStringAt(20, 285, (uint8_t*)buf, LEFT_MODE);

    // Battery (Center)
    UI_DrawIcon(350, 200, 80, ICON_BATTERY, GUI_COLOR_ACCENT, GUI_COLOR_BG, true);
    snprintf(buf, sizeof(buf), "%d%%", batSoc);
    BSP_LCD_SetFont(&Font24);
    BSP_LCD_DisplayStringAt(365, 290, (uint8_t*)buf, LEFT_MODE);

    // Outputs
    // USB
    UI_DrawIcon(620, 160, 48, ICON_USB, GUI_COLOR_SUCCESS, GUI_COLOR_PANEL, true);
    snprintf(buf, sizeof(buf), "%dW", usbW);
    BSP_LCD_SetFont(&Font12);
    BSP_LCD_DisplayStringAt(620, 210, (uint8_t*)buf, LEFT_MODE);

    // AC Out
    UI_DrawIcon(620, 310, 48, ICON_AC_SOCKET, acOutW > 0 ? GUI_COLOR_SUCCESS : GUI_COLOR_TEXT_SEC, GUI_COLOR_PANEL, true);
    snprintf(buf, sizeof(buf), "%dW", acOutW);
    BSP_LCD_DisplayStringAt(620, 360, (uint8_t*)buf, LEFT_MODE);

    // Connections (Lines)
    BSP_LCD_SetTextColor(GUI_COLOR_BORDER);
    // Solar -> Battery
    BSP_LCD_DrawLine(70, 184, 350, 240);
    // AC In -> Battery
    BSP_LCD_DrawLine(70, 259, 350, 240);

    // Battery -> USB
    BSP_LCD_DrawLine(430, 240, 620, 184);
    // Battery -> AC Out
    BSP_LCD_DrawLine(430, 240, 620, 334);
}

/**
 * @brief  Draws Section 3: Footer and Settings
 */
void UI_DrawFooter(DeviceStatus *status) {
    // Clear Footer Area
    BSP_LCD_SetTextColor(GUI_COLOR_BG);
    BSP_LCD_FillRect(0, 350, 800, 130);
    BSP_LCD_SetTextColor(GUI_COLOR_BORDER);
    BSP_LCD_DrawHLine(0, 350, 800);

    // Settings Button
    UI_FillRoundedRect(720, 360, 60, 40, 4, GUI_COLOR_ACCENT);
    UI_DrawIcon(726, 364, 24, ICON_SETTINGS, GUI_COLOR_BG, GUI_COLOR_ACCENT, true); // White icon

    // Status Indicators
    BSP_LCD_SetFont(&Font12);
    BSP_LCD_SetTextColor(GUI_COLOR_TEXT);
    BSP_LCD_SetBackColor(GUI_COLOR_BG);

    BSP_LCD_DisplayStringAt(365, 360, (uint8_t*)"BMS Status: OK", LEFT_MODE);
    BSP_LCD_DisplayStringAt(365, 380, (uint8_t*)"Conn: Connected", LEFT_MODE);
    BSP_LCD_DisplayStringAt(365, 400, (uint8_t*)"Last Update: Now", LEFT_MODE);
}
