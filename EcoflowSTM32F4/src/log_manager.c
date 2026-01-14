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

// Sync State
static uint32_t LastSyncTime = 0;

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
        // Refresh IWDG before long format operation
        extern IWDG_HandleTypeDef hiwdg;
        HAL_IWDG_Refresh(&hiwdg);

        BYTE work[FF_MAX_SS];
        MKFS_PARM opt = {FM_FAT32, 0, 0, 0, 0};
        FRESULT fmt_res = f_mkfs(SDPath, &opt, work, sizeof(work));

        HAL_IWDG_Refresh(&hiwdg); // Refresh after format

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
            xSemaphoreGive(LogMutex);
            LogManager_ForceRotate();
            xSemaphoreTake(LogMutex, portMAX_DELAY);
        } else {
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
    uint8_t buf[32];
    int len;

    len = pack_simple_cmd_message(buf, CMD_GET_FULL_CONFIG);
    UART_SendRaw(buf, len);

    len = pack_simple_cmd_message(buf, CMD_GET_DEBUG_DUMP);
    UART_SendRaw(buf, len);
}

void LogManager_ForceRotate(void) {
    if (xSemaphoreTake(LogMutex, portMAX_DELAY) != pdTRUE) return;

    if (LogOpen) {
        f_close(&LogFile);
        LogOpen = false;
    }

    // Rename current.log to log_N.txt
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
    }

    if (LogOpen) {
        LogManager_WriteSessionHeader(); // Calls Write_Internal
    }

    xSemaphoreGive(LogMutex);
}

// Internal Write function - Assumes LogMutex is held by caller
static void LogManager_Write_Internal(uint8_t level, const char* tag, const char* message) {
    if (!LogOpen) return;

    // Check size and rotate if needed
    if (f_size(&LogFile) > MAX_LOG_SIZE) {
        // We must release lock to call ForceRotate because it takes lock
        xSemaphoreGive(LogMutex);
        LogManager_ForceRotate();
        xSemaphoreTake(LogMutex, portMAX_DELAY);
        if (!LogOpen) return;
    }

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

    // Conditional Sync
    if (level <= 1) { // Error or Warn
        f_sync(&LogFile);
    }
}

void LogManager_Write(uint8_t level, const char* tag, const char* message) {
    if (!LogMutex) return;
    if (xSemaphoreTake(LogMutex, 100) == pdTRUE) {
        LogManager_Write_Internal(level, tag, message);
        xSemaphoreGive(LogMutex);
    }
}

