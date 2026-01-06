#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include <stdint.h>
#include <stdbool.h>
#include "ff.h"

// Initialize the Log Manager (Mount FS)
bool LogManager_Init(void);

// Start the Logging Task
void LogManager_StartTask(void);

// Queue a log message for writing
void LogManager_Write(const char* func, const char* msg);

// Request a list of log files (Triggers CMD_LOG_LIST_RESP via UART)
void LogManager_GetList(void);

// Request file transfer start (Triggers CMD_LOG_FILE_DATA stream)
void LogManager_StartTransfer(const char* filename);

// Delete all log files
void LogManager_DeleteAll(void);

// Format the SD Card
void LogManager_Format(void);

// Get current space usage (MB Used, MB Total)
void LogManager_GetSpace(uint32_t *used_mb, uint32_t *total_mb);

#endif
