#ifndef UART_TASK_H
#define UART_TASK_H

#include "FreeRTOS.h"
#include "task.h"
#include "ecoflow_protocol.h"
#include "stm32f4xx_hal.h"

void StartUARTTask(void * argument);

// Helper to send Wave 2 Set Commands
void UART_SendWave2Set(Wave2SetMsg *msg);
void UART_SendACSet(uint8_t enable);
void UART_SendDCSet(uint8_t enable);
void UART_SendSetValue(uint8_t type, int value);
void UART_SendPowerOff(void);
void UART_SendGetDebugInfo(void);
void UART_SendConnectDevice(uint8_t type);
void UART_SendForgetDevice(uint8_t type);
void UART_GetKnownDevices(DeviceList *list);

// Helper for IRQ dispatch
void UART_RxCpltCallback(UART_HandleTypeDef *huart);

// Direct Send (Thread Safe)
void UART_SendRaw(uint8_t* data, uint16_t len);

#endif // UART_TASK_H
