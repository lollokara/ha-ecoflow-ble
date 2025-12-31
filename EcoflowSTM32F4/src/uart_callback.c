#include "stm32f4xx_hal.h"
#include "uart_task.h"
#include "fan_task.h"

// Defined in main.c usually, but we need to override the weak symbol
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART6) {
        UART_RxCpltCallback(huart);
    } else if (huart->Instance == UART4) {
        Fan_UART_RxCpltCallback(huart);
    }
}
