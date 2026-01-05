#ifndef UI_LVGL_H
#define UI_LVGL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ecoflow_protocol.h"
#include <stdbool.h>

void UI_LVGL_Init(void);
void UI_LVGL_Update(DeviceStatus* dev);
 void UI_UpdateConnectionCache(DeviceList* list);
void UI_LVGL_ShowDashboard(void);
void UI_LVGL_ShowWave2(void);
void UI_LVGL_ShowSettings(bool auto_del_current);

void UI_ResetIdleTimer(void);

// Access to cache for debug view
DeviceStatus* UI_GetDeviceCache(int index);

#ifdef __cplusplus
}
#endif

#endif // UI_LVGL_H
