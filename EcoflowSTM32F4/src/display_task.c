#include "display_task.h"
#include "stm32469i_discovery_lcd.h"
#include "stm32469i_discovery_ts.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

QueueHandle_t displayQueue;

// Backlight Control Pin: PA3
#define BACKLIGHT_PIN GPIO_PIN_3
#define BACKLIGHT_PORT GPIOA

// Modern UI Colors
#define GUI_COLOR_BG        LCD_COLOR_BLACK
#define GUI_COLOR_HEADER    0xFF202020
#define GUI_COLOR_TEXT      LCD_COLOR_WHITE
#define GUI_COLOR_ACCENT    0xFF00ADB5
#define GUI_COLOR_BUTTON    0xFF393E46
#define GUI_COLOR_BTN_PRESS 0xFF00ADB5
#define GUI_COLOR_PANEL     0xFF222831

// Simple Button Struct
typedef struct {
    uint16_t x, y, w, h;
    char label[16];
    bool pressed;
    bool visible;
} SimpleButton;

// Global UI State
SimpleButton testBtn = {250, 200, 200, 60, "TOGGLE", false, true};

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

// Helper: Draw Button
static void DrawButton(SimpleButton* btn) {
    if (!btn->visible) return;

    if (btn->pressed) {
        BSP_LCD_SetTextColor(GUI_COLOR_BTN_PRESS);
    } else {
        BSP_LCD_SetTextColor(GUI_COLOR_BUTTON);
    }
    BSP_LCD_FillRect(btn->x, btn->y, btn->w, btn->h);

    BSP_LCD_SetTextColor(GUI_COLOR_TEXT);
    BSP_LCD_SetBackColor(btn->pressed ? GUI_COLOR_BTN_PRESS : GUI_COLOR_BUTTON);
    BSP_LCD_SetFont(&Font24);

    // Center Text
    // Font24 is 17 pixels wide roughly? Let's assume fixed width for simplicity or just center approximately
    // Font24 width is usually 17, height 24.
    int textLen = strlen(btn->label);
    int textWidth = textLen * 17;
    int tx = btn->x + (btn->w - textWidth) / 2;
    int ty = btn->y + (btn->h - 24) / 2;

    BSP_LCD_DisplayStringAt(tx, ty, (uint8_t*)btn->label, LEFT_MODE);
}

static void DrawHeader(void) {
    BSP_LCD_SetTextColor(GUI_COLOR_HEADER);
    BSP_LCD_FillRect(0, 0, BSP_LCD_GetXSize(), 50);

    BSP_LCD_SetTextColor(GUI_COLOR_TEXT);
    BSP_LCD_SetBackColor(GUI_COLOR_HEADER);
    BSP_LCD_SetFont(&Font24);
    BSP_LCD_DisplayStringAt(20, 13, (uint8_t*)"EcoFlow Controller", LEFT_MODE);
}

static void UpdateStatus(BatteryStatus* batt) {
    char buf[32];
    BSP_LCD_SetFont(&Font20);
    BSP_LCD_SetBackColor(GUI_COLOR_PANEL);

    // Panel 1: SOC
    BSP_LCD_SetTextColor(GUI_COLOR_PANEL);
    BSP_LCD_FillRect(20, 70, 200, 100);

    BSP_LCD_SetTextColor(GUI_COLOR_ACCENT);
    BSP_LCD_DisplayStringAt(40, 80, (uint8_t*)"Battery", LEFT_MODE);

    BSP_LCD_SetTextColor(GUI_COLOR_TEXT);
    snprintf(buf, sizeof(buf), "%d %%", batt->soc);
    BSP_LCD_SetFont(&Font24);
    BSP_LCD_DisplayStringAt(40, 110, (uint8_t*)buf, LEFT_MODE);

    // Panel 2: Power
    BSP_LCD_SetTextColor(GUI_COLOR_PANEL);
    BSP_LCD_FillRect(240, 70, 200, 100);

    BSP_LCD_SetTextColor(GUI_COLOR_ACCENT);
    BSP_LCD_SetFont(&Font20);
    BSP_LCD_DisplayStringAt(260, 80, (uint8_t*)"Power", LEFT_MODE);

    BSP_LCD_SetTextColor(GUI_COLOR_TEXT);
    snprintf(buf, sizeof(buf), "%d W", batt->power_w);
    BSP_LCD_SetFont(&Font24);
    BSP_LCD_DisplayStringAt(260, 110, (uint8_t*)buf, LEFT_MODE);
}

void StartDisplayTask(void * argument) {
    // Init Hardware
    BSP_SDRAM_Init();
    BSP_LCD_Init();
    BSP_LCD_LayerDefaultInit(LTDC_ACTIVE_LAYER_BACKGROUND, LCD_FB_START_ADDRESS);
    BSP_LCD_SelectLayer(LTDC_ACTIVE_LAYER_BACKGROUND);

    // Clear BG
    BSP_LCD_Clear(GUI_COLOR_BG);

    // Init Touch
    BSP_TS_Init(BSP_LCD_GetXSize(), BSP_LCD_GetYSize());

    Backlight_Init();

    displayQueue = xQueueCreate(10, sizeof(DisplayEvent));

    // Draw Initial UI
    DrawHeader();
    DrawButton(&testBtn);

    // Initial Status Placeholders
    BSP_LCD_SetTextColor(GUI_COLOR_PANEL);
    BSP_LCD_FillRect(20, 70, 200, 100); // SOC Box
    BSP_LCD_FillRect(240, 70, 200, 100); // Power Box

    DisplayEvent event;
    TS_StateTypeDef tsState;
    bool wasPressed = false;

    for (;;) {
        // Handle Data Updates
        if (xQueueReceive(displayQueue, &event, pdMS_TO_TICKS(20)) == pdTRUE) {
            if (event.type == DISPLAY_EVENT_UPDATE_BATTERY) {
                UpdateStatus(&event.data.battery);
            }
        }

        // Handle Touch
        BSP_TS_GetState(&tsState);
        bool isTouched = tsState.touchDetected;

        if (isTouched) {
            uint16_t tx = tsState.touchX[0];
            uint16_t ty = tsState.touchY[0];

            // Check Button Collision
            if (tx >= testBtn.x && tx <= (testBtn.x + testBtn.w) &&
                ty >= testBtn.y && ty <= (testBtn.y + testBtn.h)) {

                if (!testBtn.pressed) {
                    testBtn.pressed = true;
                    DrawButton(&testBtn); // Redraw Pressed
                }
            } else {
                // Dragged out
                if (testBtn.pressed) {
                    testBtn.pressed = false;
                    DrawButton(&testBtn);
                }
            }
        } else {
            // Release
            if (testBtn.pressed) {
                testBtn.pressed = false;
                DrawButton(&testBtn); // Redraw Released
            }
        }

        vTaskDelay(pdMS_TO_TICKS(30)); // Cap framerate/poll rate
    }
}
