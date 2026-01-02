#ifndef BOOTLOADER_H
#define BOOTLOADER_H

#include "stm32f4xx_hal.h"

#define OTA_FLAG_VALUE      0xDEADBEEF

static inline void RebootToBootloader() {
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    RTC->BKP0R = OTA_FLAG_VALUE;
    HAL_NVIC_SystemReset();
}

#endif
