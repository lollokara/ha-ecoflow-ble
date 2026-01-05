#include "CmdUtils.h"
#include <WiFi.h>
#include "LogBuffer.h"

static const char* TAG = "CmdUtils";

void CmdUtils::processInput(String input) {
    input.trim();
    if (input.length() == 0) return;

    ESP_LOGI(TAG, "CMD: %s", input.c_str());

    int spaceIdx = input.indexOf(' ');
    String cmd = (spaceIdx == -1) ? input : input.substring(0, spaceIdx);
    String args = (spaceIdx == -1) ? "" : input.substring(spaceIdx + 1);

    if (cmd == "help") {
        printHelp();
    } else if (cmd == "reboot") {
        ESP.restart();
    } else if (cmd == "status") {
        ESP_LOGI(TAG, "WiFi: %s, IP: %s",
            WiFi.status() == WL_CONNECTED ? "Connected" : "Disconnected",
            WiFi.localIP().toString().c_str());
        ESP_LOGI(TAG, "Heap: %d", ESP.getFreeHeap());
    } else {
        ESP_LOGW(TAG, "Unknown command: %s", cmd.c_str());
    }
}

void CmdUtils::printHelp() {
    ESP_LOGI(TAG, "Available commands:");
    ESP_LOGI(TAG, "  reboot   - Restart ESP32");
    ESP_LOGI(TAG, "  status   - System Status");
}
