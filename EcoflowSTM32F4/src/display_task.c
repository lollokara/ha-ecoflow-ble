#include "display_task.h"
#include "stm32469i_discovery_lcd.h"
#include "stm32469i_discovery_ts.h"
#include "stm32469i_discovery_sdram.h"
#include "ui/ui_lvgl.h"
#include "ui/ui_view_debug.h"
#include "ui/ui_view_connections.h"
#include "lvgl.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// QueueHandle_t displayQueue; // Defined in main.c
extern IWDG_HandleTypeDef hiwdg;

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

    for (;;) {
        HAL_IWDG_Refresh(&hiwdg);

        // Handle Data Updates
        while (xQueueReceive(displayQueue, &event, 0) == pdTRUE) {
            if (event.type == DISPLAY_EVENT_UPDATE_BATTERY) {
                // printf("Display: Update UI...\n"); // Comment out excessive logging
                UI_LVGL_Update(&event.data.deviceStatus);
                // printf("Display: UI Updated\n");
            } else if (event.type == DISPLAY_EVENT_UPDATE_DEBUG) {
                UI_UpdateDebugInfo(&event.data.debugInfo);
            } else if (event.type == DISPLAY_EVENT_UPDATE_DEVICE_LIST) {
                UI_UpdateConnectionsView(&event.data.deviceList);
            }
        }

        // Monitor Heap
        static TickType_t lastHeapPrint = 0;
        if (xTaskGetTickCount() - lastHeapPrint > pdMS_TO_TICKS(5000)) {
            lastHeapPrint = xTaskGetTickCount();
            printf("Free Heap: %d\n", xPortGetFreeHeapSize());
        }

        // LVGL Task Handler
        lv_timer_handler();

        // Delay
        vTaskDelayUntil(&xLastWakeTime, xPeriod);
    }
}
