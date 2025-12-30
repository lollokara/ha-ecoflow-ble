#include "lv_port_disp.h"
#include "stm32469i_discovery_lcd.h"
#include "stm32469i_discovery_sdram.h"
#include "backlight.h"

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
    // Actually, full double buffering (2 buffers of full size, LVGL treats them as "render targets".
    // Flush just needs to put the content on screen.
    // If we use "full_refresh = 1", LVGL redraws everything.

    // Optimization: Use partial buffer in internal RAM?
    // No, F469 has plenty of SDRAM.

    lv_disp_drv_register(&disp_drv);
}

static void disp_init(void)
{
    BSP_LCD_Init();
    BSP_LCD_LayerDefaultInit(0, LCD_FB_START_ADDRESS);
    BSP_LCD_SelectLayer(0);
    BSP_LCD_Clear(LCD_COLOR_BLACK);
    BSP_LCD_DisplayOn();
    Backlight_Init(); // Use PWM Backlight

    // Initialize DMA2D once
    hdma2d_eval.Init.Mode = DMA2D_M2M;
    hdma2d_eval.Init.ColorMode = DMA2D_ARGB8888;
    hdma2d_eval.Init.OutputOffset = 0; // Will be updated in flush
    HAL_DMA2D_Init(&hdma2d_eval);
    HAL_DMA2D_ConfigLayer(&hdma2d_eval, 1);
    hdma2d_eval.LayerCfg[1].InputOffset = 0;
    hdma2d_eval.LayerCfg[1].InputColorMode = DMA2D_ARGB8888;
}

static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
    // Use DMA2D to copy from buffer to active LCD Framebuffer
    uint32_t dest_addr = hltdc_eval.LayerCfg[0].FBStartAdress;

    // Calculate destination address
    uint32_t dest_address = dest_addr + 4 * (area->y1 * DISP_HOR_RES + area->x1);

    // Width: area->x2 - area->x1 + 1
    // Height: area->y2 - area->y1 + 1
    uint32_t width = area->x2 - area->x1 + 1;
    uint32_t height = area->y2 - area->y1 + 1;
    uint32_t offLine = DISP_HOR_RES - width;

    // Update OutputOffset
    hdma2d_eval.Init.OutputOffset = offLine;
    // We must re-init/config if we change Init structure?
    // HAL_DMA2D_Init calls HAL_DMA2D_MspInit.
    // To be safe and fast, we can access registers directly or use HAL_DMA2D_Init.
    // But since we want to avoid re-init overhead, let's write the register.
    // hdma2d_eval.Instance->OOR = offLine;

    // However, for stability, let's stick to HAL but skip full MSP init if possible.
    // Re-calling Init is safer than direct register access if we don't know the state machine.
    // Optimization: Just call Init, assuming it's fast enough if MSP is already done.
    // Actually, the main issue might be re-entrancy or state.

    // Let's try minimal reconfiguration.
    HAL_DMA2D_Init(&hdma2d_eval);
    // Note: ConfigLayer is needed for layer 1 (foreground/source)?
    // Yes, but we set it in disp_init. Does Init reset it? Yes, Init resets handle state.

    // Let's stick to the safe path but move big init out if possible.
    // If Init is required to change OutputOffset via HAL, we must do it.
    // BUT, we can simplify the flush function.

    if (HAL_DMA2D_ConfigLayer(&hdma2d_eval, 1) == HAL_OK)
    {
         if (HAL_DMA2D_Start(&hdma2d_eval, (uint32_t)color_p, dest_address, width, height) == HAL_OK)
         {
             HAL_DMA2D_PollForTransfer(&hdma2d_eval, 10);
         }
    }

    lv_disp_flush_ready(disp_drv);
}
