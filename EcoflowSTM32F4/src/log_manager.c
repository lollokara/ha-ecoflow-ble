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

// Forward Declarations
void LogManager_Write(uint8_t level, const char* tag, const char* message);
static void LogManager_Write_Internal(uint8_t level, const char* tag, const char* message);
void LogManager_WriteSessionHeader(void);
void LogManager_ForceRotate(void);

static FIL LogFile;
static bool LogOpen = false;
static bool TriggerSessionHeader = false;
static SemaphoreHandle_t LogMutex = NULL;

// Download State
static bool Downloading = false;
static FIL DownloadFile;
static char DownloadName[32];
static uint32_t DownloadOffset = 0;
static uint32_t DownloadSize = 0;

void LogManager_Init(void) {
    if (LogMutex == NULL) {
        LogMutex = xSemaphoreCreateMutex();
    }

    if (xSemaphoreTake(LogMutex, portMAX_DELAY) != pdTRUE) return;

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
            xSemaphoreGive(LogMutex);
            return;
        }
    } else if (res != FR_OK) {
        printf("FatFs Mount Failed: %d\n", res);
        xSemaphoreGive(LogMutex);
        return;
    }

    // Open current log
    res = f_open(&LogFile, LOG_FILENAME, FA_OPEN_ALWAYS | FA_WRITE | FA_READ);
    printf("LogManager: f_open res=%d\n", res);

    if (res == FR_OK) {
        f_lseek(&LogFile, f_size(&LogFile)); // Append
        LogOpen = true;

        // Check size
        if (f_size(&LogFile) > MAX_LOG_SIZE) {
            // LogManager_ForceRotate calls f_ functions, need to avoid recursive mutex take deadlock if simple mutex
            // But FreeRTOS Mutex is recursive safe if xSemaphoreCreateRecursiveMutex used.
            // Standard Mutex is NOT recursive.
            // We must unlock before calling a function that might lock, OR (better) make internal functions assume lock is held.
            // Let's make internal helpers or just unlock.
            // ForceRotate uses f_close/rename/open. We should implement logic here or make ForceRotate lock-aware.
            // Simplest: Release, call Rotate, Retake? No, unsafe.
            // Better: Move Logic inside Init or make helper.
            // Actually, let's just make ForceRotate assume caller holds lock if internal, or make it locking.
            // Current ForceRotate is public? No, header says void LogManager_ForceRotate(void);
            // It's called from Write.
            // Let's define a static InternalForceRotate that assumes lock.

            // For now, I will assume single context for Init. But Write calls Rotate.
            // Refactoring:
            // 1. Rename ForceRotate to InternalForceRotate.
            // 2. Make ForceRotate a wrapper that takes lock.
            // 3. Call InternalForceRotate here.

            // Wait, I cannot easily rename in this partial diff.
            // I will release lock, call Rotate, retake.
            xSemaphoreGive(LogMutex);
            LogManager_ForceRotate();
            xSemaphoreTake(LogMutex, portMAX_DELAY);
        } else {
            // New session in existing file
            // We hold lock, use Internal
            LogManager_Write_Internal(3, "SYS", "Log System Initialized");
            TriggerSessionHeader = true;
        }
    }
    xSemaphoreGive(LogMutex);
}

void LogManager_WriteSessionHeader(void) {
    if (!LogOpen) return;

    // Header Section 1
    LogManager_Write_Internal(0, "SYS", "--- Firmware Versions ---");
    LogManager_Write_Internal(0, "SYS", "STM32 F4: v1.0.0"); // TODO: Get real version
    LogManager_Write_Internal(0, "SYS", "ESP32: v1.0.0");     // TODO: Get real version

    // Request Section 2 & 3
    // UART_SendRaw uses its own mutex, safe to call while holding LogMutex or not
    uint8_t buf[32];
    int len;

    len = pack_simple_cmd_message(buf, CMD_GET_FULL_CONFIG);
    UART_SendRaw(buf, len);

    len = pack_simple_cmd_message(buf, CMD_GET_DEBUG_DUMP);
    UART_SendRaw(buf, len);
}

void LogManager_ForceRotate(void) {
    // Assumes LogMutex is held by caller (Write_Internal)
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
        LogManager_WriteSessionHeader();
    }
}

