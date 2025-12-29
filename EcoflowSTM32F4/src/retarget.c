#include <stdio.h>
#include "stm32f4xx_hal.h"

extern UART_HandleTypeDef huart3;

int _write(int file, char *ptr, int len) {
    HAL_UART_Transmit(&huart3, (uint8_t*)ptr, len, 100);
    return len;
}
