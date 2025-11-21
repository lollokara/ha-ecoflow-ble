#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <Adafruit_DotStar.h>
#include "EcoflowData.h"

enum class DisplayAction {
    NONE,
    TOGGLE_AC,
    TOGGLE_DC,
    TOGGLE_USB,
    SET_AC_LIMIT,
    SET_MAX_CHG,
    SET_MIN_DSG
};

enum class ButtonInput {
    BTN_UP,
    BTN_DOWN,
    BTN_ENTER_SHORT,
    BTN_ENTER_LONG,
    BTN_ENTER_VERY_LONG // 3s Hold
};

void setupDisplay();
void updateDisplay(const EcoflowData& data);
DisplayAction handleDisplayInput(ButtonInput input);
DisplayAction getPendingAction();

// Accessor for set values
int getSetAcLimit();
int getSetMaxCharge();
int getSetMinDischarge();

#endif