void LogManager_Process(void) {
    // Periodic Sync
    if (LogOpen && (xTaskGetTickCount() - LastSyncTime > 5000)) {
        if (xSemaphoreTake(LogMutex, 10) == pdTRUE) {
             f_sync(&LogFile);
             xSemaphoreGive(LogMutex);
             LastSyncTime = xTaskGetTickCount();
        }
    }

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
            FRESULT res = f_read(&DownloadFile, buffer, sizeof(buffer), &br);
            if (res == FR_OK) {
                if (br > 0) {
                    uint8_t packet[256];
                    int len = pack_log_data_chunk_message(packet, DownloadOffset, buffer, br);
                    UART_SendRaw(packet, len);
                }

                DownloadOffset += br;
                if (br < sizeof(buffer) || DownloadOffset >= DownloadSize) {
                    // End of file
                    printf("DL: EOF. Off=%lu Size=%lu\n", DownloadOffset, DownloadSize);
                    Downloading = false;
                    f_close(&DownloadFile);

                    // Restore LogFile if needed
                    if (!LogOpen) {
                         if (f_open(&LogFile, LOG_FILENAME, FA_OPEN_ALWAYS | FA_WRITE | FA_READ) == FR_OK) {
                             f_lseek(&LogFile, f_size(&LogFile));
                             LogOpen = true;
                             printf("DL: ActiveLog Restored (EOF)\n");
                         } else {
                             printf("DL: ActiveLog Restore Failed (EOF)\n");
                         }
                    }

                    // Send empty chunk
                    uint8_t packet[32];
                    int len = pack_log_data_chunk_message(packet, DownloadOffset, NULL, 0);
                    UART_SendRaw(packet, len);
                }
            } else {
                 // Error
                 printf("DL: Read Error res=%d\n", res);
                 Downloading = false;
                 f_close(&DownloadFile);

                 // Restore LogFile if needed (Error case)
                 if (!LogOpen) {
                      if (f_open(&LogFile, LOG_FILENAME, FA_OPEN_ALWAYS | FA_WRITE | FA_READ) == FR_OK) {
                          f_lseek(&LogFile, f_size(&LogFile));
                          LogOpen = true;
                          printf("DL: ActiveLog Restored (Error)\n");
                      }
                 }
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
    uint16_t count = 0;

    const char* path = SDPath[0] ? SDPath : "0:/";

    // Count files
    if (f_opendir(&dir, path) == FR_OK) {
        while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
             if (strstr(fno.fname, ".log") || strstr(fno.fname, ".txt")) {
                 count++;
             }
        }
        f_closedir(&dir);
    }

    // Must release mutex for UART send (avoid deadlock if UART calls LogManager_Write)
    xSemaphoreGive(LogMutex);

    if (count == 0) {
         int len = pack_log_list_resp_message(buffer, 0, 0, 0, "");
         UART_SendRaw(buffer, len);
         return;
    }

    // Stream files
    if (xSemaphoreTake(LogMutex, portMAX_DELAY) == pdTRUE) {
        FRESULT res = f_opendir(&dir, path);
        xSemaphoreGive(LogMutex);

        if (res == FR_OK) {
            uint16_t idx = 0;
            // Loop until we reach count. If dir ends early, fill with dummy.
            while(idx < count) {
                bool found = false;
                if (xSemaphoreTake(LogMutex, portMAX_DELAY) == pdTRUE) {
                    res = f_readdir(&dir, &fno);
                    xSemaphoreGive(LogMutex);
                    if (res == FR_OK && fno.fname[0]) {
                        if (strstr(fno.fname, ".log") || strstr(fno.fname, ".txt")) {
                            found = true;
                        } else {
                            // Non-log file, skip
                        }
                    } else {
                        // End of dir or error
                    }
                }

                if (found) {
                    int len = pack_log_list_resp_message(buffer, count, idx, fno.fsize, fno.fname);
                    UART_SendRaw(buffer, len);
                    idx++;
                    vTaskDelay(2); // Throttle every packet for reliability
                } else if (res != FR_OK || fno.fname[0] == 0) {
                    // End of dir reached, but idx < count (Under-run)
                    // Fill with dummy packets
                    while (idx < count) {
                        int len = pack_log_list_resp_message(buffer, count, idx, 0, "");
                        UART_SendRaw(buffer, len);
                        idx++;
                        vTaskDelay(2);
                    }
                    break;
                }
            }

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

    // Close LogFile if we are trying to download the active log (avoids sharing violation)
    if (LogOpen && strcmp(filename, LOG_FILENAME) == 0) {
        f_close(&LogFile);
        LogOpen = false;
        printf("DL: ActiveLog Closed for '%s'\n", filename);
    }

    strncpy(DownloadName, filename, 31);
    FRESULT res = f_open(&DownloadFile, filename, FA_READ);
    if (res == FR_OK) {
        Downloading = true;
        DownloadOffset = 0;
        DownloadSize = f_size(&DownloadFile);
        printf("DL: Opened '%s' size=%lu\n", filename, DownloadSize);
    } else {
        Downloading = false;
        printf("DL: Failed to open '%s' res=%d\n", filename, res);

        // Restore LogFile if we closed it and failed to open for download
        if (!LogOpen && strcmp(filename, LOG_FILENAME) == 0) {
             if (f_open(&LogFile, LOG_FILENAME, FA_OPEN_ALWAYS | FA_WRITE | FA_READ) == FR_OK) {
                 f_lseek(&LogFile, f_size(&LogFile));
                 LogOpen = true;
                 printf("DL: ActiveLog Restored\n");
             } else {
                 printf("DL: ActiveLog Restore Failed\n");
             }
        }

        // Send EOF to signal failure immediately
        uint8_t packet[32];
        int len = pack_log_data_chunk_message(packet, 0, NULL, 0);
        UART_SendRaw(packet, len);
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

        xSemaphoreGive(LogMutex);
        LogManager_Init();
        return;

    } else if (op_code == LOG_OP_FORMAT_SD) {
        if (LogOpen) {
            f_close(&LogFile);
            LogOpen = false;
        }
        f_mount(0, SDPath, 0);

        BYTE work[FF_MAX_SS];
        MKFS_PARM opt = {FM_FAT32, 0, 0, 0, 0};
        FRESULT res = f_mkfs(SDPath, &opt, work, sizeof(work));

        if (res == FR_OK) {
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
                     strcmp(fno.fname, LOG_FILENAME) != 0) {
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
