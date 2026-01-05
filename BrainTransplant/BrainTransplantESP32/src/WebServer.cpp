#include "WebServer.h"
#include <WiFi.h>
#include <ArduinoJson.h>
#include <LittleFS.h>
#include <Update.h>
#include "Stm32Serial.h"
#include "Credentials.h"

static const char* TAG = "WebServer";
AsyncWebServer WebServer::server(80);

// Global OTA State
int ota_progress = 0;
int ota_state = 0; // 0=Idle, 1=Uploading, 2=Flashing, 3=Done, 4=Error
String ota_msg = "";

void WebServer::begin() {
    Preferences prefs;
    prefs.begin("braintransplant", true);
    String ssid = prefs.getString("wifi_ssid", "");
    String pass = prefs.getString("wifi_pass", "");
    prefs.end();

    if (ssid.length() == 0) {
        ssid = WIFI_SSID;
        pass = WIFI_PASSWORD;
    }

    if (ssid.length() > 0) {
        WiFi.mode(WIFI_STA);
        WiFi.begin(ssid.c_str(), pass.c_str());
        Serial.printf("[WebServer] Connecting to WiFi: %s\n", ssid.c_str());
    } else {
        Serial.println("[WebServer] No WiFi credentials found. Starting AP Mode.");
        WiFi.mode(WIFI_AP);
        WiFi.softAP("BrainTransplant-AP");
        Serial.println("[WebServer] AP Started: BrainTransplant-AP");
    }

    setupRoutes();
    server.begin();
}

void WebServer::setupRoutes() {
    server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send_P(200, "text/html", WEB_APP_HTML);
    });

    server.on("/api/update/status", HTTP_GET, handleUpdateStatus);

    server.on("/api/update/esp32", HTTP_POST, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
        ESP.restart();
    }, handleUpdateEsp32);

    server.on("/api/update/stm32", HTTP_POST, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", "Upload OK");
    }, handleUpdateStm32);
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
        Serial.printf("[WebServer] ESP32 Update Start: %s\n", filename.c_str());
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
            Serial.println("[WebServer] ESP32 Update Success");
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
        Serial.printf("[WebServer] STM32 Update Start: %s\n", filename.c_str());
        LittleFS.begin();
        stm32File = LittleFS.open("/stm32_update.bin", "w");
        if (!stm32File) {
            Serial.println("[WebServer] Failed to open file for writing");
            ota_state = 4; ota_msg = "FS Error";
            return;
        }
        ota_state = 1; ota_progress = 0; ota_msg = "Uploading...";
    }
    if (stm32File) stm32File.write(data, len);

    if (final) {
        if (stm32File) stm32File.close();
        Serial.println("[WebServer] STM32 Upload Complete. Triggering Flash...");
        ota_state = 2; ota_msg = "Flashing STM32...";
        Stm32Serial::getInstance().startOta("/stm32_update.bin");
    }
}
