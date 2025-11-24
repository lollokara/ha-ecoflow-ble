#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <Adafruit_DotStar.h>
#include "EcoflowData.h"
#include "DeviceManager.h"
#include "types.h"

void setupDisplay();
void updateDisplay(const EcoflowData& currentData, DeviceSlot* activeSlot, bool isScanning);
DisplayAction handleDisplayInput(ButtonInput input);
DisplayAction getPendingAction();

// Power Management
void initPowerLatch();
void powerOff();

// Settings & Light Sensor
void loadSettings();
void setLightSensorLimits(int min, int max);
void getLightSensorLimits(int& min, int& max);
int getRawLightADC();

// Accessor for the new set value
int getSetAcLimit();
int getSetMaxChgSoc();
int getSetMinDsgSoc();
DeviceType getTargetDeviceType();

#endif
