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

#endif // LOG_MANAGER_H
