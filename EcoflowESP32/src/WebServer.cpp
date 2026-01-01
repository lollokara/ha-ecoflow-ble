#include "WebServer.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <esp_log.h>
#include <LittleFS.h>
#include "Stm32Serial.h"

static const char* TAG = "WebServer";
AsyncWebServer WebServer::server(80);

// Global OTA State
static int ota_progress = 0;
static int ota_state = 0; // 0=Idle, 1=Uploading, 2=Flashing, 3=Done, 4=Error
static String ota_msg = "";

// Helper to read internal temperature safely
#include <esp_idf_version.h>
#if CONFIG_IDF_TARGET_ESP32S3
#if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
#include <driver/temperature_sensor.h>
#else
#include <driver/temp_sensor.h>
#endif
#endif

static float get_esp_temp() {
#if CONFIG_IDF_TARGET_ESP32S3
    float tsens_out = 0.0f;
    #if ESP_IDF_VERSION >= ESP_IDF_VERSION_VAL(5, 0, 0)
        temperature_sensor_handle_t temp_handle = NULL;
        temperature_sensor_config_t temp_sensor = { .range_min = 10, .range_max = 85 };
        if (temperature_sensor_install(&temp_sensor, &temp_handle) == ESP_OK) {
            temperature_sensor_enable(temp_handle);
            temperature_sensor_get_celsius(temp_handle, &tsens_out);
            temperature_sensor_disable(temp_handle);
            temperature_sensor_uninstall(temp_handle);
        }
    #else
        temp_sensor_config_t temp_sensor = TSENS_CONFIG_DEFAULT();
        temp_sensor.dac_offset = TSENS_DAC_L2;
        temp_sensor_set_config(temp_sensor);
        temp_sensor_start();
        temp_sensor_read_celsius(&tsens_out);
        temp_sensor_stop();
    #endif
    return tsens_out;
#else
    return 0.0f;
#endif
}

void WebServer::begin() {
    Preferences prefs;
    prefs.begin("ecoflow", true);
    String ssid = prefs.getString("wifi_ssid", "");
    String pass = prefs.getString("wifi_pass", "");
    prefs.end();

    if (ssid.length() > 0) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid.c_str(), pass.c_str());
        ESP_LOGI(TAG, "Connecting to WiFi: %s", ssid.c_str());
    } else {
        ESP_LOGW(TAG, "No WiFi credentials saved. Web UI disabled.");
        return;
    }

    setupRoutes();
    server.begin();
}

void WebServer::setupRoutes() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", WEB_APP_HTML);
    });

    server.on("/api/status", HTTP_GET, handleStatus);
    server.on("/api/control", HTTP_POST, [](AsyncWebServerRequest *r){}, NULL, handleControl);
    server.on("/api/connect", HTTP_POST, [](AsyncWebServerRequest *r){}, NULL, handleConnect);
    server.on("/api/disconnect", HTTP_POST, [](AsyncWebServerRequest *r){}, NULL, handleDisconnect);
    server.on("/api/forget", HTTP_POST, [](AsyncWebServerRequest *r){}, NULL, handleForget);

    server.on("/api/history", HTTP_GET, handleHistory);

    server.on("/api/logs", HTTP_GET, handleLogs);
    server.on("/api/log_config", HTTP_POST, [](AsyncWebServerRequest *r){}, NULL, handleLogConfig);
    server.on("/api/raw_command", HTTP_POST, [](AsyncWebServerRequest *r){}, NULL, handleRawCommand);

    server.on("/api/settings", HTTP_GET, handleSettings);
    server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *r){}, NULL, handleSettingsSave);

    // OTA Routes
    server.on("/api/update/status", HTTP_GET, handleUpdateStatus);

    server.on("/api/update/esp32", HTTP_POST, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        ESP.restart();
    }, handleUpdateEsp32);

    server.on("/api/update/stm32", HTTP_POST, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "Upload OK");
    }, handleUpdateStm32);
}

