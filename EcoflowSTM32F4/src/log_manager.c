#include "log_manager.h"
#include "ff.h"
#include "uart_task.h"
#include "FreeRTOS.h"
#include "task.h"
#include "semphr.h"
#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define MAX_LOG_SIZE (5 * 1024 * 1024)
#define LOG_FILENAME "current.log"

extern char SDPath[4];
extern FATFS SDFatFs;

static FIL LogFile;
static bool LogOpen = false;
static SemaphoreHandle_t LogMutex = NULL;

// Download State
static bool Downloading = false;
static FIL DownloadFile;
static char DownloadName[32];
static uint32_t DownloadOffset = 0;
static uint32_t DownloadSize = 0;

void LogManager_Init(void) {
    if (LogMutex == NULL) LogMutex = xSemaphoreCreateMutex();

    // Mount Filesystem
    FRESULT res = f_mount(&SDFatFs, SDPath, 1);
    printf("LogManager: f_mount res=%d path=%s\n", res, SDPath);

    if (res == FR_NO_FILESYSTEM) {
        printf("No Filesystem. Formatting...\n");
        BYTE work[FF_MAX_SS];
        MKFS_PARM opt = {FM_FAT32, 0, 0, 0, 0};
        FRESULT fmt_res = f_mkfs(SDPath, &opt, work, sizeof(work));
        if (fmt_res == FR_OK) {
            printf("Format Success. Remounting...\n");
            FRESULT remount_res = f_mount(&SDFatFs, SDPath, 1);
            if (remount_res != FR_OK) {
                printf("Remount Failed: %d\n", remount_res);
                return;
            }
        } else {
            printf("Format Failed: %d\n", fmt_res);
            return;
        }
    } else if (res != FR_OK) {
        printf("FatFs Mount Failed: %d\n", res);
        return;
    }

    // Open current log
    xSemaphoreTake(LogMutex, portMAX_DELAY);
    res = f_open(&LogFile, LOG_FILENAME, FA_OPEN_ALWAYS | FA_WRITE | FA_READ);
    printf("LogManager: f_open res=%d\n", res);

    if (res == FR_OK) {
        f_lseek(&LogFile, f_size(&LogFile)); // Append
        LogOpen = true;
        xSemaphoreGive(LogMutex);

        // Check size (LogManager_Write handles this, but good to check early)
        if (f_size(&LogFile) > MAX_LOG_SIZE) {
            LogManager_ForceRotate();
        }

        LogManager_Write(3, "SYS", "Log System Initialized");
    } else {
        xSemaphoreGive(LogMutex);
    }
}

void LogManager_ForceRotate(void) {
    xSemaphoreTake(LogMutex, portMAX_DELAY);
    if (LogOpen) {
        f_close(&LogFile);
        LogOpen = false;
    }

    // Rename current.log to log_N.txt
    // Find next available N
    char new_name[32];
    int i = 0;
    while(1) {
        sprintf(new_name, "log_%d.txt", i);
        FILINFO fno;
        if (f_stat(new_name, &fno) != FR_OK) break; // Found free slot
        i++;
        if(i > 999) break; // Safety
    }

    f_rename(LOG_FILENAME, new_name);

    // Open new
    if (f_open(&LogFile, LOG_FILENAME, FA_CREATE_ALWAYS | FA_WRITE | FA_READ) == FR_OK) {
        LogOpen = true;
        xSemaphoreGive(LogMutex);

        // Section 1: Firmware Versions
        LogManager_Write(0, "SYS", "--- Firmware Versions ---");
        LogManager_Write(0, "SYS", "STM32 F4: v1.0.0");
        LogManager_Write(0, "SYS", "ESP32: v1.0.0");

        // Section 2: Devices Connected, Config Dump
        uint8_t buf[32];
        int len;

        len = pack_simple_cmd_message(buf, CMD_GET_FULL_CONFIG);
        UART_SendRaw(buf, len);

        // Section 3: Debug Values (Full Dump)
        len = pack_simple_cmd_message(buf, CMD_GET_DEBUG_DUMP);
        UART_SendRaw(buf, len);
    } else {
        xSemaphoreGive(LogMutex);
    }
}