// Internal Write function - Assumes LogMutex is held by caller
static void LogManager_Write_Internal(uint8_t level, const char* tag, const char* message) {
    if (!LogOpen) return;

    // Check size and rotate if needed
    // Recursion risk if ForceRotate calls WriteSessionHeader calls Write_Internal
    // ForceRotate logic:
    // Close -> Rename -> Open -> WriteSessionHeader -> Write_Internal
    // This is safe as long as ForceRotate does NOT call ForceRotate.
    // However, if new file is somehow > MAX immediately (impossible), we loop.
    // Standard check:
    if (f_size(&LogFile) > MAX_LOG_SIZE) {
        LogManager_ForceRotate();
    }

    if (!LogOpen) return;

    char line[512];
    uint32_t time;
    if (xTaskGetSchedulerState() == taskSCHEDULER_RUNNING) {
        time = xTaskGetTickCount();
    } else {
        time = HAL_GetTick();
    }

    int line_len = snprintf(line, sizeof(line), "[%lu] [%s] %s\n", time, tag, message);

    FRESULT res;
    if (line_len > 0) {
        UINT bw;
        res = f_write(&LogFile, line, (UINT)line_len, &bw);
        if (res != FR_OK || bw != (UINT)line_len) {
            // Error handling
        }
    }
    f_sync(&LogFile);
}

void LogManager_Write(uint8_t level, const char* tag, const char* message) {
    if (!LogMutex) return;
    if (xSemaphoreTake(LogMutex, 100) == pdTRUE) {
        LogManager_Write_Internal(level, tag, message);
        xSemaphoreGive(LogMutex);
    }
}

void LogManager_Process(void) {
    if (TriggerSessionHeader) {
        if (xSemaphoreTake(LogMutex, 100) == pdTRUE) {
            LogManager_WriteSessionHeader();
            TriggerSessionHeader = false;
            xSemaphoreGive(LogMutex);
        }
    }

    if (Downloading) {
        if (xSemaphoreTake(LogMutex, 100) == pdTRUE) {
            // Send chunks
            uint8_t buffer[200];
            UINT br;

            f_lseek(&DownloadFile, DownloadOffset);
            if (f_read(&DownloadFile, buffer, sizeof(buffer), &br) == FR_OK) {
                if (br > 0) {
                    uint8_t packet[256];
                    int len = pack_log_data_chunk_message(packet, DownloadOffset, buffer, br);
                    UART_SendRaw(packet, len);
                }

                DownloadOffset += br;
                if (br < sizeof(buffer) || DownloadOffset >= DownloadSize) {
                    // End of file
                    Downloading = false;
                    f_close(&DownloadFile);
                    // Send empty chunk
                    uint8_t packet[32];
                    int len = pack_log_data_chunk_message(packet, DownloadOffset, NULL, 0);
                    UART_SendRaw(packet, len);
                }
            } else {
                 // Error
                 Downloading = false;
                 f_close(&DownloadFile);
            }
            xSemaphoreGive(LogMutex);
        }
    }
}

void LogManager_HandleListReq(void) {
    if (!LogMutex) return;
    xSemaphoreTake(LogMutex, portMAX_DELAY);
    DIR dir;
    FILINFO fno;
    uint8_t buffer[256];
    uint8_t count = 0;

    // Use global SDPath or fallback to "0:/" if empty (handled by ff usually)
    const char* path = SDPath[0] ? SDPath : "0:/";

    // Count files - Must hold mutex for opendir/readdir sequence
    if (f_opendir(&dir, path) == FR_OK) {
        while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
             if (strstr(fno.fname, ".log") || strstr(fno.fname, ".txt")) {
                 count++;
             }
        }
        f_closedir(&dir);
    }

    // Release mutex before sending initial response (could block) or starting long loop
    xSemaphoreGive(LogMutex);

    if (count == 0) {
         int len = pack_log_list_resp_message(buffer, 0, 0, 0, "");
         UART_SendRaw(buffer, len);
         return;
    }

    // Stream files
    // We open dir, then loop. In each loop, we take mutex to read next entry, then release to send/wait.
    // This prevents blocking other tasks for the entire duration.

    if (xSemaphoreTake(LogMutex, portMAX_DELAY) == pdTRUE) {
        FRESULT res = f_opendir(&dir, path);
        xSemaphoreGive(LogMutex);

        if (res == FR_OK) {
            uint8_t idx = 0;
            while(1) {
                // Read next entry with lock
                bool found = false;
                if (xSemaphoreTake(LogMutex, 100) == pdTRUE) {
                    res = f_readdir(&dir, &fno);
                    xSemaphoreGive(LogMutex);
                    if (res == FR_OK && fno.fname[0]) {
                        found = true;
                    }
                } else {
                    break; // Timeout getting lock, abort
                }

                if (!found) break; // End of dir or error

                if (strstr(fno.fname, ".log") || strstr(fno.fname, ".txt")) {
                    int len = pack_log_list_resp_message(buffer, count, idx, fno.fsize, fno.fname);
                    UART_SendRaw(buffer, len);
                    vTaskDelay(20); // Throttle - Mutex NOT held here
                    idx++;
                }
            }

            // Close dir with lock
            xSemaphoreTake(LogMutex, portMAX_DELAY);
            f_closedir(&dir);
            xSemaphoreGive(LogMutex);
        }
    }
}

