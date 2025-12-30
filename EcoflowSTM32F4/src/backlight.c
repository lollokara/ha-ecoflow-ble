#include "backlight.h"

static TIM_HandleTypeDef htim2;

void Backlight_Init(void) {
    TIM_ClockConfigTypeDef sClockSourceConfig = {0};
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    TIM_OC_InitTypeDef sConfigOC = {0};
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // Enable Clocks
    __HAL_RCC_TIM2_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    // Configure GPIO PA3 as Alternate Function (TIM2_CH4)
    GPIO_InitStruct.Pin = GPIO_PIN_3;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // Configure Timer
    // TIM2 clock is APB1 * 2 = 90MHz (Assuming 180MHz sysclk / 4 APB1)
    // We want ~500Hz.
    // 90,000,000 / 500 = 180,000.
    // Prescaler = 90 -> 1MHz counter. Period = 2000 -> 500Hz.
    htim2.Instance = TIM2;
    htim2.Init.Prescaler = 90 - 1;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 2000 - 1;
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    HAL_TIM_Base_Init(&htim2);

    sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
    HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig);

    HAL_TIM_PWM_Init(&htim2);

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig);

    // Configure Channel 4
    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 2000; // Start 100%
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_4);

    // Start PWM
    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);
}

void Backlight_SetBrightness(uint8_t percent) {
    if (percent > 100) percent = 100;
    if (percent < 10) percent = 10; // Minimum limit

    // Pulse width: 0 to 2000
    uint32_t pulse = (percent * 2000) / 100;
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, pulse);
}
