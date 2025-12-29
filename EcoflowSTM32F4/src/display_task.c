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
#define LCD_FRAME_BUFFER_2  (LCD_FB_START_ADDRESS + 0x200000) // 800*480*4 is ~1.5MB, so +2MB is safe

static uint32_t current_buffer = LCD_FRAME_BUFFER_1;
static uint32_t pending_buffer = LCD_FRAME_BUFFER_2;

// Simple Button Struct
typedef struct {
    uint16_t x, y, w, h;
    char label[16];
    bool pressed;
    bool visible;
} SimpleButton;

// Global UI State
SimpleButton testBtn = {250, 200, 200, 60, "TOGGLE", false, true};
// Cache data to redraw full frame
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

static void RenderFrame() {
    // 1. Draw to Pending Buffer
    BSP_LCD_SelectLayer(LTDC_ACTIVE_LAYER_BACKGROUND);

    // We are drawing to the memory currently set as Layer Address?
    // Wait, BSP_LCD_SelectLayer just sets the register index for subsequent calls.
    // We need to tell the drawing primitives WHERE to write.
    // The BSP drivers usually write to the address of the selected layer.
    // So we must update the layer address BEFORE drawing?
    // No, if we update layer address, the LCD controller will scan from there immediately (or at Vsync).
    // Double buffering means:
    // A. LCD scans Buffer 1.
    // B. CPU writes to Buffer 2.
    // C. Swap: LCD scans Buffer 2.

    // The BSP functions `BSP_LCD_Draw...` write to the address configured in the handle `hltdc_eval.LayerCfg[LayerIndex].FBStartAdress`.
    // But `BSP_LCD_SetLayerAddress` updates the LTDC register AND likely the handle handle.
    // If we call SetLayerAddress, we change what is ON SCREEN.
    // We want to change what we DRAW TO, without changing screen yet.

    // The standard BSP doesn't separate "Draw Buffer" and "Display Buffer".
    // We have to manually hack it or use a simpler approach:
    // 1. Set Layer Address to PENDING (this shows garbage for a split second if not careful).
    // Actually, normally you write to memory manually, or you trick the library.

    // Let's rely on the fact that if we update the address, it takes effect at next reload.
    // But we need to write to the pending buffer.
    // `BSP_LCD_SetLayerAddress` usually calls `HAL_LTDC_SetAddress`.

    // To implement true double buffering with this BSP:
    // It is complex without modifying BSP.
    // Simple fix for tearing: Just rely on the fact that we are drawing fast and Vsync might handle it?
    // No, user said "glitchy".

    // Let's try this:
    // We can't easily re-target the BSP drawing functions to an off-screen buffer without changing what's displayed
    // because the BSP tracks "Current Layer" and uses its address.

    // Workaround:
    // We can't do true double buffering easily with this BSP API without flicker.
    // BUT, we can minimize tearing by waiting for VSync (Reload).
    // `HAL_LTDC_Reload(&hltdc_eval, LTDC_RELOAD_VERTICAL_BLANKING);`

    // Re-reading usage: `BSP_LCD_Init` sets Layer 0 to `LCD_FB_START_ADDRESS`.

    // Let's just try to redraw only what changed, which we are doing.
    // Maybe the glitch is because we clear rects.
    // Filling rect is fast.

    // If the user persists with "glitchy", let's assume it IS tearing.
    // Let's just draw to the SAME buffer but carefully.

    // Actually, I can update the BSP handle's address pointer manually?
    // extern LTDC_HandleTypeDef  hltdc_discovery; (in bsp.c)
    // It's hidden.

    // OK, let's stick to the current plan but ensure we clear background only once or use the "Modern" full redraw.
    // Wait, I will attempt to switch the layer address.
    // If I set the Layer Address to the *other* buffer, the screen *switches* to it.
    // So I must have *already* drawn to it.
    // But how do I draw to it if it's not the active layer?

    // The BSP has `BSP_LCD_SelectLayer`. This sets `ActiveLayer`.
    // And drawing uses `hltdc_eval.LayerCfg[ActiveLayer].FBStartAdress`.
    // So if I call `BSP_LCD_SetLayerAddress(Active, PENDING)`, the screen updates to PENDING.

    // CORRECT SEQUENCE for this BSP:
    // 1. Initialize with Buffer 1 on Screen.
    // 2. We want to draw to Buffer 2.
    //    We need to "Select" Buffer 2 as target, but NOT show it.
    //    The BSP doesn't support "Select Target but don't Show".
    //    When you select layer, you select the index (0 or 1).
    //    This board has 2 layers (Background/Foreground) used for blending.
    //    We are using Layer 0.

    //    Maybe we can use Layer 1 as the back buffer?
    //    No, Layer 1 is for blending on top.

    //    We need to write to RAM `LCD_FRAME_BUFFER_2` directly?
    //    We can't use BSP functions then.

    //    Okay, let's look at `BSP_LCD_SetLayerAddress`.
    //    It sets the address for the layer.

    //    Revised Plan for Double Buffering:
    //    Actually, we can't easily do it without low-level hacks.
    //    Let's focus on "glitchy". Maybe it's just the massive flickering of `FillRect`.
    //    The current code redraws the button and status panels constantly.

    //    Refactor to REDRAW ONLY ON CHANGE.

    BSP_LCD_Clear(GUI_COLOR_BG);
    DrawHeader();
    DrawButton(&testBtn);
    DrawStatusPanel(&currentBattStatus);
}

