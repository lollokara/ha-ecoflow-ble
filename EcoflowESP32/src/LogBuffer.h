#ifndef LOG_BUFFER_H
#define LOG_BUFFER_H

#include <Arduino.h>
#include <vector>
#include <string>
#include <mutex>
#include "esp_log.h"

struct LogMessage {
    uint32_t seq;
    uint32_t timestamp;
    esp_log_level_t level;
    String tag;
    String message;
};

class LogBuffer {
public:
    static LogBuffer& getInstance();

    void begin();
    void setLoggingEnabled(bool enabled);
    bool isLoggingEnabled() const;

    // Re-installs the esp_log vprintf hook if logging is enabled. Other
    // subsystems (Wi-Fi softAP, NimBLE init) can replace the global vprintf
    // handler at runtime, which would silently stop log capture; call this
    // periodically to keep ownership of the hook.
    void reassertHook();

    void setGlobalLevel(esp_log_level_t level);
    void setTagLevel(const String& tag, esp_log_level_t level);

    // Returns buffered logs whose sequence id is >= fromSeq (oldest first),
    // capped to a bounded batch so the JSON response stays small.
    std::vector<LogMessage> getLogs(uint32_t fromSeq = 0);
    size_t getLogCount() const;
    void clearLogs();

    // Direct, hook-independent logging. The Arduino core remaps app-level
    // ESP_LOGx macros to its own HAL logger, which bypasses esp_log_set_vprintf,
    // so those never reach the vprintf hook. push() writes straight into the
    // ring buffer (and mirrors to the STM32 / USB serial), guaranteeing the
    // message is visible on the web log regardless of the framework's routing.
    // Always captured, independent of the enable toggle.
    void push(esp_log_level_t level, const char* tag, const char* fmt, ...);

    // Internal use for the vprintf hook
    void addLog(esp_log_level_t level, const char* tag, const char* format, va_list args);

private:
    LogBuffer();
    LogBuffer(const LogBuffer&) = delete;
    LogBuffer& operator=(const LogBuffer&) = delete;

    // Core ring append (assumes formatted message). Thread-safe.
    void _append(esp_log_level_t level, const char* tag, const char* message);

    bool _enabled = false;
    std::vector<LogMessage> _logs;
    const size_t _maxLogs = 120; // Ring buffer depth (bounded RAM usage)
    uint32_t _seqCounter = 0;    // Monotonic id for stable web pagination
    mutable SemaphoreHandle_t _mutex;
    uint32_t _droppedLogs = 0;
};

#endif // LOG_BUFFER_H
