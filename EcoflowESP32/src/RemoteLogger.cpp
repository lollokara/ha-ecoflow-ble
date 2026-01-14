#include "RemoteLogger.h"
#include "Stm32Serial.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static vprintf_like_t old_vprintf;

static int remote_vprintf(const char *fmt, va_list args) {
    char buf[256];
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    if (len > 0) {
        // ESP-IDF log format usually starts with color code or 'E'/'W'
        // Simple check for Error/Warning markers
        // Standard format: "E (123) TAG: Message"
        // Note: colors might be present.

        uint8_t level = ESP_LOG_INFO;
        bool interesting = false;

        if (strstr(buf, "E (")) { level = ESP_LOG_ERROR; interesting = true; }
        else if (strstr(buf, "W (")) { level = ESP_LOG_WARN; interesting = true; }

        // Also capture our specific "Dump" tags regardless of level if they don't match E/W
        // but `EcoflowDataParser` uses `ESP_LOGI`.
        // So we should look for specific tags too.

        bool isDump = strstr(buf, "EcoflowDataParser") != NULL;

        if (interesting || isDump) {
             // Extract Tag and Message
             char* closeParen = strchr(buf, ')');
             if (closeParen) {
                 char* colon = strchr(closeParen, ':');
                 if (colon) {
                     char tag[32];
                     int tagLen = colon - (closeParen + 2);
                     if (tagLen > 31) tagLen = 31;
                     if (tagLen > 0) {
                         strncpy(tag, closeParen + 2, tagLen);
                         tag[tagLen] = 0;

                         // Avoid recursion
                         if (strcmp(tag, "Stm32Serial") != 0) {
                             Stm32Serial::getInstance().sendEspLog(level, tag, colon + 2);
                         }
                     }
                 }
             }
        }
    }

    // Disable local UART echo to prevent interference with GPIO 1 (Light Sensor)
    // if (old_vprintf) return old_vprintf(fmt, args);
    return len;
}

void RemoteLogger_Init(void) {
    old_vprintf = esp_log_set_vprintf(remote_vprintf);
}
