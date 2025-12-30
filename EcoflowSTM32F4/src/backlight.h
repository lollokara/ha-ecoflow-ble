#ifndef BACKLIGHT_H
#define BACKLIGHT_H

#include <stdint.h>
#include "stm32f4xx_hal.h"

// Backlight is on PA3 (TIM2_CH4)
void Backlight_Init(void);
void Backlight_SetBrightness(uint8_t percent);

#endif // BACKLIGHT_H
