#ifndef ECOFLOW_DATA_H
#define ECOFLOW_DATA_H

#include <stdint.h>

struct EcoflowData {
    int batteryLevel = 0;
    int inputPower = 0;
    int outputPower = 0;
    float batteryVoltage = 0;
    int acVoltage = 0;
    int acFrequency = 0;
    float solarInputPower = 0;
    float acOutputPower = 0;
    float dcOutputPower = 0;
    float cellTemperature = 0;
    bool acOn = false;
    bool dcOn = false;
    bool usbOn = false;
};

#endif // ECOFLOW_DATA_H
