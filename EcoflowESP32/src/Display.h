#ifndef DISPLAY_H
#define DISPLAY_H

#include <Arduino.h>
#include <Adafruit_DotStar.h>
#include <SPI.h>
#include "EcoflowData.h"

void setupDisplay();
void updateDisplay(const EcoflowData& data);

#endif
