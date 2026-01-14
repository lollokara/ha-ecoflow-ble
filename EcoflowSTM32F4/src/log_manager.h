#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "ecoflow_protocol.h"

// Log Manager Initialization
void LogManager_Init(void);

// Main Process Loop (call frequently)
void LogManager_Process(void);

// Logging API
void LogManager_Write(uint8_t level, const char* tag, const char* message);

#include <stdio.h>
#include <string.h>

#define LOG_STM_INFO(tag, fmt, ...) do { \
    char _buf[256]; \
    snprintf(_buf, sizeof(_buf), "%s() " fmt, __FUNCTION__, ##__VA_ARGS__); \
    LogManager_Write(3, tag, _buf); \
} while(0)

#define LOG_STM_WARN(tag, fmt, ...) do { \
    char _buf[256]; \
    snprintf(_buf, sizeof(_buf), "%s() " fmt, __FUNCTION__, ##__VA_ARGS__); \
    LogManager_Write(2, tag, _buf); \
} while(0)

#define LOG_STM_ERROR(tag, fmt, ...) do { \
    char _buf[256]; \
    snprintf(_buf, sizeof(_buf), "%s() " fmt, __FUNCTION__, ##__VA_ARGS__); \
    LogManager_Write(1, tag, _buf); \
} while(0)
void LogManager_ForceRotate(void);

// UART Command Handlers
void LogManager_HandleListReq(void);
void LogManager_HandleDownloadReq(const char* filename);
void LogManager_HandleDeleteReq(const char* filename);
void LogManager_HandleManagerOp(uint8_t op_code);
void LogManager_HandleEspLog(uint8_t level, const char* tag, const char* message);

// Callbacks (from UART Task)
void LogManager_SendChunkAck(void); // If needed

// Status
uint32_t LogManager_GetTotalSpace(void);
uint32_t LogManager_GetFreeSpace(void);
void LogManager_GetStats(uint32_t* size, uint32_t* file_count);

#endif // LOG_MANAGER_H
