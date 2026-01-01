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

static lv_obj_t *ota_modal = NULL;
static lv_obj_t *ota_bar = NULL;
static lv_obj_t *ota_label = NULL;

static void _update_ota_ui(uint8_t percent) {
    if (ota_modal == NULL) {
        // Create modal
        ota_modal = lv_obj_create(lv_scr_act());
        lv_obj_set_size(ota_modal, 400, 250);
        lv_obj_center(ota_modal);
        lv_obj_set_style_bg_color(ota_modal, lv_color_hex(0x111111), 0);
        lv_obj_set_style_border_color(ota_modal, lv_color_hex(0x00f3ff), 0);
        lv_obj_set_style_border_width(ota_modal, 2, 0);

        lv_obj_t *title = lv_label_create(ota_modal);
        lv_label_set_text(title, "FIRMWARE UPDATE");
        lv_obj_set_style_text_color(title, lv_color_white(), 0);
        lv_obj_set_style_text_font(title, &lv_font_montserrat_32, 0);
        lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

        ota_label = lv_label_create(ota_modal);
        lv_label_set_text(ota_label, "Initializing...");
        lv_obj_set_style_text_color(ota_label, lv_color_hex(0xaaaaaa), 0);
        lv_obj_align(ota_label, LV_ALIGN_CENTER, 0, -10);

        ota_bar = lv_bar_create(ota_modal);
        lv_obj_set_size(ota_bar, 300, 20);
        lv_obj_align(ota_bar, LV_ALIGN_BOTTOM_MID, 0, -40);
        lv_bar_set_range(ota_bar, 0, 100);
        lv_obj_set_style_bg_color(ota_bar, lv_color_hex(0x333333), LV_PART_MAIN);
        lv_obj_set_style_bg_color(ota_bar, lv_color_hex(0x00ff9d), LV_PART_INDICATOR);
    }

    lv_bar_set_value(ota_bar, percent, LV_ANIM_OFF);

    char buf[32];
    if (percent < 100) {
        snprintf(buf, sizeof(buf), "Updating... %d%%", percent);
    } else {
        snprintf(buf, sizeof(buf), "Complete! Rebooting...");
    }
    lv_label_set_text(ota_label, buf);
}

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
            } else if (event.type == DISPLAY_EVENT_OTA_PROGRESS) {
                _update_ota_ui(event.data.otaPercent);
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
