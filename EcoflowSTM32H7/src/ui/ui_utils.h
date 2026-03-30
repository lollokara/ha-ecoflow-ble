#ifndef UI_UTILS_H
#define UI_UTILS_H

#include <stdint.h>
#include <string.h>
#include <math.h>

/**
 * @brief Safely reads a float from a packed structure (unaligned access safe).
 * @param ptr Pointer to the float member.
 * @return The float value.
 */
static inline float get_float_aligned(const float *ptr) {
    float val;
    memcpy(&val, ptr, sizeof(float));
    return val;
}

/**
 * @brief Safely reads an int32 from a packed structure (unaligned access safe).
 * @param ptr Pointer to the int32 member.
 * @return The int32 value.
 */
static inline int32_t get_int32_aligned(const int32_t *ptr) {
    int32_t val;
    memcpy(&val, ptr, sizeof(int32_t));
    return val;
}

/**
 * @brief Safely casts float to int, handling NaN/Inf.
 */
static inline int safe_float_to_int(float f) {
    if (isnan(f) || isinf(f)) return 0;
    return (int)f;
}

#endif // UI_UTILS_H
