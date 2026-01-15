#include "RemoteLogger.h"
#include "Stm32Serial.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static vprintf_like_t old_vprintf;

static int remote_vprintf(const char *fmt, va_list args) {
    char buf[512]; // Increased buffer size to ensure complete messages
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    if (len > 0) {
        // Strip trailing newlines
        while (len > 0 && (buf[len-1] == '\n' || buf[len-1] == '\r')) {
            buf[len-1] = 0;
            len--;
        }

        // Aggressive filtering of noisy logs to prevent UART saturation and WDT resets
        // Specifically NimBLE scan results which flood the logs
        if (strstr(buf, "NimBLE") != NULL) {
            return len; // Ignore NimBLE logs entirely
        }

        uint8_t level = ESP_LOG_INFO;

        char* tagStart = NULL;
        char* msgStart = NULL;
        char tag[33] = "SYS";

        // Basic detection of level
        if (strncmp(buf, "E (", 3) == 0) level = ESP_LOG_ERROR;
        else if (strncmp(buf, "W (", 3) == 0) level = ESP_LOG_WARN;
        else if (strncmp(buf, "I (", 3) == 0) level = ESP_LOG_INFO;
        else if (strncmp(buf, "D (", 3) == 0) level = ESP_LOG_DEBUG;
        else if (strncmp(buf, "V (", 3) == 0) level = ESP_LOG_VERBOSE;

        if (buf[0] == '[') {
             // Arduino format: [Time][Level][File:Line] Func: Msg
             strncpy(tag, "ESP32", 32);
             msgStart = buf;
        } else {
             // ESP-IDF Format
             char* closeParen = strstr(buf, ") ");
             if (closeParen) {
                 tagStart = closeParen + 2;
                 char* colon = strstr(tagStart, ": ");
                 if (colon) {
                     int tagLen = colon - tagStart;
                     if (tagLen > 31) tagLen = 31;
                     strncpy(tag, tagStart, tagLen);
                     tag[tagLen] = 0;
                     msgStart = colon + 2;
                 }
             }
        }

        if (msgStart == NULL) msgStart = buf;

        // Recursion prevention: Do not forward logs from Stm32Serial itself
        // Note: EcoflowDataParser logs ARE forwarded as they contain the requested dumps
        if (strcmp(tag, "Stm32Serial") != 0) {
             Stm32Serial::getInstance().sendEspLog(level, tag, msgStart);
        }
    }

    if (old_vprintf) return old_vprintf(fmt, args);
    return len;
}

void RemoteLogger_Init(void) {
    old_vprintf = esp_log_set_vprintf(remote_vprintf);
}
