#include "lv_port_disp.h"
#include "stm32469i_discovery_lcd.h"
#include "stm32f4xx_hal.h"

/* Display buffer size: 1/10th of screen usually sufficient */
#define DISP_HOR_RES 800
#define DISP_VER_RES 480
#define DRAW_BUF_SIZE (DISP_HOR_RES * 40)

static lv_disp_drv_t disp_drv;
static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf1[DRAW_BUF_SIZE];

static void disp_flush(lv_disp_drv_t * disp_drv, const lv_area_t * area, lv_color_t * color_p)
{
    int32_t x1 = area->x1;
    int32_t y1 = area->y1;
    int32_t w = lv_area_get_width(area);
    int32_t h = lv_area_get_height(area);

    /* Use BSP DrawBitmap or DMA2D for faster transfer.
       For simplicity initially, we can use a loop or generic drawing.
       However, the BSP provides `BSP_LCD_DrawBitmap` which expects a full bitmap.
       A more efficient way is using DMA2D directly to copy line by line or the whole block
       to the frame buffer at `LCD_FB_START_ADDRESS`.
    */
    
    uint32_t dest_addr = LCD_FB_START_ADDRESS + 4 * (y1 * DISP_HOR_RES + x1);
    
    /* Configure DMA2D */
    /* We can use the BSP global `hdma2d_eval` if accessible, or just use a simple copy loop for first pass to ensure stability. */
    /* Implementing a simple copy loop: */
    
    uint32_t *dest = (uint32_t*)dest_addr;
    uint32_t *src = (uint32_t*)color_p;
    
    for(int y = 0; y < h; y++) {
        for(int x = 0; x < w; x++) {
            dest[x] = src[x];
        }
        dest += DISP_HOR_RES;
        src += w;
    }

    lv_disp_flush_ready(disp_drv);
}

void lv_port_disp_init(void)
{
    lv_disp_draw_buf_init(&draw_buf, buf1, NULL, DRAW_BUF_SIZE);

    lv_disp_drv_init(&disp_drv);
    disp_drv.hor_res = DISP_HOR_RES;
    disp_drv.ver_res = DISP_VER_RES;
    disp_drv.flush_cb = disp_flush;
    disp_drv.draw_buf = &draw_buf;
    
    lv_disp_drv_register(&disp_drv);
}
