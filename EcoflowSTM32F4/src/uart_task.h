#ifndef UART_TASK_H
#define UART_TASK_H

#include "FreeRTOS.h"
#include "task.h"
#include <stdint.h>

void StartUARTTask(void * argument);
void UART_SendPacket(uint8_t *buffer, uint8_t len);

#endif // UART_TASK_H
