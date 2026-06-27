#include "RemoteLogger.h"
#include "Stm32Serial.h"
#include "esp_log.h"
#include <string.h>

// Forwards a single (already parsed) log line to the STM32 over the inter-chip
// UART. The full, unfiltered log stream is captured by LogBuffer and served on
// the web UI; here we only forward high-value lines (errors, warnings and the
// data-dump output) so we don't saturate the bandwidth-limited UART that is
// shared with telemetry and log downloads.
void RemoteLogger_Forward(int level, const char* tag, const char* msg) {
    if (tag == nullptr || msg == nullptr) return;

    // Prevent recursion: never feed logs originating from the serial/log
    // plumbing back into the UART forwarder.
    if (strcmp(tag, "Stm32Serial") == 0 || strcmp(tag, "LogBuffer") == 0) {
        return;
    }

    bool forward = (level == ESP_LOG_ERROR || level == ESP_LOG_WARN);
    if (!forward && strcmp(tag, "EcoflowDataParser") == 0) {
        forward = true;
    }
    if (!forward) return;

    Stm32Serial::getInstance().sendEspLog((uint8_t)level, tag, msg);
}
