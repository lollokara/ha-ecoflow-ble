#include "display_task.h"
#include "lvgl.h"
#include "stm32469i_discovery_lcd.h"
#include "stm32469i_discovery_ts.h"
#include "ui_app.h"

// Framebuffer addresses in SDRAM
#define FB_ADDR_1 LCD_FB_START_ADDRESS
#define FB_ADDR_2 (LCD_FB_START_ADDRESS + 0x200000)

// Extern handle for DMA2D (defined in BSP drivers usually, but needed here)
extern DMA2D_HandleTypeDef hdma2d_eval;

/* Flush the content of the internal buffer the specific area on the display
 * Uses DMA2D for hardware acceleration. */
static void disp_flush(lv_display_t * disp_drv, const lv_area_t * area, uint8_t * px_map)
{
    int32_t width = lv_area_get_width(area);
    int32_t height = lv_area_get_height(area);

    // Calculate destination address in the active framebuffer
    uint32_t dest_addr = hltdc_eval.LayerCfg[LTDC_ACTIVE_LAYER_BACKGROUND].FBStartAdress +
                         4 * (area->y1 * 800 + area->x1);

    // Use DMA2D to copy from internal SRAM buffer to SDRAM framebuffer
    DMA2D_HandleTypeDef *hdma2d = &hdma2d_eval;

    hdma2d->Init.Mode = DMA2D_M2M;
    hdma2d->Init.ColorMode = DMA2D_ARGB8888;
    hdma2d->Init.OutputOffset = 800 - width; // Stride at destination

    // Foreground (Source)
    hdma2d->LayerCfg[1].InputOffset = 0;
    hdma2d->LayerCfg[1].InputColorMode = DMA2D_ARGB8888;
    hdma2d->LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;

    HAL_DMA2D_Init(hdma2d);
    HAL_DMA2D_ConfigLayer(hdma2d, 1);

    HAL_DMA2D_Start(hdma2d, (uint32_t)px_map, dest_addr, width, height);
    HAL_DMA2D_PollForTransfer(hdma2d, 10);

    lv_display_flush_ready(disp_drv);
}

/* Read the touchpad */
static void touchpad_read(lv_indev_t * indev_drv, lv_indev_data_t * data)
{
    TS_StateTypeDef tsState;
    BSP_TS_GetState(&tsState);

    if(tsState.touchDetected) {
        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = tsState.touchX[0];
        data->point.y = tsState.touchY[0];
    } else {
        data->state = LV_INDEV_STATE_RELEASED;
    }
}

static void Backlight_Init(void) {
    // PA3
    __HAL_RCC_GPIOA_CLK_ENABLE();
    GPIO_InitTypeDef g = {0};
    g.Pin = GPIO_PIN_3;
    g.Mode = GPIO_MODE_OUTPUT_PP;
    g.Pull = GPIO_NOPULL;
    g.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOA, &g);
    HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
}

void StartDisplayTask(void * argument) {
    // 1. Hardware Init
    BSP_SDRAM_Init();
    BSP_LCD_Init();
    BSP_LCD_LayerDefaultInit(LTDC_ACTIVE_LAYER_BACKGROUND, FB_ADDR_1);
    BSP_LCD_SelectLayer(LTDC_ACTIVE_LAYER_BACKGROUND);
    BSP_LCD_Clear(LCD_COLOR_WHITE);
    BSP_LCD_DisplayOn();

    BSP_TS_Init(800, 480);
    Backlight_Init();

    // 2. LVGL Init
    lv_init();

    // 3. Display Driver
    lv_display_t * disp = lv_display_create(800, 480);

    // Allocate draw buffer in Internal SRAM
    // 800px * 10 lines * 4 bytes/px = 32KB. Fits.
    static uint32_t draw_buf[800 * 10];

    lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(disp, disp_flush);

    // 4. Input Driver
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, touchpad_read);

    // 5. Create UI
    UI_Init(); // In ui_app.c

    DisplayEvent event;

    for(;;) {
        // Handle Events from main logic
        if (xQueueReceive(displayQueue, &event, 0) == pdTRUE) {
            if (event.type == DISPLAY_EVENT_UPDATE_BATTERY) {
                UI_Update(&event.data.deviceStatus);
            }
        }

        lv_timer_handler(); // Task handler
        vTaskDelay(pdMS_TO_TICKS(5)); // Yield
    }
}
