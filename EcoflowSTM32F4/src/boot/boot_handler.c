#include "boot/boot_handler.h"

void Boot_TriggerOTA(void) {
    // 1. Enable Power Clock
    __HAL_RCC_PWR_CLK_ENABLE();

    // 2. Allow access to Backup Domain
    HAL_PWR_EnableBkUpAccess();

    // 3. Write Magic Flag to Backup Register 0
    // Access directly to avoid RTC handle dependency if not initialized
    RTC->BKP0R = RTC_BKP_OTA_FLAG;

    // 4. Reset System
    HAL_NVIC_SystemReset();
}
