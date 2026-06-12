#ifndef CMD_UTILS_H
#define CMD_UTILS_H

#include <Arduino.h>

class CmdUtils {
public:
    static void processInput(String input);
private:
    static void printHelp();
};

#endif
