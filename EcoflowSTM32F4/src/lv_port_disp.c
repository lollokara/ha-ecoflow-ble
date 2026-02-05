#include "lv_port_disp.h"
#include "stm32469i_discovery_lcd.h"
#include "stm32469i_discovery_sdram.h"
#include "stm32f4xx_hal.h"

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

    // Direct Mode: LVGL renders directly into the frame buffer.
    // Full Refresh: LVGL renders the whole screen, not just dirty areas.
    // This is required for true double buffering where we swap buffers.
    disp_drv.direct_mode = 1;
    disp_drv.full_refresh = 1;

    lv_disp_drv_register(&disp_drv);
}

static void disp_init(void)
{
    BSP_LCD_Init();
    BSP_LCD_LayerDefaultInit(0, LCD_FB_START_ADDRESS);
    BSP_LCD_SelectLayer(0);
    BSP_LCD_Clear(LCD_COLOR_BLACK);
    BSP_LCD_DisplayOn();

    // Initialize DMA2D once (if used by LVGL internally)
    hdma2d_eval.Init.Mode = DMA2D_M2M;
    hdma2d_eval.Init.ColorMode = DMA2D_ARGB8888;
    hdma2d_eval.Init.OutputOffset = 0;
    HAL_DMA2D_Init(&hdma2d_eval);
    HAL_DMA2D_ConfigLayer(&hdma2d_eval, 1);
}

static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
    // With direct_mode, we swap the frame buffer address when the frame is complete
    if (lv_disp_flush_is_last(disp_drv))
    {
        // Update LTDC Layer 1 Address to the new buffer (which is color_p)
        // We use direct register access to avoid HAL overhead and ensure we control the reload type.
        // LTDC_Layer1 corresponds to Layer Index 0 in BSP.
        LTDC_Layer1->CFBAR = (uint32_t)color_p;

        // Also update the handle state so HAL functions don't get out of sync if used later
        hltdc_eval.LayerCfg[0].FBStartAdress = (uint32_t)color_p;

        // Trigger Reload on Vertical Blanking (VBR)
        // Note: HAL_LTDC_Reload usually sets IMR (Immediate). We want VBR.
        // SRCR register: Bit 1 = VBR, Bit 0 = IMR.
        hltdc_eval.Instance->SRCR = LTDC_SRCR_VBR;

        // Wait for VSYNC (Reload to take effect)
        // This ensures we don't start drawing the next frame into the buffer
        // that is still being displayed (until VSYNC happens).
        // The VBR bit is cleared by hardware when the reload occurs.
        uint32_t tickstart = HAL_GetTick();
        while((hltdc_eval.Instance->SRCR & LTDC_SRCR_VBR) != 0)
        {
            if((HAL_GetTick() - tickstart) > 50) break; // Timeout 50ms
        }
    }

    lv_disp_flush_ready(disp_drv);
}
