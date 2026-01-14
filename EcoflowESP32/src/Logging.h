#ifndef LOGGING_H
#define LOGGING_H

#include "esp_log.h"
#include "Stm32Serial.h"
#include <stdio.h>
#include <stdarg.h>

// Helper to format and send
static void LogToStm(esp_log_level_t level, const char* file, const char* func, const char* format, ...) {
    // 1. Local Log
    va_list args;
    va_start(args, format);
    // We can't easily forward va_list to ESP_LOG macros because they expect string literals or VA_ARGS.
    // So we format first.
    char loc_buf[256];
    vsnprintf(loc_buf, sizeof(loc_buf), format, args);
    va_end(args);

    // Get just the filename from the path
    const char* filename = strrchr(file, '/');
    if (filename) filename++; else filename = file;
    // Windows path separator check just in case
    const char* filename2 = strrchr(filename, '\\');
    if (filename2) filename = filename2 + 1;

    // Local Output
    ESP_LOG_LEVEL(level, "ESP32", "[%s] %s: %s", filename, func, loc_buf);

    // 2. STM32 Log
    // Format: [Filename] Function() Message
    // Example: [EcoflowDataParser.cpp] logFullDeltaPro3Data() --- Full D3P Dump ---
    char stm_buf[512];
    snprintf(stm_buf, sizeof(stm_buf), "[%s] %s() %s", filename, func, loc_buf);

    // Send
    // Level mapping: ESP_LOG_INFO (3) -> Protocol Level (3)
    Stm32Serial::getInstance().sendEspLog((uint8_t)level, "ESP32", stm_buf);
}

// Macros to replace ESP_LOGx
// We use a specific TAG "ESP32" for local logs generated via this macro to keep it consistent,
// or we could pass the original TAG.
// User requirement: "replace all esp_log calls"
// We'll define LOG_INFO_STM(tag, format, ...) to match typical usage patterns or just replace calls.

#define LOG_STM_E(tag, fmt, ...) LogToStm(ESP_LOG_ERROR, __FILE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define LOG_STM_W(tag, fmt, ...) LogToStm(ESP_LOG_WARN, __FILE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define LOG_STM_I(tag, fmt, ...) LogToStm(ESP_LOG_INFO, __FILE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define LOG_STM_D(tag, fmt, ...) LogToStm(ESP_LOG_DEBUG, __FILE__, __FUNCTION__, fmt, ##__VA_ARGS__)
#define LOG_STM_V(tag, fmt, ...) LogToStm(ESP_LOG_VERBOSE, __FILE__, __FUNCTION__, fmt, ##__VA_ARGS__)

#endif // LOGGING_H
// End of file
