#ifndef UART_TASK_H
#define UART_TASK_H

#include "FreeRTOS.h"
#include "task.h"
#include "ecoflow_protocol.h"

void StartUARTTask(void * argument);

// Helper to send Wave 2 Set Commands
void UART_SendWave2Set(Wave2SetMsg *msg);
void UART_SendACSet(uint8_t enable);
void UART_SendDCSet(uint8_t enable);
void UART_SendSetValue(uint8_t type, int value);
void UART_SendPowerOff(void);

#endif // UART_TASK_H
