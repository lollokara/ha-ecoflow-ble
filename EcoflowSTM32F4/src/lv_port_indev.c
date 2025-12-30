#include "lv_port_indev.h"
#include "stm32469i_discovery_ts.h"

static void touchpad_init(void);
static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data);

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
    // BSP_TS_Init should be called in main task
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
    BSP_TS_GetState(&TS_State);

    if(TS_State.touchDetected) {
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
