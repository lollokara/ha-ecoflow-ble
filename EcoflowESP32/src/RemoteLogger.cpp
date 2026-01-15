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

        uint8_t level = ESP_LOG_INFO;

        // Attempt to parse standard ESP log format: "L (Timestamp) TAG: Message"
        // E.g. "I (123) NimBLE: Connected"
        // Or "E (123) TAG: Message"

        char* tagStart = NULL;
        char* msgStart = NULL;
        char tag[33] = "SYS";

        // Basic detection of level
        if (strncmp(buf, "E (", 3) == 0) level = ESP_LOG_ERROR;
        else if (strncmp(buf, "W (", 3) == 0) level = ESP_LOG_WARN;
        else if (strncmp(buf, "I (", 3) == 0) level = ESP_LOG_INFO;
        else if (strncmp(buf, "D (", 3) == 0) level = ESP_LOG_DEBUG;
        else if (strncmp(buf, "V (", 3) == 0) level = ESP_LOG_VERBOSE;

        // Try to find the tag and message separator ": "
        // Format is typically: Level (Time) Tag: Message
        // But Arduino logs might be different: "[Time][Level][File:Line] Func: Msg"

        if (buf[0] == '[') {
             // Arduino format: [Time][Level][File:Line] Func: Msg
             // We can just send the whole thing as message with a generic tag or try to parse
             // User wants "[File] Func() Msg"
             // If we can't parse easily, just sending the whole line is better than nothing.
             // But we need to avoid infinite recursion with Stm32Serial logging.
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
        // Filter out noisy tags to prevent WDT resets during high-traffic operations (like BLE Scan)
        if (strcmp(tag, "Stm32Serial") != 0 &&
            strcmp(tag, "EcoflowDataParser") != 0 &&
            strcmp(tag, "NimBLEScan") != 0) {
             Stm32Serial::getInstance().sendEspLog(level, tag, msgStart);
        }
    }

    if (old_vprintf) return old_vprintf(fmt, args);
    return len;
}

void RemoteLogger_Init(void) {
    old_vprintf = esp_log_set_vprintf(remote_vprintf);
}
