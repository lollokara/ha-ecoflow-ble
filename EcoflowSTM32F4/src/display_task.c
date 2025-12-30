#include "display_task.h"
#include "stm32469i_discovery_lcd.h"
#include "stm32469i_discovery_ts.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// QueueHandle_t displayQueue; // Defined in main.c

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
// Use DeviceStatus to store ID + Data
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
    BSP_LCD_DisplayStringAt(20, 13, (uint8_t*)"Ecoflow Controller", LEFT_MODE);
}

// Draws the static background of the status panels
static void InitStatusPanel(void) {
    BSP_LCD_SetFont(&Font20);
    BSP_LCD_SetBackColor(GUI_COLOR_PANEL);

    // Panel 1: SOC Background
    BSP_LCD_SetTextColor(GUI_COLOR_PANEL);
    BSP_LCD_FillRect(20, 70, 200, 100);

    BSP_LCD_SetTextColor(GUI_COLOR_ACCENT);
    BSP_LCD_DisplayStringAt(40, 80, (uint8_t*)"Battery", LEFT_MODE);

    // Panel 2: Power Background
    BSP_LCD_SetTextColor(GUI_COLOR_PANEL);
    BSP_LCD_FillRect(240, 70, 200, 100);

    BSP_LCD_SetTextColor(GUI_COLOR_ACCENT);
    BSP_LCD_SetFont(&Font20);
    BSP_LCD_DisplayStringAt(260, 80, (uint8_t*)"Power", LEFT_MODE);
}

// Updates only the dynamic values in the status panels
static void UpdateStatusPanel(DeviceStatus* dev) {
    char buf[32];
    BatteryStatus* batt = &dev->status;

    printf("DISPLAY: Updating Panel. Device=%d, SOC=%d\n", dev->id, batt->soc);

    // Update Header with Device Name if available
    if (batt->device_name[0] != 0) {
        BSP_LCD_SetTextColor(GUI_COLOR_TEXT);
        BSP_LCD_SetBackColor(GUI_COLOR_HEADER);
        BSP_LCD_SetFont(&Font24);
        // Clear previous name area? Assuming simple overwrite for now or header redraw handles it.
        // Actually RenderFrame redraws header every time, so just draw name.
        // But DrawHeader is called before this.
        // Let's just overlay name.
        BSP_LCD_DisplayStringAt(300, 13, (uint8_t*)batt->device_name, LEFT_MODE);
    }


    // Panel 1: SOC Value
    // Clear only the text area to prevent flicker
    BSP_LCD_SetTextColor(GUI_COLOR_PANEL);
    BSP_LCD_FillRect(40, 110, 160, 24);

    BSP_LCD_SetTextColor(GUI_COLOR_TEXT);
    BSP_LCD_SetBackColor(GUI_COLOR_PANEL);
    snprintf(buf, sizeof(buf), "%d %%", batt->soc);
    BSP_LCD_SetFont(&Font24);
    BSP_LCD_DisplayStringAt(40, 110, (uint8_t*)buf, LEFT_MODE);

    // Panel 2: Power Value
    // Clear only the text area to prevent flicker
    BSP_LCD_SetTextColor(GUI_COLOR_PANEL);
    BSP_LCD_FillRect(260, 110, 160, 24);

    BSP_LCD_SetTextColor(GUI_COLOR_TEXT);
    BSP_LCD_SetBackColor(GUI_COLOR_PANEL);
    snprintf(buf, sizeof(buf), "%d W", batt->power_w);
    BSP_LCD_SetFont(&Font24);
    BSP_LCD_DisplayStringAt(260, 110, (uint8_t*)buf, LEFT_MODE);
}

static void RenderFrame() {
    // 1. Set the pending buffer as the drawing target (but don't make it visible yet)
    // We update the handle's config so BSP functions draw to pending_buffer,
    // but we DO NOT call HAL_LTDC_SetAddress which would make it visible immediately.
    hltdc_eval.LayerCfg[LTDC_ACTIVE_LAYER_BACKGROUND].FBStartAdress = pending_buffer;

    // 2. Draw everything
    BSP_LCD_Clear(GUI_COLOR_BG);
    DrawHeader();
    DrawButton(&testBtn);
    InitStatusPanel();
    UpdateStatusPanel(&currentDevStatus);

    // 3. Request buffer swap
    // Set the layer configuration to point to the pending buffer
    HAL_LTDC_SetAddress(&hltdc_eval, pending_buffer, LTDC_ACTIVE_LAYER_BACKGROUND);

    // Trigger reload during Vertical Blanking to prevent tearing
    HAL_LTDC_Reload(&hltdc_eval, LTDC_RELOAD_VERTICAL_BLANKING);

    // 4. Wait for the reload to complete (poll the VBR bit)
    // The VBR bit is cleared when the reload has happened (at VSYNC)
    uint32_t tickstart = HAL_GetTick();
    int safety_count = 0;
    while(hltdc_eval.Instance->SRCR & LTDC_SRCR_VBR) {
        // Busy wait (short duration usually)
        if((HAL_GetTick() - tickstart) > 50) {
            printf("DISPLAY: VSYNC Timeout!\n");
            break;
        }
        safety_count++;
        if (safety_count > 1000000) {
             printf("DISPLAY: VSYNC Loop Stuck! Force break.\n");
             break;
        }
    }

    // 5. Swap buffer tracking variables
    uint32_t temp = current_buffer;
    current_buffer = pending_buffer;
    pending_buffer = temp;

    // Ensure the BSP drawing functions target the new pending buffer for next time
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

    // Init Touch
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
                // Check if changed
                if (memcmp(&currentDevStatus, &event.data.deviceStatus, sizeof(DeviceStatus)) != 0) {
                     currentDevStatus = event.data.deviceStatus;
                     needs_redraw = true;
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
                needs_redraw = true;
            }
        }

        if (needs_redraw) {
            RenderFrame();
            needs_redraw = false;
        }
    }
}
