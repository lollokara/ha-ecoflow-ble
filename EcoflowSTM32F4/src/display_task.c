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
static void SetDrawTarget(uint32_t address) {
    // This updates the handle used by BSP functions, redirecting drawing
    hltdc_eval.LayerCfg[LTDC_ACTIVE_LAYER_BACKGROUND].FBStartAdress = address;
}

// Helper: Wait for Reload to complete (VSync)
static void WaitForReload() {
    // Wait until the VBR bit is cleared by hardware (meaning reload happened)
    // Add a timeout to prevent infinite blocking
    uint32_t timeout = 100; // ~100ms
    while ((hltdc_eval.Instance->SRCR & LTDC_SRCR_VBR) && timeout > 0) {
        vTaskDelay(pdMS_TO_TICKS(1));
        timeout--;
    }
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
    BSP_LCD_Clear(GUI_COLOR_BG);
    DrawHeader();
    DrawButton(&testBtn);
    DrawStatusPanel(&currentBattStatus);
}

void StartDisplayTask(void * argument) {
    BSP_SDRAM_Init();
    BSP_LCD_Init();

    // Init Buffer 1 (Visible)
    BSP_LCD_LayerDefaultInit(LTDC_ACTIVE_LAYER_BACKGROUND, LCD_FRAME_BUFFER_1);
    BSP_LCD_SelectLayer(LTDC_ACTIVE_LAYER_BACKGROUND);
    BSP_LCD_SetBackColor(GUI_COLOR_BG);
    BSP_LCD_SetTextColor(GUI_COLOR_TEXT);

    BSP_LCD_Clear(GUI_COLOR_BG);
    DrawHeader();
    DrawButton(&testBtn);
    DrawStatusPanel(&currentBattStatus);

    // Init Buffer 2 (Back)
    SetDrawTarget(LCD_FRAME_BUFFER_2);
    BSP_LCD_Clear(GUI_COLOR_BG);
    DrawHeader();
    DrawButton(&testBtn);
    DrawStatusPanel(&currentBattStatus);

    // Reset target to visible for safety
    SetDrawTarget(LCD_FRAME_BUFFER_1);

    BSP_LCD_DisplayOn();

    BSP_TS_Init(BSP_LCD_GetXSize(), BSP_LCD_GetYSize());
    Backlight_Init();

    displayQueue = xQueueCreate(10, sizeof(DisplayEvent));

    DisplayEvent event;
    TS_StateTypeDef tsState;
    uint32_t last_touch_poll = 0;

    for (;;) {
        bool needs_redraw = false;

        if (xQueueReceive(displayQueue, &event, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (event.type == DISPLAY_EVENT_UPDATE_BATTERY) {
                if (memcmp(&currentBattStatus, &event.data.battery, sizeof(BatteryStatus)) != 0) {
                    currentBattStatus = event.data.battery;
                    needs_redraw = true;
                }
            }
        }

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
            // Draw to back buffer
            SetDrawTarget(current_draw_buffer);
            RedrawFrame();

            // Swap visible buffer request
            BSP_LCD_SetLayerAddress(LTDC_ACTIVE_LAYER_BACKGROUND, current_draw_buffer);

            // Trigger reload at VSync
            HAL_LTDC_Reload(&hltdc_eval, LTDC_RELOAD_VERTICAL_BLANKING);

            // Wait for reload to happen before reusing buffers!
            WaitForReload();

            // Swap pointers
            uint32_t temp = current_visible_buffer;
            current_visible_buffer = current_draw_buffer;
            current_draw_buffer = temp;
        }

        vTaskDelay(pdMS_TO_TICKS(10));
    }
}
