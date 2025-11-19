#ifndef EcoflowData_h
#define EcoflowData_h

#include <cstdint>

struct EcoflowData {
    int batteryLevel = 0;
    int inputPower = 0;
    int outputPower = 0;
    int batteryVoltage = 0;
    int acVoltage = 0;
    int acFrequency = 0;
    bool acOn = false;
    bool dcOn = false;
    bool usbOn = false;
};

#endif // EcoflowData_h
