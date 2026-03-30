#ifndef FAN_PROTOCOL_H
#define FAN_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// UART Protocol Constants
#define FAN_UART_START_BYTE 0xBB
#define FAN_CMD_STATUS      0x01
#define FAN_CMD_SET_CONFIG  0x02
#define FAN_CMD_GET_CONFIG  0x03
#define FAN_CMD_CONFIG_RESP 0x04

// Data Structures

typedef struct {
    uint16_t min_speed;   // RPM
    uint16_t max_speed;   // RPM
    uint8_t start_temp;   // Celsius
    uint8_t max_temp;     // Celsius
} FanGroupConfig;

typedef struct {
    FanGroupConfig group1;
    FanGroupConfig group2;
} FanConfig;

typedef struct {
    float amb_temp;        // Celsius
    uint16_t fan_rpm[4];   // RPM for Fans 1-4
    bool connected;        // Connection status (Local only)
} FanStatus;

#ifdef __cplusplus
}
#endif

#endif // FAN_PROTOCOL_H
