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

// Double Buffering
#define LCD_FRAME_BUFFER_1  LCD_FB_START_ADDRESS
#define LCD_FRAME_BUFFER_2  (LCD_FB_START_ADDRESS + 0x200000) // +2MB

static uint32_t current_visible_buffer = LCD_FRAME_BUFFER_1;
static uint32_t current_draw_buffer = LCD_FRAME_BUFFER_2;

// Simple Button Struct
typedef struct {
    uint16_t x, y, w, h;
    char label[16];
    bool pressed;
    bool visible;
} SimpleButton;

SimpleButton testBtn = {250, 200, 200, 60, "TOGGLE", false, true};
BatteryStatus currentBattStatus = {0};

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

// Helper: Set Drawing Target
// This hacks the BSP to draw to a specific address without changing what is visible (yet)
static void SetDrawTarget(uint32_t address) {
    hltdc_eval.LayerCfg[LTDC_ACTIVE_LAYER_BACKGROUND].FBStartAdress = address;
}

// Helper: Draw Button
static void DrawButton(SimpleButton* btn) {
    if (!btn->visible) return;

    // Use FillRect now, since we are double buffering!
    if (btn->pressed) {
        BSP_LCD_SetTextColor(GUI_COLOR_BTN_PRESS);
    } else {
        BSP_LCD_SetTextColor(GUI_COLOR_BUTTON);
    }
    BSP_LCD_FillRect(btn->x, btn->y, btn->w, btn->h);

    BSP_LCD_SetTextColor(GUI_COLOR_TEXT);
    BSP_LCD_SetBackColor(btn->pressed ? GUI_COLOR_BTN_PRESS : GUI_COLOR_BUTTON);
    BSP_LCD_SetFont(&Font24);

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

static void DrawStatusPanel(BatteryStatus* batt) {
    char buf[32];
    BSP_LCD_SetFont(&Font20);
    BSP_LCD_SetBackColor(GUI_COLOR_PANEL);

    // Panel 1: SOC
    // With double buffering, we can safely FillRect to clear old text without flicker
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

static void RedrawFrame() {
    // Assumes SetDrawTarget is already called for the back buffer
    // Only clear if we need to? Or just redraw panels?
    // Drawing Header is slow? No, simple rects.
    // To be safe against artifacts, we might want to Draw Header too, or ensure it's preserved.
    // If we only draw Panels and Button, the rest of the screen needs to be valid.
    // Strategy: We draw Header to BOTH buffers at INIT.
    // In Loop: We only redraw Panels and Button. The background (Black) remains valid.
    // EXCEPT if a button or panel changes size or position. They don't.
    // So we just redraw the dynamic widgets.

    DrawStatusPanel(&currentBattStatus);
    DrawButton(&testBtn);
}

void StartDisplayTask(void * argument) {
    // Init Hardware
    BSP_SDRAM_Init();
    BSP_LCD_Init();

    // Init Buffer 1
    BSP_LCD_LayerDefaultInit(LTDC_ACTIVE_LAYER_BACKGROUND, LCD_FRAME_BUFFER_1);
    BSP_LCD_SelectLayer(LTDC_ACTIVE_LAYER_BACKGROUND);
    BSP_LCD_SetBackColor(GUI_COLOR_BG);
    BSP_LCD_SetTextColor(GUI_COLOR_TEXT);

    // Clear and Setup Buffer 1
    BSP_LCD_Clear(GUI_COLOR_BG);
    DrawHeader();
    DrawButton(&testBtn);
    DrawStatusPanel(&currentBattStatus);

    // Init Buffer 2 content
    // We hack the draw target to Buffer 2
    SetDrawTarget(LCD_FRAME_BUFFER_2);
    // We must manually clear it as BSP_LCD_Clear uses the target
    BSP_LCD_Clear(GUI_COLOR_BG);
    DrawHeader();
    DrawButton(&testBtn);
    DrawStatusPanel(&currentBattStatus);

    // Restore Target to Buffer 1 (Visible)
    SetDrawTarget(LCD_FRAME_BUFFER_1);

    BSP_LCD_DisplayOn();

    // Init Touch
    BSP_TS_Init(BSP_LCD_GetXSize(), BSP_LCD_GetYSize());
    Backlight_Init();

    displayQueue = xQueueCreate(10, sizeof(DisplayEvent));

    DisplayEvent event;
    TS_StateTypeDef tsState;
    uint32_t last_touch_poll = 0;

    for (;;) {
        bool needs_redraw = false;

        // Process Queue
        if (xQueueReceive(displayQueue, &event, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (event.type == DISPLAY_EVENT_UPDATE_BATTERY) {
                // Update cached state
                if (memcmp(&currentBattStatus, &event.data.battery, sizeof(BatteryStatus)) != 0) {
                    currentBattStatus = event.data.battery;
                    needs_redraw = true;
                }
            }
        }

        // Process Touch
        if (xTaskGetTickCount() - last_touch_poll > pdMS_TO_TICKS(30)) {
            last_touch_poll = xTaskGetTickCount();
            BSP_TS_GetState(&tsState);
            bool isTouched = tsState.touchDetected;

            if (isTouched) {
                uint16_t tx = tsState.touchX[0];
                uint16_t ty = tsState.touchY[0];
                if (tx >= testBtn.x && tx <= (testBtn.x + testBtn.w) &&
                    ty >= testBtn.y && ty <= (testBtn.y + testBtn.h)) {
                    if (!testBtn.pressed) {
                        testBtn.pressed = true;
                        needs_redraw = true;
                    }
                } else {
                    if (testBtn.pressed) {
                        testBtn.pressed = false;
                        needs_redraw = true;
                    }
                }
            } else {
                if (testBtn.pressed) {
                    testBtn.pressed = false;
                    needs_redraw = true;
                }
            }
        }

        if (needs_redraw) {
            // Draw to Back Buffer
            SetDrawTarget(current_draw_buffer);

            // We MUST Redraw everything that might have changed or been cleared?
            // Since we are double buffering, the back buffer has "Old" state (from 2 frames ago).
            // We just overwrite the dynamic parts.
            RedrawFrame();

            // Swap!
            BSP_LCD_SetLayerAddress(LTDC_ACTIVE_LAYER_BACKGROUND, current_draw_buffer);

            // Swap indices
            uint32_t temp = current_visible_buffer;
            current_visible_buffer = current_draw_buffer;
            current_draw_buffer = temp;
        }

        // Limit FPS
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
