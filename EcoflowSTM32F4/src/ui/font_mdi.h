#ifndef FONT_MDI_H
#define FONT_MDI_H

#include "lvgl.h"

extern const lv_font_t font_mdi;

// Icon Definitions (Mapped to 0xF000+)
#define MDI_BATTERY_100 "\xef\x80\x80" // 0xF000
#define MDI_BATTERY_50 "\xef\x80\x81" // 0xF001
#define MDI_FLASH "\xef\x80\x82" // 0xF002
#define MDI_PLUG "\xef\x80\x83" // 0xF003
#define MDI_SOLAR "\xef\x80\x84" // 0xF004
#define MDI_USB "\xef\x80\x85" // 0xF005
#define MDI_POWER "\xef\x80\x86" // 0xF006
#define MDI_CHECK "\xef\x80\x87" // 0xF007
#define MDI_CLOSE "\xef\x80\x88" // 0xF008
#define MDI_WAVE "\xef\x80\x89" // 0xF009
#define MDI_HOME "\xef\x80\x8a" // 0xF00A
#define MDI_COG "\xef\x80\x8b" // 0xF00B

#endif
