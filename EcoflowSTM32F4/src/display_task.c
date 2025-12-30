#include "display_task.h"
#include "stm32469i_discovery_lcd.h"
#include "stm32469i_discovery_ts.h"
#include "ui/ui_core.h"
#include <stdio.h>
#include <string.h>
#include <stdbool.h>

// QueueHandle_t displayQueue; // Defined in main.c

// Backlight Control Pin: PA3
#define BACKLIGHT_PIN GPIO_PIN_3
#define BACKLIGHT_PORT GPIOA

// Double Buffering
#define LCD_FRAME_BUFFER_1  LCD_FB_START_ADDRESS
#define LCD_FRAME_BUFFER_2  (LCD_FB_START_ADDRESS + 0x200000) // 800*480*4 is ~1.5MB, so +2MB is safe

static uint32_t current_buffer = LCD_FRAME_BUFFER_1;
static uint32_t pending_buffer = LCD_FRAME_BUFFER_2;

// Cache data to redraw full frame
DeviceStatus currentDevStatus = {0};

// View State
typedef enum {
    VIEW_DASHBOARD,
    VIEW_SETTINGS
} ViewState;

static ViewState currentView = VIEW_DASHBOARD;

// External View Renderers
void UI_Dashboard_Init(void);
void UI_Render_Dashboard(DeviceStatus* dev);
bool UI_CheckTouch_Dashboard(uint16_t x, uint16_t y);
void UI_Render_Settings(void);
bool UI_HandleTouch_Settings(uint16_t x, uint16_t y, bool pressed);

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
    // 1. Set the pending buffer as the drawing target (but don't make it visible yet)
    hltdc_eval.LayerCfg[LTDC_ACTIVE_LAYER_BACKGROUND].FBStartAdress = pending_buffer;

    // 2. Draw everything
    if (currentView == VIEW_DASHBOARD) {
        UI_Render_Dashboard(&currentDevStatus);
    } else {
        UI_Render_Settings();
    }

    // 3. Request buffer swap
    HAL_LTDC_SetAddress(&hltdc_eval, pending_buffer, LTDC_ACTIVE_LAYER_BACKGROUND);

    // Trigger reload during Vertical Blanking to prevent tearing
    HAL_LTDC_Reload(&hltdc_eval, LTDC_RELOAD_VERTICAL_BLANKING);

    // 4. Wait for the reload to complete (poll the VBR bit)
    uint32_t tickstart = HAL_GetTick();
    int safety_count = 0;
    while(hltdc_eval.Instance->SRCR & LTDC_SRCR_VBR) {
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

    BSP_LCD_Clear(UI_COLOR_BG);
    BSP_LCD_SetBackColor(UI_COLOR_BG);
    BSP_LCD_SetTextColor(UI_COLOR_TEXT);
    BSP_LCD_DisplayOn();

    // Init Touch
    BSP_TS_Init(BSP_LCD_GetXSize(), BSP_LCD_GetYSize());

    Backlight_Init();

    // Init UI
    UI_Dashboard_Init();

    // Initial Draw
    RenderFrame();

    DisplayEvent event;
    TS_StateTypeDef tsState;
    bool needs_redraw = false;

    uint32_t last_touch_poll = 0;
    bool last_touch_detected = false;

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

                if (!last_touch_detected) {
                    // Touch Start (Click)
                    if (currentView == VIEW_DASHBOARD) {
                        if (UI_CheckTouch_Dashboard(tx, ty)) {
                            // Switch to Settings
                            currentView = VIEW_SETTINGS;
                            stateChanged = true;
                        }
                    } else {
                        if (UI_HandleTouch_Settings(tx, ty, true)) {
                            // Back pressed
                            currentView = VIEW_DASHBOARD;
                            stateChanged = true;
                        } else {
                            // Slider interaction (drag) - continuously redraw?
                            stateChanged = true;
                        }
                    }
                } else {
                    // Touch Move (Drag)
                    if (currentView == VIEW_SETTINGS) {
                         UI_HandleTouch_Settings(tx, ty, true);
                         stateChanged = true;
                    }
                }
            }

            last_touch_detected = isTouched;

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
