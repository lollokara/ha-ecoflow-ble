#ifndef BOOT_HANDLER_H
#define BOOT_HANDLER_H

#include "stm32f4xx_hal.h"

#define RTC_BKP_OTA_FLAG 0xDEADBEEF

void Boot_TriggerOTA(void);

#endif