// ... (Existing Handlers: Status, History, Control, Connect, Disconnect, Forget, Logs, LogConfig, RawCommand, Settings, SettingsSave) ...
// Copying existing handlers here to keep file complete
// (For brevity in tool response, I will assume existing handlers are unchanged,
//  but I must output the full file content to overwrite correctly.
//  I'll include them from previous `read_file` output.)

void WebServer::handleStatus(AsyncWebServerRequest *request) {
    DynamicJsonDocument doc(4096);
    doc["esp_temp"] = get_esp_temp();
    doc["light_adc"] = LightSensor::getInstance().getRaw();

    auto fillCommon = [](JsonObject& obj, DeviceSlot* slot, EcoflowESP32* dev) {
        obj["connected"] = slot->isConnected;
        obj["sn"] = slot->serialNumber.c_str();
        obj["name"] = slot->name.c_str();
        obj["paired"] = (slot->serialNumber.length() > 0);
        obj["batt"] = dev->getBatteryLevel();
    };

    // Delta 3
    {
        DeviceSlot* s = DeviceManager::getInstance().getSlot(DeviceType::DELTA_3);
        EcoflowESP32* d = s->instance;
        if (s->isConnected || s->serialNumber.length() > 0) {
            JsonObject obj = doc.createNestedObject("d3");
            fillCommon(obj, s, d);
            if (s->isConnected) {
                const auto& data = d->getData().delta3;
                obj["in"] = d->getInputPower();
                obj["out"] = d->getOutputPower();
                obj["solar"] = d->getSolarInputPower();
                obj["ac_on"] = d->isAcOn();
                obj["dc_on"] = d->isDcOn();
                obj["usb_on"] = d->isUsbOn();
                obj["cfg_ac_lim"] = d->getAcChgLimit();
                obj["cfg_max"] = d->getMaxChgSoc();
                obj["cfg_min"] = d->getMinDsgSoc();
                obj["cell_temp"] = d->getCellTemperature();
                obj["ac_out_pow"] = (int)abs(data.acOutputPower);
                obj["dc_out_pow"] = (int)abs(data.dc12vOutputPower);
                obj["usb_out_pow"] = (int)(abs(data.usbcOutputPower) + abs(data.usbc2OutputPower) + abs(data.usbaOutputPower) + abs(data.usba2OutputPower));
            }
        }
    }
    // Wave 2
    {
        DeviceSlot* s = DeviceManager::getInstance().getSlot(DeviceType::WAVE_2);
        EcoflowESP32* d = s->instance;
        if (s->isConnected || s->serialNumber.length() > 0) {
            JsonObject obj = doc.createNestedObject("w2");
            fillCommon(obj, s, d);
            if (s->isConnected) {
                const auto& data = d->getData().wave2;
                obj["amb_temp"] = (int)data.envTemp;
                obj["out_temp"] = (int)data.outLetTemp;
                obj["set_temp"] = (int)data.setTemp;
                obj["mode"] = (int)data.mode;
                obj["sub_mode"] = (int)data.subMode;
                obj["fan"] = (int)data.fanValue;
                obj["pwr"] = (data.powerMode == 1);
                obj["drain"] = (data.wteFthEn != 0);
                obj["light"] = (data.rgbState != 0);
                obj["beep"] = (data.beepEnable != 0);
                obj["pwr_bat"] = (int)data.batPwrWatt;
                obj["pwr_mppt"] = (int)data.mpptPwrWatt;
                obj["pwr_psdr"] = (int)data.psdrPwrWatt;
            }
        }
    }
    // Delta Pro 3
    {
        DeviceSlot* s = DeviceManager::getInstance().getSlot(DeviceType::DELTA_PRO_3);
        EcoflowESP32* d = s->instance;
        if (s->isConnected || s->serialNumber.length() > 0) {
            JsonObject obj = doc.createNestedObject("d3p");
            fillCommon(obj, s, d);
            if (s->isConnected) {
                const auto& data = d->getData().deltaPro3;
                obj["in"] = d->getInputPower();
                obj["out"] = d->getOutputPower();
                obj["solar"] = d->getSolarInputPower();
                obj["ac_on"] = data.acHvPort; // Map to HV for UI consistency
                obj["dc_on"] = data.dc12vPort;
                obj["backup_en"] = data.energyBackup;
                obj["backup_lvl"] = data.energyBackupBatteryLevel;
                obj["cell_temp"] = data.cellTemperature;
                obj["cfg_max"] = d->getMaxChgSoc();
                obj["cfg_min"] = d->getMinDsgSoc();
                obj["cfg_ac_lim"] = d->getAcChgLimit();
                obj["gfi_mode"] = data.gfiMode;
                obj["ac_out_pow"] = (int)(data.acLvOutputPower + data.acHvOutputPower);
                obj["dc_out_pow"] = (int)data.dc12vOutputPower;
            }
        }
    }
    // Alternator Charger
    {
        DeviceSlot* s = DeviceManager::getInstance().getSlot(DeviceType::ALTERNATOR_CHARGER);
        EcoflowESP32* d = s->instance;
        if (s->isConnected || s->serialNumber.length() > 0) {
            JsonObject obj = doc.createNestedObject("ac");
            fillCommon(obj, s, d);
            if (s->isConnected) {
                const auto& data = d->getData().alternatorCharger;
                obj["chg_open"] = data.chargerOpen;
                obj["mode"] = data.chargerMode;
                obj["pow_lim"] = data.powerLimit;
                obj["car_volt"] = data.carBatteryVoltage;
            }
        }
    }
    String json;
    serializeJson(doc, json);
    request->send(200, "application/json", json);
}

