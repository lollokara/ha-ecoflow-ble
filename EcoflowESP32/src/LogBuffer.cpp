#include "LogBuffer.h"
#include "RemoteLogger.h"

// We need a separate static function for the hook
static vprintf_like_t old_vprintf = nullptr;

static int silent_vprintf(const char *fmt, va_list args) {
    return 0; // Do nothing
}

static int buffer_vprintf(const char *fmt, va_list args) {
    // Keep a separate copy for the downstream handler: vsnprintf() consumes the
    // original va_list, and reusing it afterwards is undefined behaviour.
    va_list args_copy;
    va_copy(args_copy, args);

    // Format the message
    char buffer[256];
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);

    if (len > 0) {
        String logLine = String(buffer);

        // Basic parsing logic
        esp_log_level_t level = ESP_LOG_INFO;
        String tag = "SYS";
        String msg = logLine;

        char levelChar = logLine.charAt(0);
        if (levelChar == 'E') level = ESP_LOG_ERROR;
        else if (levelChar == 'W') level = ESP_LOG_WARN;
        else if (levelChar == 'I') level = ESP_LOG_INFO;
        else if (levelChar == 'D') level = ESP_LOG_DEBUG;
        else if (levelChar == 'V') level = ESP_LOG_VERBOSE;

        // Extract Tag: "L (time) Tag: Message"
        int closeParen = logLine.indexOf(')');
        int colon = logLine.indexOf(':', closeParen);
        if (closeParen > 0 && colon > closeParen) {
            tag = logLine.substring(closeParen + 2, colon);
            msg = logLine.substring(colon + 2);
        }

        LogBuffer::getInstance().addLog(level, tag.c_str(), msg.c_str(), args);
        // Mirror important lines to the STM32 over the inter-chip UART.
        RemoteLogger_Forward((int)level, tag.c_str(), msg.c_str());
    }

    // Forward to original handler (USB CDC debug serial)
    int ret = len;
    if (old_vprintf) {
        ret = old_vprintf(fmt, args_copy);
    }
    va_end(args_copy);
    return ret;
}

LogBuffer& LogBuffer::getInstance() {
    static LogBuffer instance;
    return instance;
}

LogBuffer::LogBuffer() {
    _mutex = xSemaphoreCreateMutex();
}

void LogBuffer::begin() {
    // Ensure we start in the correct state
    // If _enabled is false (default), ensure we are silent
    if (!_enabled) {
        setLoggingEnabled(false);
    }
}

void LogBuffer::setLoggingEnabled(bool enabled) {
    _enabled = enabled;
    if (_enabled) {
        // Enable Logging: Restore Global Level and Switch to Buffer+UART
        esp_log_level_set("*", ESP_LOG_INFO);

        // Keep NimBLE at WARN: its INFO "GATT procedure initiated" lines are
        // extremely high-frequency and churn String allocations through the log
        // buffer, which exhausts the heap and causes bad_alloc -> abort crashes.
        esp_log_level_set("NimBLE", ESP_LOG_WARN);
        esp_log_level_set("NimBLEScan", ESP_LOG_WARN);
        esp_log_level_set("NimBLEClient", ESP_LOG_WARN);
        esp_log_level_set("NimBLEAdvertisedDevice", ESP_LOG_WARN);

        vprintf_like_t current = esp_log_set_vprintf(buffer_vprintf);
        if (current != silent_vprintf && current != buffer_vprintf) {
             old_vprintf = current;
        }
    } else {
        // Disable Logging: Mute Global Level and Switch to Silent Hook
        esp_log_level_set("*", ESP_LOG_NONE);

        // Explicitly mute NimBLE components which might ignore global * sometimes or be verbose
        esp_log_level_set("NimBLE", ESP_LOG_NONE);
        esp_log_level_set("NimBLEScan", ESP_LOG_NONE);
        esp_log_level_set("NimBLEClient", ESP_LOG_NONE);
        esp_log_level_set("NimBLEAdvertisedDevice", ESP_LOG_NONE);

        vprintf_like_t current = esp_log_set_vprintf(silent_vprintf);
        if (current != silent_vprintf && current != buffer_vprintf) {
            old_vprintf = current;
        }
    }
}

