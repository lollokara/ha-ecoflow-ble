#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <Arduino.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include "DeviceManager.h"
#include "CmdUtils.h"

class WebServer {
public:
    static void begin();

private:
    static AsyncWebServer server;
    static void setupRoutes();
    static void handleStatus(AsyncWebServerRequest *request);
    static void handleCommand(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    static void handleScan(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    static void handleDisconnect(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
};

#endif
