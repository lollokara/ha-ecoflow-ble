#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <Adafruit_DotStar.h>
#include "EcoflowData.h"
#include "DeviceManager.h"
#include "types.h"

// Initialize the display
void setupDisplay();

// Update the display content based on the data and state
void updateDisplay(const EcoflowData& data, DeviceSlot* activeSlot, bool isScanning);

// Handle button input and return an action if any
DisplayAction handleDisplayInput(ButtonInput input);

// Get pending actions (e.g. from timeouts)
DisplayAction getPendingAction();

// Getters for setting values
int getSetAcLimit();
int getSetMaxChgSoc();
int getSetMinDsgSoc();
DeviceType getTargetDeviceType();
int getSetW2Val();

#endif // DISPLAY_H
