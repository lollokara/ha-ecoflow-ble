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

static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    static TS_StateTypeDef  TS_State;
    BSP_TS_GetState(&TS_State);

    if(TS_State.touchDetected) {
        data->state = LV_INDEV_STATE_PR;

        // Clamping logic for overshoot (0-800, 0-480)
        // If touch driver returns coordinates slightly outside this range, we clamp them.
        int16_t x = TS_State.touchX[0];
        int16_t y = TS_State.touchY[0];

        // Simple manual calibration/scaling if needed.
        // Assuming 1:1 mapping but with potential noise or slight offset.
        // User report: "touch is mapped to a bigger area than the screen".
        // This suggests that e.g. touching center (400) reads as >400? Or touching right edge (800) reads >800?
        // If 800 reads as >800, clamping fixes it.
        // If 400 reads as 600, we need scaling.
        // I will assume simple clamping first as it's safer without hard calibration data.

        if (x < 0) x = 0;
        if (x > 799) x = 799;
        if (y < 0) y = 0;
        if (y > 479) y = 479;

        data->point.x = x;
        data->point.y = y;
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}