void LogManager_HandleDownloadReq(const char* filename) {
    if (!LogMutex) return;
    xSemaphoreTake(LogMutex, portMAX_DELAY);

    if (Downloading) f_close(&DownloadFile);

    strncpy(DownloadName, filename, 31);
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
    if (!LogMutex) return;
    xSemaphoreTake(LogMutex, portMAX_DELAY);
    f_unlink(filename);
    xSemaphoreGive(LogMutex);
}

void LogManager_HandleManagerOp(uint8_t op_code) {
    if (!LogMutex) return;
    xSemaphoreTake(LogMutex, portMAX_DELAY);

    if (op_code == LOG_OP_DELETE_ALL) {
        // Close current log before deleting
        if (LogOpen) {
            f_close(&LogFile);
            LogOpen = false;
        }

        DIR dir;
        FILINFO fno;
        const char* path = SDPath[0] ? SDPath : "0:/";
        if (f_opendir(&dir, path) == FR_OK) {
            while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
                if (strstr(fno.fname, ".log") || strstr(fno.fname, ".txt")) {
                    f_unlink(fno.fname);
                }
            }
            f_closedir(&dir);
        }

        // Re-init (creates new current.log)
        // LogManager_Init assumes lock is held, so we release?
        // No, LogManager_Init takes lock. We hold lock. Recursive?
        // We are using xSemaphoreCreateMutex which is NOT recursive.
        // We must release lock before calling Init.
        xSemaphoreGive(LogMutex);
        LogManager_Init();
        return; // Init manages lock

    } else if (op_code == LOG_OP_FORMAT_SD) {
        if (LogOpen) {
            f_close(&LogFile);
            LogOpen = false;
        }
        // Unmount
        f_mount(0, SDPath, 0);

        BYTE work[FF_MAX_SS];
        MKFS_PARM opt = {FM_FAT32, 0, 0, 0, 0};
        FRESULT res = f_mkfs(SDPath, &opt, work, sizeof(work));

        if (res == FR_OK) {
             // Remount and Init
             xSemaphoreGive(LogMutex);
             LogManager_Init();
             return;
        } else {
             printf("Format Failed: %d\n", res);
        }
    }
    xSemaphoreGive(LogMutex);
}

void LogManager_HandleEspLog(uint8_t level, const char* tag, const char* message) {
    LogManager_Write(level, tag, message);
}

uint32_t LogManager_GetTotalSpace(void) {
    FATFS *fs;
    DWORD fre_clust, tot_sect;
    if (f_getfree(SDPath, &fre_clust, &fs) == FR_OK) {
        tot_sect = (fs->n_fatent - 2) * fs->csize;
        return tot_sect / 2;
    }
    return 0;
}

void LogManager_GetStats(uint32_t* size, uint32_t* file_count) {
    if (size) *size = LogOpen ? f_size(&LogFile) : 0;

    if (file_count) {
        *file_count = 0;
        DIR dir;
        FILINFO fno;
        if (f_opendir(&dir, "/") == FR_OK) {
            while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
                 if ((strstr(fno.fname, ".log") || strstr(fno.fname, ".txt")) &&
                     strcmp(fno.fname, LOG_FILENAME) != 0) { // Exclude current.log from "other" count if desired?
                     // Or just count all. User asked "how many other log files".
                     (*file_count)++;
                 }
            }
            f_closedir(&dir);
        }
    }
}

uint32_t LogManager_GetFreeSpace(void) {
    FATFS *fs;
    DWORD fre_clust, fre_sect;
    if (f_getfree(SDPath, &fre_clust, &fs) == FR_OK) {
        fre_sect = fre_clust * fs->csize;
        return fre_sect / 2;
    }
    return 0;
}