void LogManager_Write(uint8_t level, const char* tag, const char* message) {
    // Check size
    // Note: Recursive calls to ForceRotate must be careful with mutex.
    // ForceRotate takes mutex. LogManager_Write shouldn't hold it when calling ForceRotate.
    // But LogManager_Write needs mutex for f_write.
    // Solution: Check size outside mutex or use recursive mutex.
    // FreeRTOS mutexes are recursive? xSemaphoreCreateMutex is NOT recursive.
    // xSemaphoreCreateRecursiveMutex is.
    // For simplicity, I'll release mutex before ForceRotate.

    xSemaphoreTake(LogMutex, portMAX_DELAY);
    if (!LogOpen) {
        xSemaphoreGive(LogMutex);
        // printf("LogManager_Write: Dropped (Not Open) [%s] %s\n", tag, message);
        return;
    }
    uint32_t size = f_size(&LogFile);
    xSemaphoreGive(LogMutex);

    if (size > MAX_LOG_SIZE) {
        LogManager_ForceRotate();
    }

    xSemaphoreTake(LogMutex, portMAX_DELAY);
    if (!LogOpen) { // Re-check
        xSemaphoreGive(LogMutex);
        return;
    }

    // Format: [Timestamp] [Tag] Message\n
    char line[512];
    uint32_t time;
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        time = xTaskGetTickCount();
    } else {
        time = HAL_GetTick();
    }

    int line_len;
    if (tag && strlen(tag) > 0) {
        line_len = snprintf(line, sizeof(line), "[%lu] [%s] %s\n", time, tag, message);
    } else {
        line_len = snprintf(line, sizeof(line), "[%lu] %s\n", time, message);
    }

    FRESULT res;
    if (line_len > 0) {
        UINT bw;
        res = f_write(&LogFile, line, (UINT)line_len, &bw);
        if (res != FR_OK || bw != (UINT)line_len) {
            // printf("LogManager_Write: f_write fail res=%d bw=%d\n", res, bw);
        }
    }

    res = f_sync(&LogFile);
    xSemaphoreGive(LogMutex);

    if (res != FR_OK) {
        // printf("LogManager_Write: f_sync fail res=%d\n", res);
    }
}

void LogManager_Process(void) {
    if (Downloading) {
        // Send chunks
        uint8_t buffer[200];
        UINT br;

        // Note: Download uses separate file handle 'DownloadFile'
        // But f_read might conflict with f_write on other handle?
        // FatFs usually handles multiple open files fine if re-entrant.
        // I will protect with mutex to be safe.
        xSemaphoreTake(LogMutex, portMAX_DELAY);
        f_lseek(&DownloadFile, DownloadOffset);
        FRESULT res = f_read(&DownloadFile, buffer, sizeof(buffer), &br);
        xSemaphoreGive(LogMutex);

        if (res == FR_OK) {
            if (br > 0) {
                uint8_t packet[256];
                int len = pack_log_data_chunk_message(packet, DownloadOffset, buffer, br);
                UART_SendRaw(packet, len);
            }

            DownloadOffset += br;
            if (br < sizeof(buffer) || DownloadOffset >= DownloadSize) {
                // End of file
                Downloading = false;
                xSemaphoreTake(LogMutex, portMAX_DELAY);
                f_close(&DownloadFile);
                xSemaphoreGive(LogMutex);
                // Send empty chunk
                uint8_t packet[32];
                int len = pack_log_data_chunk_message(packet, DownloadOffset, NULL, 0);
                UART_SendRaw(packet, len);
            }
        } else {
             // Error
             Downloading = false;
             xSemaphoreTake(LogMutex, portMAX_DELAY);
             f_close(&DownloadFile);
             xSemaphoreGive(LogMutex);
        }
    }
}

// Fixed Buffer for File Listing to ensure consistency
typedef struct {
    char name[32];
    uint32_t size;
} FileEntry;

void LogManager_HandleListReq(void) {
    DIR dir;
    FILINFO fno;
    uint8_t buffer[256];

    // Buffer for files (max 100 files, ~3.6KB on stack)
    // STM32 Stack is 8KB. This is safe.
    FileEntry entries[64];
    uint8_t count = 0;

    xSemaphoreTake(LogMutex, portMAX_DELAY);
    if (f_opendir(&dir, "/") == FR_OK) {
        while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
             if (strstr(fno.fname, ".log") || strstr(fno.fname, ".txt")) {
                 if (count < 64) {
                     strncpy(entries[count].name, fno.fname, 31);
                     entries[count].name[31] = 0;
                     entries[count].size = fno.fsize;
                     count++;
                 }
             }
        }
        f_closedir(&dir);
    }
    xSemaphoreGive(LogMutex);

    if (count == 0) {
         int len = pack_log_list_resp_message(buffer, 0, 0, 0, "");
         UART_SendRaw(buffer, len);
    } else {
        for (uint8_t i = 0; i < count; i++) {
            int len = pack_log_list_resp_message(buffer, count, i, entries[i].size, entries[i].name);
            UART_SendRaw(buffer, len);
            vTaskDelay(30); // 30ms throttle
        }
    }
}

