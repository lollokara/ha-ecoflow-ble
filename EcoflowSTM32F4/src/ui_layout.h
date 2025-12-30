#ifndef UI_LAYOUT_H
#define UI_LAYOUT_H

#include "ui_graphics.h"
#include "display_task.h" // For DeviceStatus

// UI Layout Function Prototypes
void UI_DrawBatteryStatus(DeviceStatus *status);
void UI_DrawEnergyFlow(DeviceStatus *status);
void UI_DrawFooter(DeviceStatus *status);
void UI_HandleTouch(uint16_t x, uint16_t y, DeviceStatus *status);

#endif // UI_LAYOUT_H
