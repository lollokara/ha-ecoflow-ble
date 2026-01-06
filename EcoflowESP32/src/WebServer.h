#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include "DeviceManager.h"
#include "LightSensor.h"
#include "LogBuffer.h"
#include "CmdUtils.h"
#include "WebAssets.h"
#include <Update.h>

class WebServer {
public:
    static void begin();

private:
    static AsyncWebServer server;
    static void setupRoutes();
    static void handleStatus(AsyncWebServerRequest *request);
    static void handleControl(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    static void handleConnect(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    static void handleDisconnect(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    static void handleForget(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    static void handleHistory(AsyncWebServerRequest *request);
    static void handleLogs(AsyncWebServerRequest *request);
    static void handleLogList(AsyncWebServerRequest *request);
    static void handleLogDownload(AsyncWebServerRequest *request);
    static void handleLogDelete(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    static void handleLogConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    static void handleRawCommand(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    static void handleSettings(AsyncWebServerRequest *request);
    static void handleSettingsSave(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);

    // OTA Handlers
    static void handleUpdateEsp32(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
    static void handleUpdateStm32(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
    static void handleUpdateStatus(AsyncWebServerRequest *request);
};

#endif