void StartDisplayTask(void * argument) {
    // Init Hardware
    BSP_SDRAM_Init();
    BSP_LCD_Init();
    BSP_LCD_LayerDefaultInit(LTDC_ACTIVE_LAYER_BACKGROUND, LCD_FRAME_BUFFER_1);
    BSP_LCD_SelectLayer(LTDC_ACTIVE_LAYER_BACKGROUND);

    BSP_LCD_Clear(GUI_COLOR_BG);
    BSP_LCD_SetBackColor(GUI_COLOR_BG);
    BSP_LCD_SetTextColor(GUI_COLOR_TEXT);
    BSP_LCD_DisplayOn();

    // Init Touch
    BSP_TS_Init(BSP_LCD_GetXSize(), BSP_LCD_GetYSize());

    Backlight_Init();

    displayQueue = xQueueCreate(10, sizeof(DisplayEvent));

    // Initial Draw
    DrawHeader();
    DrawButton(&testBtn);
    DrawStatusPanel(&currentBattStatus);

    DisplayEvent event;
    TS_StateTypeDef tsState;

    uint32_t last_touch_poll = 0;

    for (;;) {
        // Handle Data Updates
        if (xQueueReceive(displayQueue, &event, pdMS_TO_TICKS(10)) == pdTRUE) {
            if (event.type == DISPLAY_EVENT_UPDATE_BATTERY) {
                // Check if changed
                if (memcmp(&currentBattStatus, &event.data.battery, sizeof(BatteryStatus)) != 0) {
                     currentBattStatus = event.data.battery;
                     DrawStatusPanel(&currentBattStatus);
                }
            }
        }

        if (xTaskGetTickCount() - last_touch_poll > pdMS_TO_TICKS(30)) {
            last_touch_poll = xTaskGetTickCount();

            BSP_TS_GetState(&tsState);
            bool isTouched = tsState.touchDetected;
            bool stateChanged = false;

            if (isTouched) {
                uint16_t tx = tsState.touchX[0];
                uint16_t ty = tsState.touchY[0];

                if (tx >= testBtn.x && tx <= (testBtn.x + testBtn.w) &&
                    ty >= testBtn.y && ty <= (testBtn.y + testBtn.h)) {

                    if (!testBtn.pressed) {
                        testBtn.pressed = true;
                        stateChanged = true;
                    }
                } else {
                    if (testBtn.pressed) {
                        testBtn.pressed = false;
                        stateChanged = true;
                    }
                }
            } else {
                if (testBtn.pressed) {
                    testBtn.pressed = false;
                    stateChanged = true;
                }
            }

            if (stateChanged) {
                DrawButton(&testBtn);
            }
        }
    }
}
