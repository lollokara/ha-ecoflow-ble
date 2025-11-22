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

    // Try handling based on prefix or known commands
    if (cmd.startsWith("set_")) {
        // Wave 2 commands
        if (cmd.indexOf("ambient_light") > 0 || cmd.indexOf("fan_speed") > 0 || cmd.indexOf("temperature") > 0 || cmd.indexOf("sub_mode") > 0 || cmd.indexOf("main_mode") > 0 || cmd.indexOf("power_state") > 0 || cmd.indexOf("beep_enabled") > 0 || cmd.indexOf("automatic_drain") > 0 || cmd.indexOf("countdown_timer") > 0 || cmd.indexOf("idle_screen_timeout") > 0) {
            handleWave2Command(cmd, args);
            return;
        }

        // D3 / D3P / AltChg Commands - try all if specific commands overlap?
        // Let's split by device prefix if possible, or try to match unique commands.
        // Or check if a device is connected.

        // Simpler: Try to execute on all relevant connected devices? No, that's dangerous.
        // User didn't specify a way to select target device in CLI.
        // I'll implement prefixes: d3_set_..., d3p_set_..., ac_set_...
        // AND keep generic ones for Wave 2 as requested originally.

        if (cmd.startsWith("d3_")) {
            handleDelta3Command(cmd, args);
            return;
        }
        if (cmd.startsWith("d3p_")) {
            handleDeltaPro3Command(cmd, args);
            return;
        }
        if (cmd.startsWith("ac_")) {
            handleAltChargerCommand(cmd, args);
            return;
        }

        // Fallback for original Wave 2 commands which didn't have prefix
        handleWave2Command(cmd, args);
    } else {
        Serial.println("Unknown command. Type 'help' for list.");
    }
}

void CmdUtils::printHelp() {
    Serial.println("=== Available Commands ===");

    Serial.println("\n[Wave 2]");
    Serial.println("  set_ambient_light <status>      (0x5C)");
    Serial.println("  set_automatic_drain <enable>    (0x59)");
    Serial.println("  set_beep_enabled <flag>         (0x56)");
    Serial.println("  set_fan_speed <fan_gear>        (0x5E)");
    Serial.println("  set_main_mode <mode>            (0x51)");
    Serial.println("  set_power_state <mode>          (0x5B)");
    Serial.println("  set_temperature <temp_value>    (0x58)");
    Serial.println("  set_countdown_timer <status>    (0x55)");
    Serial.println("  set_idle_screen_timeout <time>  (0x54)");
    Serial.println("  set_sub_mode <sub_mode>         (0x52)");
    Serial.println("  set_temperature_display_type <type> (0x5D)");
    Serial.println("  set_temperature_unit <unit>     (0x53)");

    Serial.println("\n[Delta 3] (Prefix: d3_)");
    Serial.println("  d3_set_ac <0/1>");
    Serial.println("  d3_set_dc <0/1>");
    Serial.println("  d3_set_usb <0/1>");
    Serial.println("  d3_set_ac_limit <watts>");
    Serial.println("  d3_set_soc_max <%>");
    Serial.println("  d3_set_soc_min <%>");

    Serial.println("\n[Delta Pro 3] (Prefix: d3p_)");
    Serial.println("  d3p_set_ac_hv <0/1>");
    Serial.println("  d3p_set_ac_lv <0/1>");
    Serial.println("  d3p_set_energy_backup <0/1>");
    Serial.println("  d3p_set_energy_backup_level <%>");

    Serial.println("\n[Alternator Charger] (Prefix: ac_)");
    Serial.println("  ac_set_open <0/1>");
    Serial.println("  ac_set_mode <mode>");
    Serial.println("  ac_set_power_limit <watts>");
    Serial.println("  ac_set_batt_voltage <volts>");
    Serial.println("  ac_set_car_chg_limit <amps>");
    Serial.println("  ac_set_dev_chg_limit <amps>");
}

uint8_t CmdUtils::parseHexByte(String s) {
    s.trim();
    if (s.startsWith("0x") || s.startsWith("0X")) {
        s = s.substring(2);
    }
    return (uint8_t)strtol(s.c_str(), NULL, 16);
}

float CmdUtils::parseFloat(String s) {
    return s.toFloat();
}

