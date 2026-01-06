#include "log_manager.h"
#include "ff.h"
#include "FreeRTOS.h"
#include "semphr.h"
#include "task.h"
#include <stdio.h>
#include <string.h>

static FATFS fs;
static FIL logFile;
static FIL readFile;
static bool is_mounted = false;
static bool logging_enabled = false;
static SemaphoreHandle_t logMutex;

#define LOG_FILENAME "log.txt"
#define MAX_LOG_SIZE (5 * 1024 * 1024) // 5MB
#define F4_VER "1.0.0"
#define ESP_VER "Unknown"

static void WriteLogHeader(void) {
    LogManager_WriteSection1(F4_VER, ESP_VER);
}

void LogManager_Init(void) {
    logMutex = xSemaphoreCreateMutex();

    // Mount immediate? Or lazy?
    // Let's try mount.
    if (f_mount(&fs, "", 1) == FR_OK) {
        is_mounted = true;
    }
}

static void RotateLogIfNeeded(void) {
    if (f_size(&logFile) < MAX_LOG_SIZE) return;

    f_close(&logFile);

    // Find next available index
    char newName[32];
    int idx = 1;
    while(1) {
        snprintf(newName, sizeof(newName), "log_%03d.txt", idx);
        FILINFO fno;
        if (f_stat(newName, &fno) != FR_OK) break; // File doesn't exist
        idx++;
    }

    f_rename(LOG_FILENAME, newName);

    // Re-open new file
    f_open(&logFile, LOG_FILENAME, FA_WRITE | FA_OPEN_APPEND | FA_CREATE_ALWAYS);
    WriteLogHeader();
}

void LogManager_SetEnabled(bool enabled) {
    xSemaphoreTake(logMutex, portMAX_DELAY);
    if (enabled && !logging_enabled) {
        if (!is_mounted) {
            if (f_mount(&fs, "", 1) == FR_OK) is_mounted = true;
        }

        if (is_mounted) {
            if (f_open(&logFile, LOG_FILENAME, FA_WRITE | FA_OPEN_APPEND | FA_CREATE_ALWAYS) == FR_OK) {
                logging_enabled = true;
                if (f_size(&logFile) == 0) WriteLogHeader();
                RotateLogIfNeeded();
            }
        }
    } else if (!enabled && logging_enabled) {
        f_close(&logFile);
        logging_enabled = false;
    }
    xSemaphoreGive(logMutex);
}

bool LogManager_IsEnabled(void) {
    return logging_enabled;
}

void LogManager_Write(const char* module, const char* function, const char* message) {
    if (!logging_enabled) return;

    xSemaphoreTake(logMutex, portMAX_DELAY);

    char header[128];
    int len = snprintf(header, sizeof(header), "[%s] %s() ", module, function);

    UINT bw;
    f_write(&logFile, header, len, &bw);
    f_write(&logFile, message, strlen(message), &bw);
    f_write(&logFile, "\n", 1, &bw);

    f_sync(&logFile); // Ensure data is written

    RotateLogIfNeeded();

    xSemaphoreGive(logMutex);
}

bool LogManager_FormatSD(void) {
    xSemaphoreTake(logMutex, portMAX_DELAY);
    bool was_enabled = logging_enabled;
    if (was_enabled) {
        f_close(&logFile);
        logging_enabled = false;
    }

    uint8_t work[512]; // Working buffer for mkfs
    FRESULT res = f_mkfs("", 0, work, sizeof(work));

    if (res == FR_OK && was_enabled) {
        // Re-enable
         f_open(&logFile, LOG_FILENAME, FA_WRITE | FA_OPEN_APPEND | FA_CREATE_ALWAYS);
         logging_enabled = true;
    }

    xSemaphoreGive(logMutex);
    return res == FR_OK;
}

