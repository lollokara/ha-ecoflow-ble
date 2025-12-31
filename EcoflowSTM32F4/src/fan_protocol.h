#ifndef FAN_PROTOCOL_H
#define FAN_PROTOCOL_H

#include <stdint.h>
#include <stdbool.h>

typedef struct {
  uint16_t min_speed;
  uint16_t max_speed;
  uint8_t start_temp;
  uint8_t max_temp;
} FanGroupConfig;

typedef struct {
    FanGroupConfig group1;
    FanGroupConfig group2;
} FanConfig;

typedef struct {
    float amb_temp;
    uint16_t fan_rpm[4];
    bool connected;
} FanStatus;

#define FAN_CMD_START 0xBB

#define CMD_FAN_STATUS 0x01
#define CMD_FAN_SET_CONFIG 0x02
#define CMD_FAN_GET_CONFIG 0x03
#define CMD_FAN_CONFIG_RESP 0x04

#endif
