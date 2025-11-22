#include "CmdUtils.h"
#include <esp_log.h>
#include <esp_idf_version.h>
#include <WiFi.h>

#if CONFIG_IDF_TARGET_ESP32S3
// Check IDF version for correct header
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include <driver/temperature_sensor.h>
#else
#include <driver/temp_sensor.h>
#endif
#endif

static const char* TAG = "CmdUtils";

// Helper to read internal temperature safely
static float readInternalTemp() {
#if CONFIG_IDF_TARGET_ESP32S3
    float tsens_out = 0.0f;

    #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        // New API (IDF v5+)
        temperature_sensor_handle_t temp_handle = NULL;
        temperature_sensor_config_t temp_sensor = {
            .range_min = 10,
            .range_max = 85,
        };
        if (temperature_sensor_install(&temp_sensor, &temp_handle) == ESP_OK) {
            temperature_sensor_enable(temp_handle);
            temperature_sensor_get_celsius(temp_handle, &tsens_out);
            temperature_sensor_disable(temp_handle);
            temperature_sensor_uninstall(temp_handle);
        } else {
            return -999.0f; // Error
        }
    #else
        // Legacy API (IDF v4.4)
        temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
        temp_sensor.dac_offset = TSENS_DAC_L2; // Set range
        temp_sensor_set_config(temp_sensor);
        temp_sensor_start();
        if (temp_sensor_read_celsius(&tsens_out) != ESP_OK) {
            return -999.0f;
        }
        temp_sensor_stop();
    #endif

    return tsens_out;
#else
    return -1.0f; // Not supported on this chip
#endif
}

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

    // System diagnostics
    if (cmd.startsWith("sys_")) {
        handleSysCommand(cmd);
        return;
    }

    // Connection Manager
    if (cmd.startsWith("con_") || cmd.startsWith("wifi_")) {
        handleConCommand(cmd, args);
        return;
    }

    // Try handling based on prefix or known commands
    if (cmd.startsWith("set_")) {
        // Wave 2 commands
        if (cmd.indexOf("ambient_light") > 0 || cmd.indexOf("fan_speed") > 0 || cmd.indexOf("temperature") > 0 || cmd.indexOf("sub_mode") > 0 || cmd.indexOf("main_mode") > 0 || cmd.indexOf("power_state") > 0 || cmd.indexOf("beep_enabled") > 0 || cmd.indexOf("automatic_drain") > 0 || cmd.indexOf("countdown_timer") > 0 || cmd.indexOf("idle_screen_timeout") > 0) {
            handleWave2Command(cmd, args);
            return;
        }
        handleWave2Command(cmd, args); // Fallback
    } else if (cmd.startsWith("d3_")) {
        if (cmd.startsWith("d3_get_")) handleDelta3Read(cmd);
        else handleDelta3Command(cmd, args);
    } else if (cmd.startsWith("d3p_")) {
        if (cmd.startsWith("d3p_get_")) handleDeltaPro3Read(cmd);
        else handleDeltaPro3Command(cmd, args);
    } else if (cmd.startsWith("ac_")) {
        if (cmd.startsWith("ac_get_")) handleAltChargerRead(cmd);
        else handleAltChargerCommand(cmd, args);
    } else if (cmd.startsWith("get_")) {
        // Generic Wave 2 gets
        handleWave2Read(cmd);
    } else {
        Serial.println("Unknown command. Type 'help' for list.");
    }
}


