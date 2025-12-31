#ifndef RP2040_TASK_H
#define RP2040_TASK_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
    uint16_t rpm[4];
    float temp;
    bool connected;
    uint32_t last_update;
} RP2040_Status;

typedef struct {
    uint16_t min_speed;
    uint16_t max_speed;
    uint8_t start_temp;
    uint8_t max_temp;
} FanGroupConfig;

typedef void (*ConfigCallback)(uint8_t group, FanGroupConfig* config);

void StartRP2040Task(void *argument);
void RP2040_SendConfig(uint8_t group, FanGroupConfig* config);
void RP2040_RequestConfig(uint8_t group);
void RP2040_SetConfigCallback(ConfigCallback cb);
RP2040_Status* RP2040_GetStatus(void);
bool RP2040_IsConnected(void);

#endif
