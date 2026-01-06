#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

void LogManager_Init(void);
void StartLogTask(void * argument);

void LogManager_Log(const char* fmt, ...);

// Command Handlers
void LogManager_HandleList(void);
void LogManager_HandleDownload(const char* filename, uint32_t offset);
void LogManager_HandleDelete(const char* filename);
void LogManager_HandleDeleteAll(void);
void LogManager_HandleFormat(void);

void LogManager_HandlePushData(uint8_t type, const char* msg);

// Control
void LogManager_SetLogging(bool enabled);
bool LogManager_IsLogging(void);
void LogManager_GetSpace(uint32_t *total, uint32_t *free);

#endif
