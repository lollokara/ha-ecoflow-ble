#ifndef LV_CONF_H
#define LV_CONF_H

#include <stdint.h>

/*====================
   COLOR SETTINGS
 *====================*/
#define LV_COLOR_DEPTH 32
#define LV_COLOR_16_SWAP 0

/*=========================
   MEMORY SETTINGS
 *=========================*/
#define LV_MEM_CUSTOM 0
#define LV_MEM_SIZE (128 * 1024U)          /*[bytes]*/
#define LV_MEM_ADR 0     /*0: use malloc, 1: use custom pointer*/

/*====================
   HAL SETTINGS
 *====================*/
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "FreeRTOS.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (xTaskGetTickCount())

/*=======================
 * FEATURE CONFIGURATION
 *=======================*/
#define LV_USE_GPU_STM32_DMA2D 1
#define LV_GPU_DMA2D_CMSIS_INCLUDE "stm32f4xx.h"

#define LV_USE_LOG 1
#define LV_LOG_LEVEL LV_LOG_LEVEL_WARN

#define LV_USE_FONT_COMPRESSED 1
#define LV_FONT_MONTSERRAT_12 1
#define LV_FONT_MONTSERRAT_14 1
#define LV_FONT_MONTSERRAT_16 1
#define LV_FONT_MONTSERRAT_20 1
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_28 1

#define LV_USE_THEME_DEFAULT 1
#define LV_THEME_DEFAULT_DARK 0

/*==================
 * EXAMPLES
 *==================*/
#define LV_BUILD_EXAMPLES 0

#endif /*LV_CONF_H*/