void WebServer::handleHistory(AsyncWebServerRequest *request) {
    if (request->hasParam("type")) {
        String type = request->getParam("type")->value();
        std::vector<int> hist;
        if (type == "w2") hist = DeviceManager::getInstance().getWave2TempHistory();
        else if (type == "d3") hist = DeviceManager::getInstance().getSolarHistory(DeviceType::DELTA_3);
        else if (type == "d3p") hist = DeviceManager::getInstance().getSolarHistory(DeviceType::DELTA_PRO_3);
        else { request->send(400, "text/plain", "Invalid Type"); return; }
        DynamicJsonDocument doc(2048);
        JsonArray arr = doc.to<JsonArray>();
        for(int t : hist) arr.add(t);
        String json; serializeJson(doc, json);
        request->send(200, "application/json", json);
    } else { request->send(400, "text/plain", "Missing Type"); }
}

void WebServer::handleControl(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    StaticJsonDocument<512> doc; deserializeJson(doc, data, len);
    String typeStr = doc["type"]; String cmd = doc["cmd"];
    DeviceType type = DeviceType::DELTA_3;
    if (typeStr == "w2") type = DeviceType::WAVE_2;
    else if (typeStr == "d3p") type = DeviceType::DELTA_PRO_3;
    else if (typeStr == "ac") type = DeviceType::ALTERNATOR_CHARGER;
    EcoflowESP32* dev = DeviceManager::getInstance().getDevice(type);
    if (!dev || !dev->isConnected()) { request->send(400, "text/plain", "Device not connected"); return; }
    bool success = true;
    if (type == DeviceType::DELTA_3) {
        if (cmd == "set_ac") success = dev->setAC(doc["val"]);
        else if (cmd == "set_dc") success = dev->setDC(doc["val"]);
        else if (cmd == "set_usb") success = dev->setUSB(doc["val"]);
        else if (cmd == "set_ac_lim") success = dev->setAcChargingLimit(doc["val"]);
        else if (cmd == "set_max_soc") success = dev->setBatterySOCLimits(doc["val"], -1);
        else if (cmd == "set_min_soc") success = dev->setBatterySOCLimits(101, doc["val"]);
        else success = false;
    } else if (type == DeviceType::WAVE_2) {
        if (cmd == "set_temp") dev->setTemperature((uint8_t)(int)doc["val"]);
        else if (cmd == "set_power") dev->setPowerState(doc["val"] ? 1 : 2);
        else if (cmd == "set_mode") dev->setMainMode((uint8_t)(int)doc["val"]);
        else if (cmd == "set_sub_mode") dev->setSubMode((uint8_t)(int)doc["val"]);
        else if (cmd == "set_fan") dev->setFanSpeed((uint8_t)(int)doc["val"]);
        else if (cmd == "set_drain") dev->setAutomaticDrain(doc["val"] ? 1 : 0);
        else if (cmd == "set_light") dev->setAmbientLight(doc["val"] ? 1 : 2);
        else if (cmd == "set_beep") dev->setBeep(doc["val"] ? 1 : 0);
        else success = false;
    } else if (type == DeviceType::DELTA_PRO_3) {
        if (cmd == "set_ac") success = dev->setAC(doc["val"]);
        else if (cmd == "set_ac_hv") success = dev->setAcHvPort(doc["val"]);
        else if (cmd == "set_ac_lv") success = dev->setAcLvPort(doc["val"]);
        else if (cmd == "set_dc") success = dev->setDC(doc["val"]);
        else if (cmd == "set_backup_en") success = dev->setEnergyBackup(doc["val"]);
        else if (cmd == "set_backup_level") success = dev->setEnergyBackupLevel(doc["val"]);
        else if (cmd == "set_max_soc") success = dev->setBatterySOCLimits(doc["val"], -1);
        else if (cmd == "set_min_soc") success = dev->setBatterySOCLimits(101, doc["val"]);
        else if (cmd == "set_ac_lim") success = dev->setAcChargingLimit(doc["val"]);
        else if (cmd == "set_gfi") success = dev->setGfi(doc["val"]);
        else success = false;
    } else if (type == DeviceType::ALTERNATOR_CHARGER) {
        if (cmd == "set_limit") success = dev->setPowerLimit((int)doc["val"]);
        else if (cmd == "set_open") success = dev->setChargerOpen(doc["val"]);
        else if (cmd == "set_mode") success = dev->setChargerMode((int)doc["val"]);
        else success = false;
    }
    if (success) request->send(200, "text/plain", "OK"); else request->send(400, "text/plain", "Invalid Command");
}

