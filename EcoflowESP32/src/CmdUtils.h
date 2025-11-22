#ifndef CMD_UTILS_H
#define CMD_UTILS_H

#include <Arduino.h>
#include <vector>
#include "EcoflowESP32.h"
#include "DeviceManager.h"

class CmdUtils {
public:
    static void processInput(String input);

private:
    static void printHelp();
    static uint8_t parseHexByte(String s);
    static void handleWave2Command(String cmd, String args);
};

#endif // CMD_UTILS_H
