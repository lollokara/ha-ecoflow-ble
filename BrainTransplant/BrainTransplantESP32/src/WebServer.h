#ifndef WEB_SERVER_H
#define WEB_SERVER_H

#include <ESPAsyncWebServer.h>
#include <Preferences.h>
#include <Update.h>

class WebServer {
public:
    static void begin();

private:
    static AsyncWebServer server;
    static void setupRoutes();

    static void handleLogs(AsyncWebServerRequest *request);
    static void handleLogConfig(AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total);
    static void handleUpdateStatus(AsyncWebServerRequest *request);
    static void handleUpdateEsp32(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
    static void handleUpdateStm32(AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final);
};

#endif
