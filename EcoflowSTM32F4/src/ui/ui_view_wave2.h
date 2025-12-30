#ifndef UI_VIEW_WAVE2_H
#define UI_VIEW_WAVE2_H

#include "lvgl.h"
#include "ecoflow_protocol.h"

void ui_view_wave2_init(lv_obj_t * parent);
void ui_view_wave2_update(Wave2DataStruct * data);
lv_obj_t * ui_view_wave2_get_screen(void);

#endif
