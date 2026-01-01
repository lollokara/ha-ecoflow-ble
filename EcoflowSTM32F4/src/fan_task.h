#ifndef FAN_TASK_H
#define FAN_TASK_H

#include "FreeRTOS.h"
#include "task.h"
#include "fan_protocol.h"
#include "stm32f4xx_hal.h"

// Initialize Task
void StartFanTask(void *argument);

// Interface for UI
void Fan_GetStatus(FanStatus *status);
void Fan_SetConfig(const FanConfig *config);
void Fan_GetConfig(FanConfig *config); // Returns last known config sent or default
void Fan_RequestConfig(void);

// Helper for IRQ dispatch
void Fan_UART_RxCpltCallback(UART_HandleTypeDef *huart);

#endif // FAN_TASK_H