void LogManager_DeleteAllLogs(void) {
    xSemaphoreTake(logMutex, portMAX_DELAY);

    DIR dir;
    FILINFO fno;

    if (f_opendir(&dir, "") == FR_OK) {
        while (1) {
            if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0) break;
            if (strstr(fno.fname, ".txt")) { // Delete all .txt files
                // If it's the current log and open, close it first
                if (logging_enabled && strcmp(fno.fname, LOG_FILENAME) == 0) {
                     f_close(&logFile);
                     logging_enabled = false;
                }
                f_unlink(fno.fname);
            }
        }
        f_closedir(&dir);
    }

    // Restart log if it was enabled
    if (!logging_enabled) {
        // User might want to keep it disabled or restart.
        // Function says "Delete all logs". Usually implies resetting state.
    }

    xSemaphoreGive(logMutex);
}

bool LogManager_GetFreeSpace(uint32_t* total_mb, uint32_t* free_mb) {
    FATFS *pfs;
    DWORD fre_clust, fre_sect, tot_sect;

    if (f_getfree("", &fre_clust, &pfs) != FR_OK) return false;

    tot_sect = (pfs->n_fatent - 2) * pfs->csize;
    fre_sect = fre_clust * pfs->csize;

    *total_mb = tot_sect / 2048; // Assuming 512 bytes/sector
    *free_mb = fre_sect / 2048;

    return true;
}

int LogManager_GetLogList(LogFileEntry* entries, int max_entries) {
    DIR dir;
    FILINFO fno;
    int count = 0;

    if (f_opendir(&dir, "") == FR_OK) {
        while (count < max_entries) {
            if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0) break;
            if (strstr(fno.fname, ".txt")) {
                strncpy(entries[count].name, fno.fname, sizeof(entries[count].name));
                entries[count].size = fno.fsize;
                count++;
            }
        }
        f_closedir(&dir);
    }
    return count;
}

// Read API for streaming
bool LogManager_StartRead(const char* filename) {
    xSemaphoreTake(logMutex, portMAX_DELAY);
    if (f_open(&readFile, filename, FA_READ) == FR_OK) {
        return true;
    }
    xSemaphoreGive(logMutex);
    return false;
}

int LogManager_ReadChunk(uint8_t* buffer, int len) {
    UINT br;
    if (f_read(&readFile, buffer, len, &br) != FR_OK) return -1;
    return (int)br;
}

void LogManager_CloseRead(void) {
    f_close(&readFile);
    xSemaphoreGive(logMutex);
}

uint32_t LogManager_GetFileSize(const char* filename) {
    FILINFO fno;
    if (f_stat(filename, &fno) == FR_OK) {
        return fno.fsize;
    }
    return 0;
}

void LogManager_WriteSection1(const char* f4_ver, const char* esp_ver) {
    if (!logging_enabled) return;
    xSemaphoreTake(logMutex, portMAX_DELAY);

    f_printf(&logFile, "--- Firmware Version ---\n");
    f_printf(&logFile, "F4: %s\n", f4_ver);
    f_printf(&logFile, "ESP32: %s\n\n", esp_ver);
    f_sync(&logFile);

    xSemaphoreGive(logMutex);
}

void LogManager_WriteSection2(const char* config_dump) {
    if (!logging_enabled) return;
    xSemaphoreTake(logMutex, portMAX_DELAY);

    f_printf(&logFile, "--- Config Dump ---\n");
    f_printf(&logFile, "%s\n\n", config_dump);
    f_sync(&logFile);

    xSemaphoreGive(logMutex);
}

void LogManager_WriteSection3Header(const char* device_name) {
     if (!logging_enabled) return;
    xSemaphoreTake(logMutex, portMAX_DELAY);
    f_printf(&logFile, "--- Full %s Dump ---\n", device_name);
    xSemaphoreGive(logMutex);
}

void LogManager_WriteSection3Value(const char* key, const char* value) {
     if (!logging_enabled) return;
    xSemaphoreTake(logMutex, portMAX_DELAY);
    f_printf(&logFile, "%s: %s, ", key, value);
    xSemaphoreGive(logMutex);
}

void LogManager_EndSection3(void) {
     if (!logging_enabled) return;
    xSemaphoreTake(logMutex, portMAX_DELAY);
    f_printf(&logFile, "\n");
    f_sync(&logFile);
    xSemaphoreGive(logMutex);
}
