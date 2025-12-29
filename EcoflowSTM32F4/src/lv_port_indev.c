#include "lv_port_indev.h"
#include "stm32469i_discovery_ts.h"

static lv_indev_drv_t indev_drv;

static void touchpad_read(lv_indev_drv_t * indev_drv, lv_indev_data_t * data)
{
    TS_StateTypeDef TS_State;
    BSP_TS_GetState(&TS_State);

    if(TS_State.touchDetected) {
        data->state = LV_INDEV_STATE_PR;
        data->point.x = TS_State.touchX[0];
        data->point.y = TS_State.touchY[0];
    } else {
        data->state = LV_INDEV_STATE_REL;
    }
}

void lv_port_indev_init(void)
{
    lv_indev_drv_init(&indev_drv);
    indev_drv.type = LV_INDEV_TYPE_POINTER;
    indev_drv.read_cb = touchpad_read;
    lv_indev_drv_register(&indev_drv);
}
