#ifndef UI_LVGL_H
#define UI_LVGL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ecoflow_protocol.h"

void UI_LVGL_Init(void);
void UI_LVGL_Update(DeviceStatus* dev);
void UI_LVGL_CheckConnection(void);
void UI_LVGL_ShowDashboard(void);
void UI_LVGL_ShowWave2(void);

#ifdef __cplusplus
}
#endif

#endif /* UI_LVGL_H */
