#include "LogBuffer.h"

static int custom_vprintf(const char* fmt, va_list args) {
    // This function hooks into esp_log.
    // We need to parse the format to extract level and tag if possible,
    // but esp_log_write passes the raw format string.
    // The standard esp_log implementation handles formatting.
    // Hooking vprintf catches EVERYTHING, including Serial.printf.

    // However, esp_log uses esp_log_write which calls esp_log_set_vprintf handler.
    // The handler receives the format string.
    // Standard ESP log format is: "Wait..." or with color codes.
    // It's hard to reverse-engineer the level/tag from the final string without
    // reimplementing the whole log macro logic.

    // LIMITATION: esp_log_set_vprintf gives us the formatted string *generation* capability.
    // It does NOT give us the level/tag explicitly in the arguments.
    // But wait, esp_log system calls the vprintf function with the *final* string?
    // No, it calls it with format and args.

    // To get structured logs (Level, Tag), we need to wrap the logging macros or
    // rely on the string parsing.
    // Since we cannot easily change all ESP_LOGx calls in the code (and libraries),
    // we will capture the output string.

    // Actually, a better approach for this specific requirement ("Select source... like DeviceManager.cpp")
    // is to rely on `esp_log_level_set`.
    // If we enable verbose for "DeviceManager", those logs will be generated.
    // We just need to capture them.

    // Let's use a static buffer to format the message.
    char buf[256];
    vsnprintf(buf, sizeof(buf), fmt, args);

    // We will simply store the string.
    // Parsing "I (123) Tag: Message" from the string is possible if standard format is used.
    // Default format: "L (time) tag: message"
    // L = I, E, W, D, V

    LogBuffer::getInstance().addLog(ESP_LOG_INFO, "SYS", buf, args); // We pass args but we already formatted.

    // Also print to Serial so we don't lose console
    return vprintf(fmt, args);
}

// Re-implementing a cleaner hook that tries to parse standard ESP-IDF log format if possible,
// or just dumps it.
// But since we can't change the log format easily, let's just capture the string.

LogBuffer& LogBuffer::getInstance() {
    static LogBuffer instance;
    return instance;
}

LogBuffer::LogBuffer() {
    _mutex = xSemaphoreCreateMutex();
}

void LogBuffer::begin() {
    // Hook into the logging system?
    // esp_log_set_vprintf(custom_vprintf);
    // This is dangerous if not careful.
    // Also, capturing ALL output might be too much.
    // Let's implement a specific "addLog" that we can call from a custom macro if we wanted,
    // but the user wants to see existing logs.

    // For the sake of stability and the "hefty" warning, we will NOT hook vprintf globally by default.
    // We will only enable it when the user toggles "Web Logging".
}

// We need a separate static function for the hook
static vprintf_like_t old_vprintf = nullptr;

static int silent_vprintf(const char *fmt, va_list args) {
    return 0; // Do nothing
}

static int buffer_vprintf(const char *fmt, va_list args) {
    // Format the message
    char buffer[256];
    int len = vsnprintf(buffer, sizeof(buffer), fmt, args);

    if (len > 0) {
        // Try to parse level and tag from standard ESP log format
        // Format: "L (time) Tag: Message\n"
        // Example: "I (1234) DeviceManager: Found device..."

        String logLine = String(buffer);

        // Basic parsing logic (fragile but works for standard logs)
        esp_log_level_t level = ESP_LOG_INFO;
        String tag = "SYS";
        String msg = logLine;

        char levelChar = logLine.charAt(0);
        if (levelChar == 'E') level = ESP_LOG_ERROR;
        else if (levelChar == 'W') level = ESP_LOG_WARN;
        else if (levelChar == 'I') level = ESP_LOG_INFO;
        else if (levelChar == 'D') level = ESP_LOG_DEBUG;
        else if (levelChar == 'V') level = ESP_LOG_VERBOSE;

        // Extract Tag? It's usually between ) and :
        int closeParen = logLine.indexOf(')');
        int colon = logLine.indexOf(':', closeParen);
        if (closeParen > 0 && colon > closeParen) {
            tag = logLine.substring(closeParen + 2, colon); // +2 to skip ") "
            msg = logLine.substring(colon + 2); // +2 to skip ": "
        }

        // Add to buffer (fake args since we already formatted)
        // We pass the RAW formatted string as message because our parsing is guesswork.
        LogBuffer::getInstance().addLog(level, tag.c_str(), logLine.c_str(), args);
    }

    // Forward to original handler (usually UART)
    if (old_vprintf) {
        return old_vprintf(fmt, args);
    } else {
        return vprintf(fmt, args);
    }
}

void LogBuffer::setLoggingEnabled(bool enabled) {
    if (_enabled == enabled) return;

    _enabled = enabled;
    if (_enabled) {
        // Enable Logging: Switch from Silent (or default) to Buffer+UART
        vprintf_like_t current = esp_log_set_vprintf(buffer_vprintf);
        // If we were silent, current is silent_vprintf. We don't want that as old_vprintf.
        // We want the original UART handler.
        // If old_vprintf is null, it means we haven't saved the UART handler yet?
        // Or if we were disabled, we set it to silent.
        if (current != silent_vprintf && current != buffer_vprintf) {
             old_vprintf = current;
        }
    } else {
        // Disable Logging: Switch to Silent
        vprintf_like_t current = esp_log_set_vprintf(silent_vprintf);
        if (current != silent_vprintf && current != buffer_vprintf) {
            old_vprintf = current;
        }
    }
}

bool LogBuffer::isLoggingEnabled() const {
    return _enabled;
}

void LogBuffer::setGlobalLevel(esp_log_level_t level) {
    esp_log_level_set("*", level);
}

void LogBuffer::setTagLevel(const String& tag, esp_log_level_t level) {
    // Exclusive Tag Mode: Mute everything else
    esp_log_level_set("*", ESP_LOG_NONE);
    esp_log_level_set(tag.c_str(), level);
}

void LogBuffer::addLog(esp_log_level_t level, const char* tag, const char* message, va_list args) {
    if (!_enabled) return;

    xSemaphoreTake(_mutex, portMAX_DELAY);

    if (_logs.size() >= _maxLogs) {
        _logs.erase(_logs.begin()); // Remove oldest
    }

    LogMessage lm;
    lm.timestamp = millis();
    lm.level = level;
    lm.tag = String(tag);
    lm.message = String(message); // Already formatted
    // Strip newline if present at end
    if (lm.message.endsWith("\n")) lm.message.remove(lm.message.length()-1);

    _logs.push_back(lm);

    xSemaphoreGive(_mutex);
}

std::vector<LogMessage> LogBuffer::getLogs(size_t fromIndex) {
    xSemaphoreTake(_mutex, portMAX_DELAY);
    std::vector<LogMessage> result = _logs; // Copy
    xSemaphoreGive(_mutex);
    return result;
}

size_t LogBuffer::getLogCount() const {
    // Not strictly thread safe without mutex but size() is atomic usually
    return _logs.size();
}

void LogBuffer::clearLogs() {
    xSemaphoreTake(_mutex, portMAX_DELAY);
    _logs.clear();
    xSemaphoreGive(_mutex);
}
