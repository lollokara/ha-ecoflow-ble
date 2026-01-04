#ifndef TYPES_H
#define TYPES_H

#include <stdint.h>

// Enum values MUST match ecoflow_protocol.h defines:
// DEV_TYPE_DELTA_3 = 1
// DEV_TYPE_DELTA_PRO_3 = 2
// DEV_TYPE_WAVE_2 = 3
// DEV_TYPE_ALT_CHARGER = 4
enum class DeviceType {
    DELTA_3 = 1,
    DELTA_PRO_3 = 2,
    WAVE_2 = 3,
    ALTERNATOR_CHARGER = 4
};

enum class ButtonInput {
    NONE,
    BTN_UP,
    BTN_DOWN,
    BTN_ENTER_SHORT,
    BTN_ENTER_HOLD
};

enum class DisplayAction {
    NONE,
    CONNECT_DEVICE,
    DISCONNECT_DEVICE,
    TOGGLE_AC,
    TOGGLE_DC,
    TOGGLE_USB,
    SET_AC_LIMIT,
    SET_SOC_LIMITS,
    W2_TOGGLE_PWR,
    W2_SET_MODE,
    W2_SET_FAN,
    W2_SET_SUB_MODE,
    W2_SET_PWR,
    SYSTEM_OFF
};

#endif // TYPES_H
