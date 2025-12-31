#ifndef FAN_TASK_H
#define FAN_TASK_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "fan_protocol.h"

// Initialize Task
void StartFanTask(void *argument);

// Interface
FanStatus* Fan_GetStatus(void);
FanConfig* Fan_GetConfig(void);
void Fan_SetConfig(FanConfig* config);

// UART Handlers (called from IRQ/HAL)
void Fan_RxCallback(uint8_t byte);

#endif
