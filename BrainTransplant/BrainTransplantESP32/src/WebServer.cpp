#include "WebServer.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <esp_log.h>
#include <LittleFS.h>
#include "Stm32Serial.h"
#include "WebAssets.h"
#include "LogBuffer.h"
#include "CmdUtils.h"
#include "Credentials.h"

static const char* TAG = "WebServer";
AsyncWebServer WebServer::server(80);

int ota_progress = 0;
int ota_state = 0;
String ota_msg = "";

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

    server.on("/api/logs", HTTP_GET, handleLogs);
    server.on("/api/log_config", HTTP_POST, [](AsyncWebServerRequest *r){}, NULL, handleLogConfig);

    server.on("/api/update/status", HTTP_GET, handleUpdateStatus);

    server.on("/api/update/esp32", HTTP_POST, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        ESP.restart();
    }, handleUpdateEsp32);

    server.on("/api/update/stm32", HTTP_POST, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "Upload OK");
    }, handleUpdateStm32);
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
        if (!Update.begin(UPDATE_SIZE_UNKNOWN)) Update.printError(Serial);
        ota_state = 1; ota_progress = 0; ota_msg = "Uploading...";
    }
    if (Update.write(data, len) != len) Update.printError(Serial);
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
        Stm32Serial::getInstance().startOta("/stm32_update.bin");
    }
}
