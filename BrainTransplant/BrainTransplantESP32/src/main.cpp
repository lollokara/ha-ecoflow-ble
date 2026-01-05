#include <Arduino.h>
#include <esp_task_wdt.h>
#include "Credentials.h"
#include "WebServer.h"
#include "Stm32Serial.h"
#include "LogBuffer.h"

// Hardware Pin Definitions
#define POWER_LATCH_PIN 16

static const char* TAG = "Main";

void setup() {
    esp_task_wdt_init(10, true);
    esp_task_wdt_add(NULL);

    pinMode(POWER_LATCH_PIN, OUTPUT);
    digitalWrite(POWER_LATCH_PIN, LOW);

    Serial.begin(115200);
    ESP_LOGI(TAG, "Starting BrainTransplant Host...");

    LogBuffer::getInstance().setLoggingEnabled(true);

    // Start Web Server (connects to WiFi)
    WebServer::begin();

    // Start UART for STM32
    Stm32Serial::getInstance().begin();
}

void loop() {
    esp_task_wdt_reset();
    Stm32Serial::getInstance().update();
    vTaskDelay(10);
}
