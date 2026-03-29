#include "RemoteLogger.h"
#include "Stm32Serial.h"
#include "esp_log.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static vprintf_like_t old_vprintf;

static int remote_vprintf(const char *fmt, va_list args) {
    char buf[512]; // Increased buffer
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    if (len > 0) {
        // Strip ANSI Color Codes if present (basic simplistic strip)
        // Usually \033[0;31mE (123) ...
        char* start = buf;
        if (buf[0] == 0x1B) { // ESC
             char* m = strchr(buf, 'm');
             if (m) start = m + 1;
        }

        // We want to capture EVERYTHING according to user request
        // "even if that means to replace all esp_log calls to a custom function that reroutes the logs to both the ESP_Log and F4"
        // But we should avoid sending Stm32Serial logs to avoid recursion loops if Stm32Serial logs about sending.
        // Stm32Serial logs usually use TAG "Stm32Serial".

        if (!strstr(start, "Stm32Serial")) {
             Stm32Serial::getInstance().sendEspLog(start);
        }
    }

    if (old_vprintf) return old_vprintf(fmt, args);
    return len;
}

void RemoteLogger_Init(void) {
    old_vprintf = esp_log_set_vprintf(remote_vprintf);
}
