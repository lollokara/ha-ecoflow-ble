#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/

#define LV_COLOR_DEPTH 32

/*=========================
   MEMORY SETTINGS
 *=========================*/

#define LV_USE_STDLIB_MALLOC    LV_STDLIB_CLIB
#define LV_USE_STDLIB_STRING    LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF   LV_STDLIB_CLIB

#define LV_MEM_SIZE (128 * 1024U)
#define LV_MEM_ADR 0

/*====================
   HAL SETTINGS
 *====================*/

#define LV_DEF_REFR_PERIOD  33
#define LV_INDEV_DEF_READ_PERIOD 33

#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "task.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (xTaskGetTickCount())

/*====================
   FEATURE CONFIGURATION
 *====================*/

#define LV_DRAW_COMPLEX 1
#define LV_USE_DRAW_DMA2D 1

/*-------------
 * Others
 *-----------*/

#define LV_USE_LOG 0
#define LV_USE_ASSERT_NULL          1
#define LV_USE_ASSERT_MEM           1
#define LV_USE_SYSMON               0
#define LV_USE_PERF_MONITOR         0

/*====================
   FONT USAGE
 *====================*/

#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 1

/*=================
 *  TEXT SETTINGS
 *=================*/

#define LV_TXT_ENC LV_TXT_ENC_UTF8

#endif /*LV_CONF_H*/