void CmdUtils::printHelp() {
    Serial.println("=== Available Commands ===");

    Serial.println("\n[System & Connection]");
    Serial.println("  sys_temp                        (Read internal ESP32 temp)");
    Serial.println("  sys_reset                       (Factory reset & reboot)");
    Serial.println("  con_status                      (List connections)");
    Serial.println("  con_connect <d3/w2/d3p/ac>      (Connect)");
    Serial.println("  con_disconnect <d3/w2/d3p/ac>   (Disconnect)");
    Serial.println("  con_forget <d3/w2/d3p/ac>       (Forget)");
    Serial.println("  wifi_set <ssid> <pass>          (Set WiFi credentials)");
    Serial.println("  wifi_ip                         (Get current IP)");

    Serial.println("\n[Wave 2] (get_ or set_)");
    Serial.println("  get_temp / set_temperature <val>");
    Serial.println("  get_fan_speed / set_fan_speed <val>");
    Serial.println("  get_mode / set_main_mode <val>");
    Serial.println("  get_power");
    Serial.println("  get_battery");
    Serial.println("  set_ambient_light <0/1>");
    // ... other writes omitted for brevity but exist

    Serial.println("\n[Delta 3] (Prefix: d3_)");
    Serial.println("  d3_get_switches / d3_set_ac/dc/usb <0/1>");
    Serial.println("  d3_get_power");
    Serial.println("  d3_get_battery / d3_set_soc_max/min");
    Serial.println("  d3_set_ac_limit <watts>");

    Serial.println("\n[Delta Pro 3] (Prefix: d3p_)");
    Serial.println("  d3p_get_switches / d3p_set_ac_hv/lv <0/1>");
    Serial.println("  d3p_get_power");
    Serial.println("  d3p_get_battery");
    Serial.println("  d3p_set_energy_backup <0/1>");

    Serial.println("\n[Alternator Charger] (Prefix: ac_)");
    Serial.println("  ac_get_status / ac_set_open <0/1>");
    Serial.println("  ac_get_power");
    Serial.println("  ac_get_battery");
    Serial.println("  ac_set_mode <val>");
    Serial.println("  ac_set_power_limit <watts>");
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

// --- System Handler ---

void CmdUtils::handleSysCommand(String cmd) {
    if (cmd.equalsIgnoreCase("sys_temp")) {
        float t = readInternalTemp();
        if (t > -900) Serial.printf("Internal Temp: %.2f C\n", t);
        else Serial.println("Failed to read internal temp (or not supported).");
    } else if (cmd.equalsIgnoreCase("sys_reset")) {
        Serial.println("Resetting settings and rebooting...");
        Preferences prefs;
        prefs.begin("ecoflow", false);
        prefs.clear();
        prefs.end();
        ESP.restart();
    } else {
        Serial.println("Unknown sys command.");
    }
}

void CmdUtils::handleConCommand(String cmd, String args) {
    DeviceManager& dm = DeviceManager::getInstance();

    if (cmd.equalsIgnoreCase("con_status")) {
        dm.printStatus();
        return;
    }

if (cmd.equalsIgnoreCase("wifi_ip")) {
    if (WiFi.status() == WL_CONNECTED) {
        IPAddress ip = WiFi.localIP();
        Serial.print("Current IP address: ");
        Serial.println(ip);
    } else {
        Serial.println("WiFi not connected.");
    }
    return;
}

if (cmd.equalsIgnoreCase("wifi_set")) {
    // Trim whitespace from the start
    args.trim();
    // Find first quote for SSID
    int firstQuote = args.indexOf('"');
    if (firstQuote == -1) {
        Serial.println("Usage: wifi_set \"<ssid>\" \"<pass>\"");
        return;
    }
    // Find closing quote for SSID
    int secondQuote = args.indexOf('"', firstQuote + 1);
    if (secondQuote == -1) {
        Serial.println("Usage: wifi_set \"<ssid>\" \"<pass>\"");
        return;
    }
    String ssid = args.substring(firstQuote + 1, secondQuote);
    // Find first quote for password (after ssid)
    int thirdQuote = args.indexOf('"', secondQuote + 1);
    if (thirdQuote == -1) {
        Serial.println("Usage: wifi_set \"<ssid>\" \"<pass>\"");
        return;
    }
    int fourthQuote = args.indexOf('"', thirdQuote + 1);
    if (fourthQuote == -1) {
        Serial.println("Usage: wifi_set \"<ssid>\" \"<pass>\"");
        return;
    }
    String pass = args.substring(thirdQuote + 1, fourthQuote);
    Serial.print("Saved SSID: ");
    Serial.print(ssid);
    Serial.print("  Password: ");
    Serial.println(pass);
    Preferences prefs;
    prefs.begin("ecoflow", false);
    prefs.putString("wifi_ssid", ssid);
    prefs.putString("wifi_pass", pass);
    prefs.end();
    Serial.println("WiFi credentials saved. Reboot to connect.");
    return;
}

    DeviceType type;
    args.trim();
    if (args.equalsIgnoreCase("d3")) type = DeviceType::DELTA_3;
    else if (args.equalsIgnoreCase("w2")) type = DeviceType::WAVE_2;
    else if (args.equalsIgnoreCase("d3p")) type = DeviceType::DELTA_PRO_3;
    else if (args.equalsIgnoreCase("ac") || args.equalsIgnoreCase("chg")) type = DeviceType::ALTERNATOR_CHARGER;
    else {
        Serial.println("Invalid device type. Use d3, w2, d3p, or ac.");
        return;
    }

    if (cmd.equalsIgnoreCase("con_connect")) dm.scanAndConnect(type);
    else if (cmd.equalsIgnoreCase("con_disconnect")) dm.disconnect(type);
    else if (cmd.equalsIgnoreCase("con_forget")) dm.forget(type);
    else Serial.println("Unknown connection command.");
}

// --- Write Handlers ---

void CmdUtils::handleWave2Command(String cmd, String args) {
    EcoflowESP32* w2 = DeviceManager::getInstance().getDevice(DeviceType::WAVE_2);
    if (!w2 || !w2->isAuthenticated()) {
        // Warn if desired
    }
    uint8_t val = parseHexByte(args);

    if (cmd.equalsIgnoreCase("set_ambient_light")) { if(w2) w2->setAmbientLight(val); }
    else if (cmd.equalsIgnoreCase("set_automatic_drain")) { if(w2) w2->setAutomaticDrain(val); }
    else if (cmd.equalsIgnoreCase("set_beep_enabled")) { if(w2) w2->setBeep(val); }
    else if (cmd.equalsIgnoreCase("set_fan_speed")) { if(w2) w2->setFanSpeed(val); }
    else if (cmd.equalsIgnoreCase("set_main_mode")) { if(w2) w2->setMainMode(val); }
    else if (cmd.equalsIgnoreCase("set_power_state")) { if(w2) w2->setPowerState(val); }
    else if (cmd.equalsIgnoreCase("set_temperature")) { if(w2) w2->setTemperature(val); }
    else if (cmd.equalsIgnoreCase("set_countdown_timer")) { if(w2) w2->setCountdownTimer(val); }
    else if (cmd.equalsIgnoreCase("set_idle_screen_timeout")) { if(w2) w2->setIdleScreenTimeout(val); }
    else if (cmd.equalsIgnoreCase("set_sub_mode")) { if(w2) w2->setSubMode(val); }
    else if (cmd.equalsIgnoreCase("set_temperature_display_type")) { if(w2) w2->setTempDisplayType(val); }
    else if (cmd.equalsIgnoreCase("set_temperature_unit")) { if(w2) w2->setTempUnit(val); }
}

void CmdUtils::handleDelta3Command(String cmd, String args) {
    EcoflowESP32* d3 = DeviceManager::getInstance().getDevice(DeviceType::DELTA_3);
    if (!d3 || !d3->isAuthenticated()) { Serial.println("D3 not ready."); return; }

    if (cmd.equalsIgnoreCase("d3_set_ac")) d3->setAC(parseHexByte(args));
    else if (cmd.equalsIgnoreCase("d3_set_dc")) d3->setDC(parseHexByte(args));
    else if (cmd.equalsIgnoreCase("d3_set_usb")) d3->setUSB(parseHexByte(args));
    else if (cmd.equalsIgnoreCase("d3_set_ac_limit")) d3->setAcChargingLimit(args.toInt());
    else if (cmd.equalsIgnoreCase("d3_set_soc_max")) d3->setBatterySOCLimits(args.toInt(), d3->getMinDsgSoc());
    else if (cmd.equalsIgnoreCase("d3_set_soc_min")) d3->setBatterySOCLimits(d3->getMaxChgSoc(), args.toInt());
}

void CmdUtils::handleDeltaPro3Command(String cmd, String args) {
    EcoflowESP32* d3p = DeviceManager::getInstance().getDevice(DeviceType::DELTA_PRO_3);
    if (!d3p || !d3p->isAuthenticated()) { Serial.println("D3P not ready."); return; }

    if (cmd.equalsIgnoreCase("d3p_set_ac_hv")) d3p->setAcHvPort(parseHexByte(args));
    else if (cmd.equalsIgnoreCase("d3p_set_ac_lv")) d3p->setAcLvPort(parseHexByte(args));
    else if (cmd.equalsIgnoreCase("d3p_set_energy_backup")) d3p->setEnergyBackup(parseHexByte(args));
    else if (cmd.equalsIgnoreCase("d3p_set_energy_backup_level")) d3p->setEnergyBackupLevel(args.toInt());
}

void CmdUtils::handleAltChargerCommand(String cmd, String args) {
    EcoflowESP32* ac = DeviceManager::getInstance().getDevice(DeviceType::ALTERNATOR_CHARGER);
    if (!ac || !ac->isAuthenticated()) { Serial.println("AC not ready."); return; }

    if (cmd.equalsIgnoreCase("ac_set_open")) ac->setChargerOpen(parseHexByte(args));
    else if (cmd.equalsIgnoreCase("ac_set_mode")) ac->setChargerMode(args.toInt());
    else if (cmd.equalsIgnoreCase("ac_set_power_limit")) ac->setPowerLimit(args.toInt());
    else if (cmd.equalsIgnoreCase("ac_set_batt_voltage")) ac->setBatteryVoltage(parseFloat(args));
    else if (cmd.equalsIgnoreCase("ac_set_car_chg_limit")) ac->setCarBatteryChargeLimit(parseFloat(args));
    else if (cmd.equalsIgnoreCase("ac_set_dev_chg_limit")) ac->setDeviceBatteryChargeLimit(parseFloat(args));
}

// --- Read Handlers ---

void CmdUtils::handleWave2Read(String cmd) {
    EcoflowESP32* w2 = DeviceManager::getInstance().getDevice(DeviceType::WAVE_2);
    if (!w2) return;
    const Wave2Data& d = w2->getData().wave2;

    if (cmd.equalsIgnoreCase("get_temp")) Serial.printf("Temp: Set=%d, Env=%.2f, Outlet=%.2f\n", d.setTemp, d.envTemp, d.outLetTemp);
    else if (cmd.equalsIgnoreCase("get_fan_speed")) Serial.printf("Fan Speed: %d\n", d.fanValue);
    else if (cmd.equalsIgnoreCase("get_mode")) Serial.printf("Mode: %d, SubMode: %d\n", d.mode, d.subMode);
    else if (cmd.equalsIgnoreCase("get_power")) Serial.printf("Power: Bat=%dW, MPPT=%dW, PSDR=%dW\n", d.batPwrWatt, d.mpptPwrWatt, d.psdrPwrWatt);
    else if (cmd.equalsIgnoreCase("get_battery")) Serial.printf("Batt: %d%% (Stat: %d), Rem: %dm\n", d.batSoc, d.batChgStatus, d.remainingTime);
    else if (cmd.equalsIgnoreCase("get_power_state")) Serial.printf("Power State: %d\n", d.powerMode);
    else Serial.println("Unknown get command for Wave 2");
}

void CmdUtils::handleDelta3Read(String cmd) {
    EcoflowESP32* d3 = DeviceManager::getInstance().getDevice(DeviceType::DELTA_3);
    if (!d3) return;
    const Delta3Data& d = d3->getData().delta3;

    if (cmd.equalsIgnoreCase("d3_get_switches")) Serial.printf("AC: %d, DC: %d, USB: %d\n", d.acOn, d.dcOn, d.usbOn);
    else if (cmd.equalsIgnoreCase("d3_get_power")) Serial.printf("In: %.1fW, Out: %.1fW, AC In: %.1fW, AC Out: %.1fW, Solar: %.1fW\n", d.inputPower, d.outputPower, d.acInputPower, d.acOutputPower, d.solarInputPower);
    else if (cmd.equalsIgnoreCase("d3_get_battery")) Serial.printf("Batt: %.1f%%, Limits: %d%%-%d%%\n", d.batteryLevel, d.batteryChargeLimitMin, d.batteryChargeLimitMax);
    else if (cmd.equalsIgnoreCase("d3_get_settings")) Serial.printf("AC Limit: %dW\n", d.acChargingSpeed);
    else Serial.println("Unknown get command for Delta 3");
}

void CmdUtils::handleDeltaPro3Read(String cmd) {
    EcoflowESP32* d3p = DeviceManager::getInstance().getDevice(DeviceType::DELTA_PRO_3);
    if (!d3p) return;
    const DeltaPro3Data& d = d3p->getData().deltaPro3;

    if (cmd.equalsIgnoreCase("d3p_get_switches")) Serial.printf("AC LV: %d, AC HV: %d, DC: %d\n", d.acLvPort, d.acHvPort, d.dc12vPort);
    else if (cmd.equalsIgnoreCase("d3p_get_power")) Serial.printf("In: %.1fW, Out: %.1fW, AC In: %.1fW, AC LV: %.1fW, AC HV: %.1fW\n", d.inputPower, d.outputPower, d.acInputPower, d.acLvOutputPower, d.acHvOutputPower);
    else if (cmd.equalsIgnoreCase("d3p_get_battery")) Serial.printf("Batt: %.1f%%, Limits: %d%%-%d%%\n", d.batteryLevel, d.batteryChargeLimitMin, d.batteryChargeLimitMax);
    else if (cmd.equalsIgnoreCase("d3p_get_energy_backup")) Serial.printf("Enabled: %d, Level: %d%%\n", d.energyBackup, d.energyBackupBatteryLevel);
    else Serial.println("Unknown get command for Delta Pro 3");
}

void CmdUtils::handleAltChargerRead(String cmd) {
    EcoflowESP32* ac = DeviceManager::getInstance().getDevice(DeviceType::ALTERNATOR_CHARGER);
    if (!ac) return;
    const AlternatorChargerData& d = ac->getData().alternatorCharger;

    if (cmd.equalsIgnoreCase("ac_get_status")) Serial.printf("Open: %d, Mode: %d\n", d.chargerOpen, d.chargerMode);
    else if (cmd.equalsIgnoreCase("ac_get_power")) Serial.printf("DC Power: %.1fW, Limit: %dW\n", d.dcPower, d.powerLimit);
    else if (cmd.equalsIgnoreCase("ac_get_battery")) Serial.printf("Dev Batt: %.1f%%, Car Batt: %.2fV\n", d.batteryLevel, d.carBatteryVoltage);
    else Serial.println("Unknown get command for Alternator Charger");
}