void WebServer::handleConnect(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    StaticJsonDocument<200> doc; deserializeJson(doc, data, len);
    String typeStr = doc["type"];
    DeviceType type = DeviceType::DELTA_3;
    if (typeStr == "w2") type = DeviceType::WAVE_2;
    else if (typeStr == "d3p") type = DeviceType::DELTA_PRO_3;
    else if (typeStr == "ac") type = DeviceType::ALTERNATOR_CHARGER;
    DeviceManager::getInstance().scanAndConnect(type);
    request->send(200, "text/plain", "Scanning...");
}

void WebServer::handleDisconnect(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    StaticJsonDocument<200> doc; deserializeJson(doc, data, len);
    String typeStr = doc["type"];
    DeviceType type = DeviceType::DELTA_3;
    if (typeStr == "w2") type = DeviceType::WAVE_2;
    else if (typeStr == "d3p") type = DeviceType::DELTA_PRO_3;
    else if (typeStr == "ac") type = DeviceType::ALTERNATOR_CHARGER;
    DeviceManager::getInstance().disconnect(type);
    request->send(200, "text/plain", "Disconnected");
}

void WebServer::handleForget(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    StaticJsonDocument<200> doc; deserializeJson(doc, data, len);
    String typeStr = doc["type"];
    DeviceType type = DeviceType::DELTA_3;
    if (typeStr == "w2") type = DeviceType::WAVE_2;
    else if (typeStr == "d3p") type = DeviceType::DELTA_PRO_3;
    else if (typeStr == "ac") type = DeviceType::ALTERNATOR_CHARGER;
    DeviceManager::getInstance().forget(type);
    request->send(200, "text/plain", "Forgotten");
}

void WebServer::handleLogs(AsyncWebServerRequest *request) {
    size_t index = 0;
    if (request->hasParam("index")) index = request->getParam("index")->value().toInt();
    DynamicJsonDocument doc(8192); JsonArray arr = doc.to<JsonArray>();
    std::vector<LogMessage> logs = LogBuffer::getInstance().getLogs(index);
    for (const auto& log : logs) {
        JsonObject obj = arr.createNestedObject();
        obj["ts"] = log.timestamp;
        obj["lvl"] = (int)log.level;
        obj["tag"] = log.tag.isEmpty() ? "?" : log.tag;
        obj["msg"] = log.message;
    }
    String json; serializeJson(doc, json);
    request->send(200, "application/json", json);
}

