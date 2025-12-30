#include "display_task.h"
#include "stm32469i_discovery_lcd.h"
#include "stm32469i_discovery_ts.h"
#include "stm32469i_discovery_sdram.h"
#include "ui/ui_lvgl.h"
#include "lvgl.h"
#include "backlight.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// QueueHandle_t displayQueue; // Defined in main.c

void StartDisplayTask(void * argument) {
    // Init Hardware
    BSP_SDRAM_Init();

    // Init Touch (Hardcoded size to avoid dependency on LCD init state)
    BSP_TS_Init(800, 480);

    printf("Display Task Started\n");

    // Init UI (Handles LCD and Backlight Init)
    UI_LVGL_Init();

    printf("UI Init Done\n");

    DisplayEvent event;
    TickType_t xLastWakeTime;
    const TickType_t xPeriod = pdMS_TO_TICKS(5); // 5ms loop for smooth UI
    xLastWakeTime = xTaskGetTickCount();

    // State for Backlight
    uint8_t target_brightness = 100;

    for (;;) {
        // Handle Data Updates
        while (xQueueReceive(displayQueue, &event, 0) == pdTRUE) {
            if (event.type == DISPLAY_EVENT_UPDATE_BATTERY) {
                // Update UI (and get requested brightness from packet)
                UI_LVGL_Update(&event.data.deviceStatus);
                // The brightness comes from ESP32 -> packet -> DeviceStatus.brightness
                // Store it for use when active
                target_brightness = event.data.deviceStatus.brightness;
            }
        }

        // Idle Logic for Backlight
        // lv_disp_get_inactive_time() returns ms since last input
        if (lv_disp_get_inactive_time(NULL) > 60000) { // 60s
            Backlight_SetBrightness(10); // Sleep: 10%
        } else {
            // Active: Set to brightness from packet
            // Backlight_SetBrightness will clamp to minimum 10%
            Backlight_SetBrightness(target_brightness);
        }

        // Monitor Heap
        static TickType_t lastHeapPrint = 0;
        if (xTaskGetTickCount() - lastHeapPrint > pdMS_TO_TICKS(5000)) {
            lastHeapPrint = xTaskGetTickCount();
            // printf("Free Heap: %d\n", xPortGetFreeHeapSize());
        }

        // LVGL Task Handler
        lv_timer_handler();

        // Delay
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}