bool LogBuffer::isLoggingEnabled() const {
    return _enabled;
}

void LogBuffer::reassertHook() {
    if (!_enabled) return;
    // If some other subsystem replaced the global vprintf handler, take it back.
    // Remember a foreign handler so console output still chains through.
    vprintf_like_t current = esp_log_set_vprintf(buffer_vprintf);
    if (current != silent_vprintf && current != buffer_vprintf) {
        old_vprintf = current;
    }
}

void LogBuffer::setGlobalLevel(esp_log_level_t level) {
    esp_log_level_set("*", level);
}

void LogBuffer::setTagLevel(const String& tag, esp_log_level_t level) {
    // Exclusive Tag Mode: Mute everything else
    esp_log_level_set("*", ESP_LOG_NONE);

    // Also mute NimBLE explicitly if we are filtering for something else
    esp_log_level_set("NimBLE", ESP_LOG_NONE);
    esp_log_level_set("NimBLEScan", ESP_LOG_NONE);

    esp_log_level_set(tag.c_str(), level);
}

void LogBuffer::addLog(esp_log_level_t level, const char* tag, const char* message, va_list args) {
    if (!_enabled) return;
    _append(level, tag, message);
}

void LogBuffer::push(esp_log_level_t level, const char* tag, const char* fmt, ...) {
    char msg[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(msg, sizeof(msg), fmt, args);
    va_end(args);

    // Always captured (ignores the enable toggle) so deliberately-placed
    // diagnostics (boot, OTA, connection flow) are never silently lost.
    _append(level, tag, msg);

    // Mirror important lines to the STM32 (suppressed during OTA internally)
    // and echo to USB CDC for wired debugging.
    RemoteLogger_Forward((int)level, tag, msg);
    Serial.printf("[%s] %s\n", tag, msg);
}

void LogBuffer::_append(esp_log_level_t level, const char* tag, const char* message) {
    if (xSemaphoreTake(_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        _droppedLogs++;
        return;
    }

    if (_logs.size() >= _maxLogs) {
        _logs.erase(_logs.begin()); // Remove oldest
    }

    LogMessage lm;
    lm.seq = _seqCounter++;
    lm.timestamp = millis();
    lm.level = level;
    lm.tag = String(tag);
    lm.message = String(message);
    // Bound per-entry heap: long lines (protobuf dumps, etc.) are truncated so a
    // burst of large messages can't exhaust the heap.
    if (lm.message.length() > 160) lm.message.remove(160);
    if (lm.message.endsWith("\n")) lm.message.remove(lm.message.length()-1);

    _logs.push_back(lm);

    xSemaphoreGive(_mutex);
}

std::vector<LogMessage> LogBuffer::getLogs(uint32_t fromSeq) {
    xSemaphoreTake(_mutex, portMAX_DELAY);
    std::vector<LogMessage> result;

    // The buffer is a sliding window; entries carry a monotonic sequence id so
    // the client can simply ask for "everything newer than the last id I saw".
    // This is robust to ring rotation (unlike index-by-position pagination).
    const size_t MAX_BATCH = 60;
    for (const auto& lm : _logs) {
        if (lm.seq >= fromSeq) {
            result.push_back(lm);
            if (result.size() >= MAX_BATCH) break;
        }
    }

    xSemaphoreGive(_mutex);
    return result;
}

size_t LogBuffer::getLogCount() const {
    return _logs.size();
}

void LogBuffer::clearLogs() {
    xSemaphoreTake(_mutex, portMAX_DELAY);
    _logs.clear();
    xSemaphoreGive(_mutex);
}
