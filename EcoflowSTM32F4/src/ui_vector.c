#include "ui_vector.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

// Helper: Cubic Bezier Interpolation
// We draw Bezier by discretizing it into lines
static void DrawBezier(float x0, float y0, float x1, float y1, float x2, float y2, float x3, float y3, uint32_t color) {
    float t;
    float prev_x = x0;
    float prev_y = y0;

    // Resolution: 10 segments
    for (int i = 1; i <= 10; i++) {
        t = (float)i / 10.0f;
        float u = 1 - t;
        float tt = t * t;
        float uu = u * u;
        float uuu = uu * u;
        float ttt = tt * t;

        float xt = uuu * x0 + 3 * uu * t * x1 + 3 * u * tt * x2 + ttt * x3;
        float yt = uuu * y0 + 3 * uu * t * y1 + 3 * u * tt * y2 + ttt * y3;

        BSP_LCD_DrawLine((uint16_t)prev_x, (uint16_t)prev_y, (uint16_t)xt, (uint16_t)yt);
        prev_x = xt;
        prev_y = yt;
    }
}

// Helper: Parse float from string
static const char* skip_spaces(const char* p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

static float parse_next_float(const char** ptr) {
    char* end;
    float val = strtof(*ptr, &end);
    *ptr = end;
    return val;
}

/**
 * @brief Draws a shape defined by a simple SVG Path string.
 *        Supports M, L, C, Z commands. Absolute coordinates only for simplicity.
 *        Coordinate space: 0-24 usually (MDI viewport), scaled by `scale`.
 */
void UI_DrawSVGPath(const char* path_data, int16_t x, int16_t y, float scale, uint32_t color) {
    BSP_LCD_SetTextColor(color);

    const char* p = path_data;
    float cx = 0, cy = 0; // Current cursor
    float start_x = 0, start_y = 0; // Start of current subpath

    // Skip initial whitespace
    p = skip_spaces(p);

    while (*p) {
        char cmd = *p++;

        switch (cmd) {
            case 'M': { // Move To (Absolute)
                p = skip_spaces(p);
                float nx = parse_next_float(&p);
                p = skip_spaces(p);
                if (*p == ',') p++; // Handle comma
                float ny = parse_next_float(&p);

                cx = x + nx * scale;
                cy = y + ny * scale;
                start_x = cx;
                start_y = cy;
                break;
            }
            case 'L': { // Line To (Absolute)
                p = skip_spaces(p);
                float nx = parse_next_float(&p);
                p = skip_spaces(p);
                 if (*p == ',') p++;
                float ny = parse_next_float(&p);

                float target_x = x + nx * scale;
                float target_y = y + ny * scale;

                BSP_LCD_DrawLine((uint16_t)cx, (uint16_t)cy, (uint16_t)target_x, (uint16_t)target_y);
                cx = target_x;
                cy = target_y;
                break;
            }
            case 'C': { // Cubic Bezier (Absolute)
                // C x1 y1, x2 y2, x y
                p = skip_spaces(p); float x1 = x + parse_next_float(&p) * scale;
                p = skip_spaces(p); if(*p==',') p++; float y1 = y + parse_next_float(&p) * scale;
                p = skip_spaces(p); if(*p==',') p++; float x2 = x + parse_next_float(&p) * scale;
                p = skip_spaces(p); if(*p==',') p++; float y2 = y + parse_next_float(&p) * scale;
                p = skip_spaces(p); if(*p==',') p++; float tx = x + parse_next_float(&p) * scale;
                p = skip_spaces(p); if(*p==',') p++; float ty = y + parse_next_float(&p) * scale;

                DrawBezier(cx, cy, x1, y1, x2, y2, tx, ty, color);
                cx = tx;
                cy = ty;
                break;
            }
            case 'Z': { // Close Path
                BSP_LCD_DrawLine((uint16_t)cx, (uint16_t)cy, (uint16_t)start_x, (uint16_t)start_y);
                cx = start_x;
                cy = start_y;
                break;
            }
            default:
                // If it's a number, it might be continuation of previous command (e.g., L x y x y)
                // But for simplicity, we assume explicit commands in our mocked data.
                if (!isspace((unsigned char)cmd)) {
                   // Skip unknown char
                }
                break;
        }
        p = skip_spaces(p);
    }
}
