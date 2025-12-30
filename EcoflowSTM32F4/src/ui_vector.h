#ifndef UI_VECTOR_H
#define UI_VECTOR_H

#include "stm32469i_discovery_lcd.h"
#include <stdint.h>
#include <math.h>

// Vector Command Types
typedef enum {
    VCMD_MOVE_TO,
    VCMD_LINE_TO,
    VCMD_CUBIC_TO,
    VCMD_CLOSE_PATH
} VectorCmdType;

// Simple structure to parse SVG path strings roughly
// But better to define our own compressed format or use strings if speed allows.
// Let's use strings for flexibility: "M 10 10 L 50 10 ..."

// Scale factor for coordinates
#define VEC_SCALE_ONE 1.0f

void UI_DrawSVGPath(const char* path_data, int16_t x, int16_t y, float scale, uint32_t color);

#endif // UI_VECTOR_H
