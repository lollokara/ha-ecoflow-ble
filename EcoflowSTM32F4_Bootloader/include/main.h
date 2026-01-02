#ifndef __MAIN_H
#define __MAIN_H

#include "stm32f4xx_hal.h"

// Hardware Definitions
#define LED_GREEN_PIN GPIO_PIN_6
#define LED_GREEN_PORT GPIOG
#define LED_ORANGE_PIN GPIO_PIN_4
#define LED_ORANGE_PORT GPIOD
#define LED_RED_PIN GPIO_PIN_5
#define LED_RED_PORT GPIOD
#define LED_BLUE_PIN GPIO_PIN_3
#define LED_BLUE_PORT GPIOK

#define APP_ADDRESS 0x08008000
#define INACTIVE_BANK_ADDRESS 0x08108000

// Flag in Backup Register to trigger OTA
#define RTC_BKP_OTA_FLAG 0xDEADBEEF

void JumpToApplication(void);
void Error_Handler(void);

#endif
