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
// Use LOG_INFO macro instead of calling this directly for automatic file/func info
void LogManager_Write(const char* file, const char* func, const char* message, ...);

#define LOG_INFO(msg, ...) LogManager_Write(__FILE__, __FUNCTION__, msg, ##__VA_ARGS__)

void LogManager_ForceRotate(void);

// UART Command Handlers
void LogManager_HandleListReq(void);
void LogManager_HandleDownloadReq(const char* filename);
void LogManager_HandleDeleteReq(const char* filename);
void LogManager_HandleManagerOp(uint8_t op_code);
void LogManager_HandleEspLog(const char* message); // ESP logs come as formatted strings

// Status
uint32_t LogManager_GetTotalSpace(void);
uint32_t LogManager_GetFreeSpace(void);
uint32_t LogManager_GetFileCount(void);

#endif // LOG_MANAGER_H
