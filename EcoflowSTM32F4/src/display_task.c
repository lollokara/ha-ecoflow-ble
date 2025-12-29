#include "display_task.h"
#include "stm32469i_discovery_lcd.h"
#include "stm32469i_discovery_ts.h"
#include <stdio.h>

QueueHandle_t displayQueue;

// Backlight Control Pin: PA3
#define BACKLIGHT_PIN GPIO_PIN_3
#define BACKLIGHT_PORT GPIOA

static void Backlight_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Pin = BACKLIGHT_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(BACKLIGHT_PORT, &GPIO_InitStruct);

    // Turn ON
    HAL_GPIO_WritePin(BACKLIGHT_PORT, BACKLIGHT_PIN, GPIO_PIN_SET);
}

void StartDisplayTask(void * argument) {
    // Initialize Hardware
    BSP_LCD_Init();
    BSP_LCD_LayerDefaultInit(LTDC_ACTIVE_LAYER_BACKGROUND, LCD_FB_START_ADDRESS);
    BSP_LCD_SelectLayer(LTDC_ACTIVE_LAYER_BACKGROUND);
    BSP_LCD_Clear(LCD_COLOR_BLACK);
    BSP_LCD_SetBackColor(LCD_COLOR_BLACK);
    BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
    BSP_LCD_DisplayOn();

    BSP_TS_Init(BSP_LCD_GetXSize(), BSP_LCD_GetYSize());

    Backlight_Init();

    // Create Queue
    displayQueue = xQueueCreate(10, sizeof(DisplayEvent));

    // Draw UI Layout (4 Sections)
    uint16_t x_size = BSP_LCD_GetXSize();
    uint16_t y_size = BSP_LCD_GetYSize();
    uint16_t mid_x = x_size / 2;
    uint16_t mid_y = y_size / 2;

    BSP_LCD_SetTextColor(LCD_COLOR_DARKGRAY);
    BSP_LCD_DrawHLine(0, mid_y, x_size);
    BSP_LCD_DrawVLine(mid_x, 0, y_size);

    BSP_LCD_SetFont(&Font24);
    BSP_LCD_SetTextColor(LCD_COLOR_WHITE);
    BSP_LCD_DisplayStringAt(x_size/4 - 50, mid_y/4, (uint8_t*)"Battery", LEFT_MODE);
    BSP_LCD_DisplayStringAt(x_size*3/4 - 50, mid_y/4, (uint8_t*)"Power", LEFT_MODE);
    BSP_LCD_DisplayStringAt(x_size/4 - 50, mid_y*3/4, (uint8_t*)"Status", LEFT_MODE);
    BSP_LCD_DisplayStringAt(x_size*3/4 - 50, mid_y*3/4, (uint8_t*)"Ctrl", LEFT_MODE);

    DisplayEvent event;
    char buffer[32];

    for (;;) {
        if (xQueueReceive(displayQueue, &event, pdMS_TO_TICKS(100)) == pdTRUE) {
            if (event.type == DISPLAY_EVENT_UPDATE_BATTERY) {
                // Update Battery Section (Top Left)
                BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
                BSP_LCD_FillRect(20, mid_y/2, mid_x - 40, 50); // Clear area

                BSP_LCD_SetTextColor(LCD_COLOR_GREEN);
                snprintf(buffer, sizeof(buffer), "SOC: %d%%", event.data.battery.soc);
                BSP_LCD_DisplayStringAt(x_size/4 - 60, mid_y/2, (uint8_t*)buffer, LEFT_MODE);

                // Update Power Section (Top Right)
                BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
                BSP_LCD_FillRect(mid_x + 20, mid_y/2, mid_x - 40, 50); // Clear area

                BSP_LCD_SetTextColor(LCD_COLOR_ORANGE);
                snprintf(buffer, sizeof(buffer), "%d W", event.data.battery.power_w);
                BSP_LCD_DisplayStringAt(x_size*3/4 - 40, mid_y/2, (uint8_t*)buffer, LEFT_MODE);

                // Update Status (Bottom Left)
                BSP_LCD_SetTextColor(LCD_COLOR_BLACK);
                BSP_LCD_FillRect(20, mid_y + mid_y/2, mid_x - 40, 50); // Clear

                BSP_LCD_SetTextColor(LCD_COLOR_BLUE);
                if (event.data.battery.connected) {
                    BSP_LCD_DisplayStringAt(x_size/4 - 60, mid_y + mid_y/2, (uint8_t*)"Conn: YES", LEFT_MODE);
                } else {
                    BSP_LCD_DisplayStringAt(x_size/4 - 60, mid_y + mid_y/2, (uint8_t*)"Conn: NO", LEFT_MODE);
                }
            }
        }

        // Poll Touch (Simple example)
        TS_StateTypeDef tsState;
        BSP_TS_GetState(&tsState);
        if (tsState.touchDetected) {
            // Handle touch
        }
    }
}
