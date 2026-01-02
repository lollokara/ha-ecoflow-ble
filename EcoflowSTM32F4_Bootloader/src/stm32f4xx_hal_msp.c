#include "main.h"

// Defined in main.c
extern UART_HandleTypeDef huart3; // Debug
extern UART_HandleTypeDef huart6; // ESP32

void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if(uartHandle->Instance==USART3) {
        // Debug UART: PB10 (TX), PB11 (RX)
        __HAL_RCC_USART3_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();

        GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

        // Use Polling for Debug, so no NVIC needed usually.
        // But if CLI uses Interrupt, enable it.
        // For simple CLI, Polling is easiest, but blocking.
        // Let's enable RX IT for CLI responsiveness.
        HAL_NVIC_SetPriority(USART3_IRQn, 6, 0);
        HAL_NVIC_EnableIRQ(USART3_IRQn);
    }
    else if(uartHandle->Instance==USART6) {
        // ESP32 UART: PG9 (RX), PG14 (TX)
        __HAL_RCC_USART6_CLK_ENABLE();
        __HAL_RCC_GPIOG_CLK_ENABLE();

        GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_14;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF8_USART6;
        HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
    }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef* uartHandle) {
    if(uartHandle->Instance==USART3) {
        __HAL_RCC_USART3_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_10|GPIO_PIN_11);
        HAL_NVIC_DisableIRQ(USART3_IRQn);
    }
    else if(uartHandle->Instance==USART6) {
        __HAL_RCC_USART6_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOG, GPIO_PIN_9|GPIO_PIN_14);
    }
}
