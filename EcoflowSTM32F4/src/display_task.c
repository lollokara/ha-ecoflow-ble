#include "display_task.h"
#include "stm32469i_discovery_lcd.h"
#include "stm32469i_discovery_ts.h"
#include "ui_layout.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// Backlight Control Pin: PA3
#define BACKLIGHT_PIN GPIO_PIN_3
#define BACKLIGHT_PORT GPIOA

// Double Buffering
#define LCD_FRAME_BUFFER_1  LCD_FB_START_ADDRESS
#define LCD_FRAME_BUFFER_2  (LCD_FB_START_ADDRESS + 0x200000)

static uint32_t current_buffer = LCD_FRAME_BUFFER_1;
static uint32_t pending_buffer = LCD_FRAME_BUFFER_2;

// Cache data to redraw full frame
DeviceStatus currentDevStatus = {0};

// Helper: Init Backlight
static void Backlight_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Pin = BACKLIGHT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BACKLIGHT_PORT, &GPIO_InitStruct);

    HAL_GPIO_WritePin(BACKLIGHT_PORT, BACKLIGHT_PIN, GPIO_PIN_SET);
}

static void RenderFrame() {
    // 1. Set drawing target to pending buffer
    hltdc_eval.LayerCfg[LTDC_ACTIVE_LAYER_BACKGROUND].FBStartAdress = pending_buffer;

    // 2. Draw UI Sections using the new Layout module
    // Background is white in the new spec
    BSP_LCD_Clear(GUI_COLOR_BG);

    UI_DrawBatteryStatus(&currentDevStatus);
    UI_DrawEnergyFlow(&currentDevStatus);
    UI_DrawFooter(&currentDevStatus);

    // 3. Request buffer swap
    HAL_LTDC_SetAddress(&hltdc_eval, pending_buffer, LTDC_ACTIVE_LAYER_BACKGROUND);
    HAL_LTDC_Reload(&hltdc_eval, LTDC_RELOAD_VERTICAL_BLANKING);

    // 4. Wait for VSYNC
    uint32_t tickstart = HAL_GetTick();
    int safety_count = 0;
    while(hltdc_eval.Instance->SRCR & LTDC_SRCR_VBR) {
        if((HAL_GetTick() - tickstart) > 50) break;
        safety_count++;
        if (safety_count > 1000000) break;
    }

    // 5. Swap
    uint32_t temp = current_buffer;
    current_buffer = pending_buffer;
    pending_buffer = temp;

    // Reset drawing target for next frame (though we set it at start of RenderFrame anyway)
    hltdc_eval.LayerCfg[LTDC_ACTIVE_LAYER_BACKGROUND].FBStartAdress = pending_buffer;
}

void StartDisplayTask(void * argument) {
    // Init Hardware
    BSP_SDRAM_Init();
    BSP_LCD_Init();
    BSP_LCD_LayerDefaultInit(LTDC_ACTIVE_LAYER_BACKGROUND, current_buffer);
    BSP_LCD_SelectLayer(LTDC_ACTIVE_LAYER_BACKGROUND);

    BSP_LCD_Clear(GUI_COLOR_BG);
    BSP_LCD_SetBackColor(GUI_COLOR_BG);
    BSP_LCD_SetTextColor(GUI_COLOR_TEXT);
    BSP_LCD_DisplayOn();

    BSP_TS_Init(BSP_LCD_GetXSize(), BSP_LCD_GetYSize());
    Backlight_Init();

    // Initial Draw
    RenderFrame();

    DisplayEvent event;
    TS_StateTypeDef tsState;
    bool needs_redraw = false;
    uint32_t last_touch_poll = 0;

    for (;;) {
        // Handle Data Updates
        if (xQueueReceive(displayQueue, &event, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (event.type == DISPLAY_EVENT_UPDATE_BATTERY) {
                if (memcmp(&currentDevStatus, &event.data.deviceStatus, sizeof(DeviceStatus)) != 0) {
                     currentDevStatus = event.data.deviceStatus;
                     needs_redraw = true;
                }
            }
        }

        // Handle Touch
        if (xTaskGetTickCount() - last_touch_poll > pdMS_TO_TICKS(30)) {
            last_touch_poll = xTaskGetTickCount();
            BSP_TS_GetState(&tsState);
            if (tsState.touchDetected) {
                // For now, just redraw if touched to simulate interaction responsiveness
                // Later we can add touch handling logic in UI_HandleTouch
                // needs_redraw = true;
                // Actually, let's implement the settings button click
                 uint16_t tx = tsState.touchX[0];
                 uint16_t ty = tsState.touchY[0];

                 // Check Settings Button (720, 360, 60, 40)
                 if (tx >= 720 && tx <= 780 && ty >= 360 && ty <= 400) {
                     // Settings clicked - for now just log
                     // printf("Settings Clicked\n");
                 }

                 // Check Toggles... (To be implemented)
            }
        }

        if (needs_redraw) {
            RenderFrame();
            needs_redraw = false;
        }
    }
}