void WebServer::handleLogConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    StaticJsonDocument<200> doc; deserializeJson(doc, data, len);
    if (doc.containsKey("enable")) LogBuffer::getInstance().setLoggingEnabled(doc["enable"]);
    if (doc.containsKey("level")) {
        String tag = doc.containsKey("tag") ? doc["tag"].as<String>() : "";
        esp_log_level_t lvl = (esp_log_level_t)(int)doc["level"];
        if (tag.length() > 0 && tag != "Global") LogBuffer::getInstance().setTagLevel(tag, lvl);
        else LogBuffer::getInstance().setGlobalLevel(lvl);
    }
    request->send(200, "text/plain", "OK");
}

void WebServer::handleRawCommand(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    StaticJsonDocument<200> doc; deserializeJson(doc, data, len);
    if (doc.containsKey("cmd")) {
        String cmd = doc["cmd"];
        if (cmd.length() > 0) { CmdUtils::processInput(cmd); request->send(200, "text/plain", "OK"); return; }
    }
    request->send(400, "text/plain", "Invalid Command");
}

void WebServer::handleSettings(AsyncWebServerRequest *request) {
    StaticJsonDocument<200> doc;
    doc["min"] = LightSensor::getInstance().getMin();
    doc["max"] = LightSensor::getInstance().getMax();
    String json; serializeJson(doc, json);
    request->send(200, "application/json", json);
}

void WebServer::handleSettingsSave(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total) {
    StaticJsonDocument<200> doc; deserializeJson(doc, data, len);
    if (doc.containsKey("min") && doc.containsKey("max")) {
        LightSensor::getInstance().setCalibration(doc["min"], doc["max"]);
        request->send(200, "text/plain", "Saved");
    } else { request->send(400, "text/plain", "Invalid Payload"); }
}

// --- OTA HANDLERS ---

void WebServer::handleUpdateStatus(AsyncWebServerRequest *request) {
    StaticJsonDocument<200> doc;
    doc["state"] = ota_state;
    doc["progress"] = ota_progress;
    doc["msg"] = ota_msg;
    String json; serializeJson(doc, json);
    request->send(200, "application/json", json);
}

void WebServer::handleUpdateEsp32(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
        ESP_LOGI(TAG, "ESP32 Update Start: %s", filename.c_str());
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
            Update.printError(Serial);
        }
        ota_state = 1; ota_progress = 0; ota_msg = "Uploading...";
    }
    if (Update.write(data, len) != len) {
        Update.printError(Serial);
    }
    if (final) {
        if (Update.end(true)) {
            ESP_LOGI(TAG, "ESP32 Update Success");
            ota_state = 3; ota_msg = "Success! Rebooting...";
        } else {
            Update.printError(Serial);
            ota_state = 4; ota_msg = "Update Failed";
        }
    }
}

static File stm32File;

void WebServer::handleUpdateStm32(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
    if (!index) {
        ESP_LOGI(TAG, "STM32 Update Start: %s", filename.c_str());
        LittleFS.begin();
        stm32File = LittleFS.open("/stm32_update.bin", "w");
        if (!stm32File) {
            ESP_LOGE(TAG, "Failed to open file for writing");
            ota_state = 4; ota_msg = "FS Error";
            return;
        }
        ota_state = 1; ota_progress = 0; ota_msg = "Uploading...";
    }
    if (stm32File) stm32File.write(data, len);

    if (final) {
        if (stm32File) stm32File.close();
        ESP_LOGI(TAG, "STM32 Upload Complete. Triggering Flash...");
        ota_state = 2; ota_msg = "Flashing STM32...";

        // Trigger OTA Task
        Stm32Serial::getInstance().startOta("/stm32_update.bin");
    }
}
