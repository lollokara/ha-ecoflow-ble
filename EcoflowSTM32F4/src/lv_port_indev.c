#include "lv_port_indev.h"
#include "stm32469i_discovery_ts.h"
#include <stdbool.h>
#include "ui/ui_lvgl.h"

static void touchpad_init(void);
static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);

static volatile bool touch_irq_triggered = false;

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == TS_INT_PIN) {
        touch_irq_triggered = true;
    }
}

void lv_port_indev_init(void)
{
    static lv_indev_drv_t indev_drv;

    touchpad_init();

    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    lv_indev_drv_register(&indev_drv);
}

static void touchpad_init(void)
{
    // BSP_TS_Init is called in StartDisplayTask
    // Configure Touch Interrupts
    BSP_TS_ITConfig();
}

// Calibration Values
// TL: 12, 14 -> 0, 0
// TR: 799, 15 -> 800, 0
// BL: 10, 432 -> 0, 480
// BR: 796, 373 (Outlier?) -> Using BL/TR logic:
// X range: ~11 to ~798 mapping to 0-800
// Y range: ~14 to ~432 mapping to 0-480

static int32_t map(int32_t x, int32_t in_min, int32_t in_max, int32_t out_min, int32_t out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    static TS_StateTypeDef  TS_State;

    // Check if interrupt triggered or pin is active (Low)
    if (touch_irq_triggered || HAL_GPIO_ReadPin(TS_INT_GPIO_PORT, TS_INT_PIN) == GPIO_PIN_RESET) {
        touch_irq_triggered = false;
        BSP_TS_GetState(&TS_State);
    } else {
        TS_State.touchDetected = 0;
    }

    if(TS_State.touchDetected) {
        // Reset Idle Timer on any physical touch detection
        UI_ResetIdleTimer();

        data->state = LV_INDEV_STATE_PR;

        int16_t x_raw = TS_State.touchX[0];
        int16_t y_raw = TS_State.touchY[0];

        // Calibration
        int32_t x_cal = map(x_raw, 11, 799, 0, 800);
        int32_t y_cal = map(y_raw, 14, 432, 0, 480);

        // Clamp
        if (x_cal < 0) x_cal = 0;
        if (x_cal > 799) x_cal = 799;
        if (y_cal < 0) y_cal = 0;
        if (y_cal > 479) y_cal = 479;

        data->point.x = (lv_coord_t)x_cal;
        data->point.y = (lv_coord_t)y_cal;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}
