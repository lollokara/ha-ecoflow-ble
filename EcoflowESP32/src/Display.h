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
    SET_AC_LIMIT
};

enum class ButtonInput {
    BTN_UP,
    BTN_DOWN,
    BTN_ENTER_SHORT,
    BTN_ENTER_LONG
};

void setupDisplay();
void updateDisplay(const EcoflowData& data);
DisplayAction handleDisplayInput(ButtonInput input);
DisplayAction getPendingAction();

// Accessor for the new set value
int getSetAcLimit();

#endif
