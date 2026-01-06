#ifndef LOG_MANAGER_H
#define LOG_MANAGER_H

#include <stdint.h>
#include <stdbool.h>

// Structure for a log file entry in the list
typedef struct {
    char name[32];
    uint32_t size;
} LogFileEntry;

// Maximum number of files to list
#define MAX_LOG_FILES 20

// Init
void LogManager_Init(void);

// Management
bool LogManager_FormatSD(void);
void LogManager_DeleteAllLogs(void);
bool LogManager_GetFreeSpace(uint32_t* total_mb, uint32_t* free_mb);
int LogManager_GetLogList(LogFileEntry* entries, int max_entries);

// Logging
void LogManager_SetEnabled(bool enabled);
bool LogManager_IsEnabled(void);
void LogManager_Write(const char* module, const char* function, const char* message);

// Streaming (for download)
bool LogManager_StartRead(const char* filename);
int LogManager_ReadChunk(uint8_t* buffer, int len);
void LogManager_CloseRead(void);
uint32_t LogManager_GetFileSize(const char* filename);

// Special Sections
void LogManager_WriteSection1(const char* f4_ver, const char* esp_ver);
void LogManager_WriteSection2(const char* config_dump);
void LogManager_WriteSection3Header(const char* device_name);
void LogManager_WriteSection3Value(const char* key, const char* value);
void LogManager_EndSection3(void);

#endif
