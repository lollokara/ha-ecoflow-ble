#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <ESPAsyncWebServer.h>
#include <ArduinoJson.h>
#include <Preferences.h>
#include "WebAssets.h"

class WebServer {
public:
    static void begin();

private:
    static AsyncWebServer server;
    static void setupRoutes();

    // OTA Handlers
    static void handleUpdateEsp32(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
    static void handleUpdateStm32(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
    static void handleUpdateStatus(AsyncWebServerRequest *request);
};

#endif