void CmdUtils::handleWave2Command(String cmd, String args) {
    EcoflowESP32* w2 = DeviceManager::getInstance().getDevice(DeviceType::WAVE_2);
    if (!w2 || !w2->isAuthenticated()) {
        // Only warn if we are sure it's a Wave 2 command, but since we fallback here, silent fail or check cmd validity first
        // But for user feedback:
        // Serial.println("Wave 2 not connected.");
        // We will proceed but if commands are specific they will fail inside EcoflowESP32 if checks exist,
        // actually existing checks in EcoflowESP32 just return/fail silently usually.
        // Let's keep the check.
    }

    uint8_t val = parseHexByte(args);

    if (cmd.equalsIgnoreCase("set_ambient_light")) {
        if(w2) w2->setAmbientLight(val);
    } else if (cmd.equalsIgnoreCase("set_automatic_drain")) {
        if(w2) w2->setAutomaticDrain(val);
    } else if (cmd.equalsIgnoreCase("set_beep_enabled")) {
        if(w2) w2->setBeep(val);
    } else if (cmd.equalsIgnoreCase("set_fan_speed")) {
        if(w2) w2->setFanSpeed(val);
    } else if (cmd.equalsIgnoreCase("set_main_mode")) {
        if(w2) w2->setMainMode(val);
    } else if (cmd.equalsIgnoreCase("set_power_state")) {
        if(w2) w2->setPowerState(val);
    } else if (cmd.equalsIgnoreCase("set_temperature")) {
        if(w2) w2->setTemperature(val);
    } else if (cmd.equalsIgnoreCase("set_countdown_timer")) {
        if(w2) w2->setCountdownTimer(val);
    } else if (cmd.equalsIgnoreCase("set_idle_screen_timeout")) {
        if(w2) w2->setIdleScreenTimeout(val);
    } else if (cmd.equalsIgnoreCase("set_sub_mode")) {
        if(w2) w2->setSubMode(val);
    } else if (cmd.equalsIgnoreCase("set_temperature_display_type")) {
        if(w2) w2->setTempDisplayType(val);
    } else if (cmd.equalsIgnoreCase("set_temperature_unit")) {
        if(w2) w2->setTempUnit(val);
    }
}

void CmdUtils::handleDelta3Command(String cmd, String args) {
    EcoflowESP32* d3 = DeviceManager::getInstance().getDevice(DeviceType::DELTA_3);
    if (!d3 || !d3->isAuthenticated()) {
        Serial.println("D3 not ready.");
        return;
    }

    if (cmd.equalsIgnoreCase("d3_set_ac")) d3->setAC(parseHexByte(args));
    else if (cmd.equalsIgnoreCase("d3_set_dc")) d3->setDC(parseHexByte(args));
    else if (cmd.equalsIgnoreCase("d3_set_usb")) d3->setUSB(parseHexByte(args));
    else if (cmd.equalsIgnoreCase("d3_set_ac_limit")) d3->setAcChargingLimit(args.toInt());
    else if (cmd.equalsIgnoreCase("d3_set_soc_max")) d3->setBatterySOCLimits(args.toInt(), d3->getMinDsgSoc()); // Preserves other limit? ideally yes but getMinDsgSoc accesses cached data
    else if (cmd.equalsIgnoreCase("d3_set_soc_min")) d3->setBatterySOCLimits(d3->getMaxChgSoc(), args.toInt());
}

void CmdUtils::handleDeltaPro3Command(String cmd, String args) {
    EcoflowESP32* d3p = DeviceManager::getInstance().getDevice(DeviceType::DELTA_PRO_3);
    if (!d3p || !d3p->isAuthenticated()) {
        Serial.println("D3P not ready.");
        return;
    }

    if (cmd.equalsIgnoreCase("d3p_set_ac_hv")) d3p->setAcHvPort(parseHexByte(args));
    else if (cmd.equalsIgnoreCase("d3p_set_ac_lv")) d3p->setAcLvPort(parseHexByte(args));
    else if (cmd.equalsIgnoreCase("d3p_set_energy_backup")) d3p->setEnergyBackup(parseHexByte(args));
    else if (cmd.equalsIgnoreCase("d3p_set_energy_backup_level")) d3p->setEnergyBackupLevel(args.toInt());
}

void CmdUtils::handleAltChargerCommand(String cmd, String args) {
    EcoflowESP32* ac = DeviceManager::getInstance().getDevice(DeviceType::ALTERNATOR_CHARGER);
    if (!ac || !ac->isAuthenticated()) {
        Serial.println("AC not ready.");
        return;
    }

    if (cmd.equalsIgnoreCase("ac_set_open")) ac->setChargerOpen(parseHexByte(args));
    else if (cmd.equalsIgnoreCase("ac_set_mode")) ac->setChargerMode(args.toInt());
    else if (cmd.equalsIgnoreCase("ac_set_power_limit")) ac->setPowerLimit(args.toInt());
    else if (cmd.equalsIgnoreCase("ac_set_batt_voltage")) ac->setBatteryVoltage(parseFloat(args));
    else if (cmd.equalsIgnoreCase("ac_set_car_chg_limit")) ac->setCarBatteryChargeLimit(parseFloat(args));
    else if (cmd.equalsIgnoreCase("ac_set_dev_chg_limit")) ac->setDeviceBatteryChargeLimit(parseFloat(args));
}
