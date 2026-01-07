#include "log_manager.h"
#include "ff.h"
#include "uart_task.h"
#include "FreeRTOS.h"
#include "task.h"
#include <stdio.h>
#include <stdarg.h>
#include <string.h>

#define MAX_LOG_SIZE (5 * 1024 * 1024)
#define LOG_FILENAME "current.log"

extern char SDPath[4];
extern FATFS SDFatFs;

static FIL LogFile;
static bool LogOpen = false;

// Download State
static bool Downloading = false;
static FIL DownloadFile;
static char DownloadName[32];
static uint32_t DownloadOffset = 0;
static uint32_t DownloadSize = 0;

void LogManager_Init(void) {
    // Mount Filesystem
    FRESULT res = f_mount(&SDFatFs, SDPath, 1);
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
    if (f_open(&LogFile, LOG_FILENAME, FA_OPEN_ALWAYS | FA_WRITE | FA_READ) == FR_OK) {
        f_lseek(&LogFile, f_size(&LogFile)); // Append
        LogOpen = true;

        // Check size
        if (f_size(&LogFile) > MAX_LOG_SIZE) {
            LogManager_ForceRotate();
        }
    }
}

void LogManager_ForceRotate(void) {
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

        // Header Section 1
        LogManager_Write(0, "SYS", "--- Firmware Versions ---");
        LogManager_Write(0, "SYS", "STM32 F4: v1.0.0"); // TODO: Get real version
        LogManager_Write(0, "SYS", "ESP32: v1.0.0");     // TODO: Get real version

        // Request Section 2 & 3
        uint8_t buf[32];
        int len;

        len = pack_simple_cmd_message(buf, CMD_GET_FULL_CONFIG);
        UART_SendRaw(buf, len);

        len = pack_simple_cmd_message(buf, CMD_GET_DEBUG_DUMP);
        UART_SendRaw(buf, len);
    }
}

void LogManager_Write(uint8_t level, const char* tag, const char* message) {
    if (!LogOpen) return;

    // Check size
    if (f_size(&LogFile) > MAX_LOG_SIZE) {
        LogManager_ForceRotate();
    }

    // Format: [Timestamp] [Tag] Message\n
    char line[512];
    uint32_t time = xTaskGetTickCount();
    // [millis] [Tag] Message
    snprintf(line, sizeof(line), "[%lu] [%s] %s\n", time, tag, message);

    UINT bw;
    f_write(&LogFile, line, strlen(line), &bw);
    f_sync(&LogFile); // Sync frequently or rely on periodic sync? Sync is safer but slower.
}

void LogManager_Process(void) {
    if (Downloading) {
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
    }
}

void LogManager_HandleListReq(void) {
    DIR dir;
    FILINFO fno;
    uint8_t buffer[256];
    uint8_t count = 0;

    if (f_opendir(&dir, "/") == FR_OK) {
        while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
             if (strstr(fno.fname, ".log") || strstr(fno.fname, ".txt")) {
                 count++;
             }
        }
        f_closedir(&dir);
    }

    uint8_t idx = 0;
    if (f_opendir(&dir, "/") == FR_OK) {
        while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
             if (strstr(fno.fname, ".log") || strstr(fno.fname, ".txt")) {
                 int len = pack_log_list_resp_message(buffer, count, idx, fno.fsize, fno.fname);
                 UART_SendRaw(buffer, len);
                 vTaskDelay(20); // Throttle increased to prevent ESP32 overflow
                 idx++;
             }
        }
        f_closedir(&dir);
    }

    if (count == 0) {
         int len = pack_log_list_resp_message(buffer, 0, 0, 0, "");
         UART_SendRaw(buffer, len);
    }
}

void LogManager_HandleDownloadReq(const char* filename) {
    if (Downloading) f_close(&DownloadFile);

    strncpy(DownloadName, filename, 31);
    if (f_open(&DownloadFile, filename, FA_READ) == FR_OK) {
        Downloading = true;
        DownloadOffset = 0;
        DownloadSize = f_size(&DownloadFile);
    } else {
        Downloading = false;
    }
}

void LogManager_HandleDeleteReq(const char* filename) {
    f_unlink(filename);
}

void LogManager_HandleManagerOp(uint8_t op_code) {
    if (op_code == LOG_OP_DELETE_ALL) {
        DIR dir;
        FILINFO fno;
        if (f_opendir(&dir, "/") == FR_OK) {
            while (f_readdir(&dir, &fno) == FR_OK && fno.fname[0]) {
                if (strstr(fno.fname, ".log") || strstr(fno.fname, ".txt")) {
                    f_unlink(fno.fname);
                }
            }
            f_closedir(&dir);
        }
    } else if (op_code == LOG_OP_FORMAT_SD) {
        BYTE work[FF_MAX_SS];
        MKFS_PARM opt = {FM_FAT32, 0, 0, 0, 0};
        f_mkfs(SDPath, &opt, work, sizeof(work));
        f_mount(&SDFatFs, SDPath, 1);
        LogManager_Init();
    }
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
