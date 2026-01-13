#ifndef UI_LVGL_H
#define UI_LVGL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "ecoflow_protocol.h"
#include <stdbool.h>
#include <string.h>

static inline float get_float_aligned(const float *ptr) {
    float val;
    memcpy(&val, ptr, sizeof(float));
    return val;
}

static inline int32_t get_int32_aligned(const int32_t *ptr) {
    int32_t val;
    memcpy(&val, ptr, sizeof(int32_t));
    return val;
}

void UI_LVGL_Init(void);
void UI_LVGL_Update(DeviceStatus* dev);
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