void LogManager_HandleDownloadReq(const char* filename) {
    if (Downloading) {
        xSemaphoreTake(LogMutex, portMAX_DELAY);
        f_close(&DownloadFile);
        xSemaphoreGive(LogMutex);
    }

    strncpy(DownloadName, filename, 31);
    xSemaphoreTake(LogMutex, portMAX_DELAY);
    if (f_open(&DownloadFile, filename, FA_READ) == FR_OK) {
        Downloading = true;
        DownloadOffset = 0;
        DownloadSize = f_size(&DownloadFile);
    } else {
        Downloading = false;
    }
    xSemaphoreGive(LogMutex);
}

void LogManager_HandleDeleteReq(const char* filename) {
    xSemaphoreTake(LogMutex, portMAX_DELAY);
    f_unlink(filename);
    xSemaphoreGive(LogMutex);
}

void LogManager_HandleManagerOp(uint8_t op_code) {
    if (op_code == LOG_OP_DELETE_ALL) {
        DIR dir;
        FILINFO fno;
        xSemaphoreTake(LogMutex, portMAX_DELAY);
        if (f_opendir(&dir, "/") == FR_OK) {
            while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
                if (strstr(fno.fname, ".log") || strstr(fno.fname, ".txt")) {
                    f_unlink(fno.fname);
                }
            }
            f_closedir(&dir);
        }
        xSemaphoreGive(LogMutex);
    } else if (op_code == LOG_OP_FORMAT_SD) {
        BYTE work[FF_MAX_SS];
        MKFS_PARM opt = {FM_FAT32, 0, 0, 0, 0};
        xSemaphoreTake(LogMutex, portMAX_DELAY);
        f_mkfs(SDPath, &opt, work, sizeof(work));
        f_mount(&SDFatFs, SDPath, 1);
        xSemaphoreGive(LogMutex);
        LogManager_Init();
    }
}

void LogManager_HandleEspLog(uint8_t level, const char* tag, const char* message) {
    LogManager_Write(level, tag, message);
}

uint32_t LogManager_GetTotalSpace(void) {
    FATFS *fs;
    DWORD fre_clust, tot_sect;
    // f_getfree might need mutex
    xSemaphoreTake(LogMutex, portMAX_DELAY);
    if (f_getfree(SDPath, &fre_clust, &fs) == FR_OK) {
        tot_sect = (fs->n_fatent - 2) * fs->csize;
        xSemaphoreGive(LogMutex);
        return tot_sect / 2;
    }
    xSemaphoreGive(LogMutex);
    return 0;
}

void LogManager_GetStats(uint32_t* size, uint32_t* file_count) {
    xSemaphoreTake(LogMutex, portMAX_DELAY);
    if (size) *size = LogOpen ? f_size(&LogFile) : 0;

    if (file_count) {
        *file_count = 0;
        DIR dir;
        FILINFO fno;
        if (f_opendir(&dir, "/") == FR_OK) {
            while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
                 if ((strstr(fno.fname, ".log") || strstr(fno.fname, ".txt")) &&
                     strcmp(fno.fname, LOG_FILENAME) != 0) {
                     (*file_count)++;
                 }
            }
            f_closedir(&dir);
        }
    }
    xSemaphoreGive(LogMutex);
}

uint32_t LogManager_GetFreeSpace(void) {
    FATFS *fs;
    DWORD fre_clust, fre_sect;
    xSemaphoreTake(LogMutex, portMAX_DELAY);
    if (f_getfree(SDPath, &fre_clust, &fs) == FR_OK) {
        fre_sect = fre_clust * fs->csize;
        xSemaphoreGive(LogMutex);
        return fre_sect / 2;
    }
    xSemaphoreGive(LogMutex);
    return 0;
}
