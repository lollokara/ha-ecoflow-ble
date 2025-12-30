#ifndef DISPLAY_TASK_H
#define DISPLAY_TASK_H

#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <stdint.h>
#include "ecoflow_protocol.h"

// Events that can be sent to the display task
typedef enum {
    DISPLAY_EVENT_UPDATE_BATTERY,
    DISPLAY_EVENT_UPDATE_TEMP,
    DISPLAY_EVENT_UPDATE_CONNECTION
} DisplayEventType;

typedef struct {
    DisplayEventType type;
    union {
        DeviceStatus deviceStatus; // Includes id and BatteryStatus
        // Add other event data structs here
    } data;
} DisplayEvent;

extern QueueHandle_t displayQueue;

void StartDisplayTask(void * argument);

#endif // DISPLAY_TASK_H
