#include "CmdUtils.h"
#include <esp_log.h>

static const char* TAG = "CmdUtils";

void CmdUtils::processInput(String input) {
    input.trim();
    if (input.length() == 0) return;

    int spaceIndex = input.indexOf(' ');
    String cmd = (spaceIndex == -1) ? input : input.substring(0, spaceIndex);
    String args = (spaceIndex == -1) ? "" : input.substring(spaceIndex + 1);

    if (cmd.equalsIgnoreCase("help") || cmd.equalsIgnoreCase("-h")) {
        printHelp();
        return;
    }

    handleWave2Command(cmd, args);
}

void CmdUtils::printHelp() {
    Serial.println("Available Wave 2 Commands:");
    Serial.println("  set_ambient_light <status>      - 0x5C (0x01=on, 0x00=off)");
    Serial.println("  set_automatic_drain <enable>    - 0x59 (0x01=enable)");
    Serial.println("  set_beep_enabled <flag>         - 0x56 (0x01=on, 0x00=off)");
    Serial.println("  set_fan_speed <fan_gear>        - 0x5E (1 byte)");
    Serial.println("  set_main_mode <mode>            - 0x51 (1 byte)");
    Serial.println("  set_power_state <mode>          - 0x5B (0x01=on, 0x00=off)");
    Serial.println("  set_temperature <temp_value>    - 0x58 (1 byte)");
    Serial.println("  set_countdown_timer <status>    - 0x55 (Sends [0x00, 0x00, status])");
    Serial.println("  set_idle_screen_timeout <time>  - 0x54 (Sends [0x00, 0x00, time])");
    Serial.println("  set_sub_mode <sub_mode>         - 0x52 (1 byte)");
    Serial.println("  set_temperature_display_type <type> - 0x5D (1 byte)");
    Serial.println("  set_temperature_unit <unit>     - 0x53 (0x00 or 0x01)");
    Serial.println("");
    Serial.println("Usage: Enter command followed by hex value (e.g., 'set_main_mode 0x01')");
}

uint8_t CmdUtils::parseHexByte(String s) {
    s.trim();
    if (s.startsWith("0x") || s.startsWith("0X")) {
        s = s.substring(2);
    }
    return (uint8_t)strtol(s.c_str(), NULL, 16);
}

void CmdUtils::handleWave2Command(String cmd, String args) {
    EcoflowESP32* w2 = DeviceManager::getInstance().getDevice(DeviceType::WAVE_2);
    if (!w2 || !w2->isAuthenticated()) {
        Serial.println("Error: Wave 2 device not connected or authenticated.");
        return;
    }

    uint8_t val = parseHexByte(args);

    if (cmd.equalsIgnoreCase("set_ambient_light")) {
        w2->setAmbientLight(val);
    } else if (cmd.equalsIgnoreCase("set_automatic_drain")) {
        w2->setAutomaticDrain(val);
    } else if (cmd.equalsIgnoreCase("set_beep_enabled")) {
        w2->setBeep(val);
    } else if (cmd.equalsIgnoreCase("set_fan_speed")) {
        w2->setFanSpeed(val);
    } else if (cmd.equalsIgnoreCase("set_main_mode")) {
        w2->setMainMode(val);
    } else if (cmd.equalsIgnoreCase("set_power_state")) {
        w2->setPowerState(val);
    } else if (cmd.equalsIgnoreCase("set_temperature")) {
        w2->setTemperature(val);
    } else if (cmd.equalsIgnoreCase("set_countdown_timer")) {
        w2->setCountdownTimer(val);
    } else if (cmd.equalsIgnoreCase("set_idle_screen_timeout")) {
        w2->setIdleScreenTimeout(val);
    } else if (cmd.equalsIgnoreCase("set_sub_mode")) {
        w2->setSubMode(val);
    } else if (cmd.equalsIgnoreCase("set_temperature_display_type")) {
        w2->setTempDisplayType(val);
    } else if (cmd.equalsIgnoreCase("set_temperature_unit")) {
        w2->setTempUnit(val);
    } else {
        Serial.println("Unknown command. Type 'help' for list.");
        return;
    }
    Serial.println("Command sent.");
}
