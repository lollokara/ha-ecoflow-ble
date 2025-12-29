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
#define LV_MEM_SIZE (64 * 1024U)

/*====================
   HAL SETTINGS
 *====================*/
#define LV_TICK_CUSTOM 1
#define LV_TICK_CUSTOM_INCLUDE "FreeRTOS.h"
#include "task.h"
#define LV_TICK_CUSTOM_SYS_TIME_EXPR (xTaskGetTickCount())

/*=======================
 * FEATURE CONFIGURATION
 *=======================*/
#define LV_USE_LOG 0

/*=====================
 *  COMPILER SETTINGS
 *====================*/
#define LV_ATTRIBUTE_LARGE_CONST

#endif /*LV_CONF_H*/
