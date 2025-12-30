#include "lv_port_disp.h"
#include "stm32469i_discovery_lcd.h"
#include "stm32469i_discovery_sdram.h"

// Define screen size
#define DISP_HOR_RES 800
#define DISP_VER_RES 480

static void disp_init(void);
static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p);

// Buffers for LVGL
// Use SDRAM for buffers to save internal RAM
// LCD_FRAME_BUFFER_1 is 0xC0000000.
// We need buffers for LVGL to render into.
// Let's allocate them in SDRAM after the main FrameBuffer.
// FB size = 800 * 480 * 4 = 1.5MB approx.
#define LV_BUF_ADDR_1 (LCD_FB_START_ADDRESS + 0x400000) // +4MB offset
#define LV_BUF_ADDR_2 (LCD_FB_START_ADDRESS + 0x600000)

void lv_port_disp_init(void)
{
    disp_init();

    static lv_disp_draw_buf_t draw_buf_dsc_1;
    // Use double buffering with full screen buffers for smoothest animation
    // lv_color_t is 32bit (4 bytes)
    // 800 * 480 * 4 = 1,536,000 bytes.
    // Ensure we point to SDRAM addresses.
    lv_color_t * buf_1 = (lv_color_t *)LV_BUF_ADDR_1;
    lv_color_t * buf_2 = (lv_color_t *)LV_BUF_ADDR_2;

    lv_disp_draw_buf_init(&draw_buf_dsc_1, buf_1, buf_2, DISP_HOR_RES * DISP_VER_RES);

    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);

    disp_drv.hor_res = DISP_HOR_RES;
    disp_drv.ver_res = DISP_VER_RES;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf_dsc_1;

    // Direct Mode: LVGL renders directly into the frame buffer
    // disp_drv.direct_mode = 1;
    // Wait, if we use direct mode, 'flush' doesn't copy, it just swaps.
    // But BSP_LCD functions expect a specific address.
    // For now, let's use standard partial/full buffering and copy in flush.
    // Actually, full double buffering (2 buffers of full screen size) allows direct swap if supported.
    // BSP_LCD_SetLayerAddress can swap.

    // Let's stick to simple copy first to ensure stability.
    // Double buffer full screen means flush is called with full area?
    // If we give 2 buffers of full size, LVGL treats them as "render targets".
    // Flush just needs to put the content on screen.
    // If we use "full_refresh = 1", LVGL redraws everything.

    // Optimization: Use partial buffer in internal RAM?
    // No, F469 has plenty of SDRAM.

    lv_disp_drv_register(&disp_drv);
}

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

    HAL_GPIO_WritePin(BACKLIGHT_PORT, BACKLIGHT_PIN, GPIO_PIN_SET);
}

static void disp_init(void)
{
    BSP_LCD_Init();
    BSP_LCD_LayerDefaultInit(0, LCD_FB_START_ADDRESS);
    BSP_LCD_SelectLayer(0);
    BSP_LCD_Clear(LCD_COLOR_BLACK);
    BSP_LCD_DisplayOn();
    Backlight_Init();
}

static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
    int32_t x, y;

    // Use DMA2D to copy from buffer to active LCD Framebuffer
    // Destination is always LCD_FB_START_ADDRESS (or whatever the active one is)
    // Actually, we should probably query the active layer address.
    uint32_t dest_addr = hltdc_eval.LayerCfg[0].FBStartAdress;

    // DMA2D Copy
    // Configure DMA2D
    // Width: area->x2 - area->x1 + 1
    // Height: area->y2 - area->y1 + 1
    uint32_t width = area->x2 - area->x1 + 1;
    uint32_t height = area->y2 - area->y1 + 1;
    uint32_t offLine = DISP_HOR_RES - width;

    hdma2d_eval.Init.Mode = DMA2D_M2M;
    hdma2d_eval.Init.ColorMode = DMA2D_ARGB8888;
    hdma2d_eval.Init.OutputOffset = offLine;

    if(HAL_DMA2D_Init(&hdma2d_eval) == HAL_OK)
    {
        if(HAL_DMA2D_ConfigLayer(&hdma2d_eval, 1) == HAL_OK)
        {
            hdma2d_eval.LayerCfg[1].InputOffset = 0; // Source is dense
            hdma2d_eval.LayerCfg[1].InputColorMode = DMA2D_ARGB8888;

            if (HAL_DMA2D_Start(&hdma2d_eval, (uint32_t)color_p, dest_addr + 4*(area->y1*DISP_HOR_RES + area->x1), width, height) == HAL_OK)
            {
                HAL_DMA2D_PollForTransfer(&hdma2d_eval, 10);
            }
        }
    }
    else
    {
        // Fallback CPU Copy
        for(y = area->y1; y <= area->y2; y++) {
            for(x = area->x1; x <= area->x2; x++) {
                // BSP_LCD_DrawPixel(x, y, color_p->full); // Slow
                 *(__IO uint32_t*) (dest_addr + (4*(y*DISP_HOR_RES + x))) = color_p->full;
                color_p++;
            }
        }
    }

    lv_disp_flush_ready(disp_drv);
}
