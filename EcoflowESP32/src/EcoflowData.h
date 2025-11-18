#ifndef EcoflowData_h
#define EcoflowData_h

#include "Arduino.h"

struct EcoflowData {
    int batteryLevel;
    int inputPower;
    int outputPower;
    bool acOn;
    bool dcOn;
    bool usbOn;
};

#endif
