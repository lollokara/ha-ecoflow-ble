#ifndef EcoflowData_h
#define EcoflowData_h

#include <stdint.h>

struct EcoflowData {
  uint8_t batteryLevel = 0;        // %
  uint16_t inputPower = 0;         // W
  uint16_t outputPower = 0;        // W
  uint16_t batteryVoltage = 0;     // V or 0.1V steps depending on protocol
  uint16_t acVoltage = 0;          // V
  uint16_t acFrequency = 0;        // Hz
  bool acOn = false;
  bool dcOn = false;
  bool usbOn = false;
};

#endif
