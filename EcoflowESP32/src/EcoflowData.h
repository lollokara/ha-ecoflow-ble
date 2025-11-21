#ifndef ECOFLOW_DATA_H
#define ECOFLOW_DATA_H

#include <stdint.h>

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

    // New fields
    int solarInputPower = 0;
    int acOutputPower = 0;
    int dcOutputPower = 0;
    int cellTemperature = 0;

    // Wave 2 Specifics
    int currentTemp = 0;
    int setTemp = 0;
    int fanSpeed = 0;
    int mode = 0;
    int subMode = 0;
    int remainingTime = 0; // Battery discharge/charge remaining in mins

    // Connection Status
    bool isConnected = false;
};

#endif // ECOFLOW_DATA_H
