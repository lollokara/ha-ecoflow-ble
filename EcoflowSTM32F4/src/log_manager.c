#include "log_manager.h"
#include "sd_diskio.h"
#include "ff_gen_drv.h"
#include "uart_task.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include "ecoflow_protocol.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

static FATFS SDFatFs;
static char SDPath[4];
static bool fs_mounted = false;

typedef struct {
    char func[32];
    char msg[128];
} LogEntry;

static QueueHandle_t logQueue;
static TaskHandle_t logTaskHandle;
static char currentLogFile[32];
static const uint32_t MAX_LOG_SIZE = 5 * 1024 * 1024; // 5MB

void LogManager_Task(void *arg); // Forward Declaration

// Helper to get next filename
static void get_new_filename(char* buffer) {
    sprintf(buffer, "current.log");
}

static void rotate_logs(void) {
    FILINFO fno;
    if (f_stat("current.log", &fno) == FR_OK) {
        if (fno.fsize >= MAX_LOG_SIZE) {
            char new_name[32];
            for (int i = 0; i < 999; i++) {
                sprintf(new_name, "log_%03d.txt", i);
                if (f_stat(new_name, &fno) != FR_OK) {
                    // File doesn't exist, use this name
                    f_rename("current.log", new_name);
                    break;
                }
            }
        }
    }
}

bool LogManager_Init(void) {
    if (f_mount(&SDFatFs, "", 1) != FR_OK) {
        // Try formatting if mount fails? No, risky.
        fs_mounted = false;
        return false;
    }
    fs_mounted = true;

    // Check for rotation on boot
    rotate_logs();

    return true;
}

void LogManager_Write(const char* func, const char* msg) {
    if (!logQueue) return;
    LogEntry entry;
    // Ensure null termination
    strncpy(entry.func, func, sizeof(entry.func)-1);
    entry.func[sizeof(entry.func)-1] = 0;

    strncpy(entry.msg, msg, sizeof(entry.msg)-1);
    entry.msg[sizeof(entry.msg)-1] = 0;

    xQueueSend(logQueue, &entry, 0);
}

void LogManager_StartTask(void) {
    logQueue = xQueueCreate(20, sizeof(LogEntry));
    xTaskCreate((TaskFunction_t)LogManager_Task, "LogTask", 4096, NULL, 1, &logTaskHandle);
}

void LogManager_Task(void *arg) {
    LogEntry entry;
    FIL file;
    // UINT bw; // Unused

    // Try to init if not already
    if (!fs_mounted) {
        LogManager_Init();
    }

    for(;;) {
        if (xQueueReceive(logQueue, &entry, portMAX_DELAY) == pdTRUE) {
            if (!fs_mounted) continue;

            // Check rotation before opening
            rotate_logs();

            if (f_open(&file, "current.log", FA_OPEN_APPEND | FA_WRITE) == FR_OK) {
                // Format: [FUNC] MSG\n
                f_printf(&file, "[%s] %s\n", entry.func, entry.msg);
                f_close(&file);
            }
        }
    }
}

void LogManager_GetList(void) {
    if (!fs_mounted) return;

    DIR dir;
    FILINFO fno;
    char buffer[1024]; // Max buffer for JSON list
    int offset = 0;

    // Start JSON Array
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "[");

    // We only send the latest 5 logs to avoid packet overflow (255 byte limit)
    // and keep it simple. Ideally we'd sort by date, but FATFS doesn't guarantee order.
    // We'll just take the first 5 found for now, or look for specific pattern.
    // Given rotation: log_000.txt... log_999.txt.

    int count = 0;
    if (f_opendir(&dir, "") == FR_OK) {
        bool first = true;
        while (1) {
            if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0) break;
            if (!(fno.fattrib & AM_DIR)) {
                // Only list .txt or .log files
                if (strstr(fno.fname, ".txt") || strstr(fno.fname, ".log")) {
                    if (count >= 5) break; // Limit to 5

                    if (!first) offset += snprintf(buffer + offset, sizeof(buffer) - offset, ",");
                    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "\"%s\"", fno.fname);
                    first = false;
                    count++;
                }
            }
        }
        f_closedir(&dir);
    }

    // End JSON Array
    offset += snprintf(buffer + offset, sizeof(buffer) - offset, "]");

    // Construct Packet: [START][CMD][LEN][PAYLOAD][CRC]
    // CMD_LOG_LIST_RESP (0x72)
    // We send it raw via UART_SendRaw
    uint8_t packet[1030];
    packet[0] = START_BYTE;
    packet[1] = CMD_LOG_LIST_RESP;

    // Length (Simple byte len if < 255, but JSON might be longer!)
    // Protocol limit is 255. If longer, we truncate or need chunking.
    // For now, let's assume it fits or truncate.
    if (offset > 250) offset = 250;

    packet[2] = (uint8_t)offset;
    memcpy(&packet[3], buffer, offset);

    packet[3 + offset] = calculate_crc8(&packet[1], 2 + offset);

    UART_SendRaw(packet, 4 + offset);
}

void LogManager_StartTransfer(const char* filename) {
    if (!fs_mounted) return;

    FIL file;
    if (f_open(&file, filename, FA_READ) != FR_OK) {
        // Send EOF immediately if fail
        uint8_t packet[4];
        packet[0] = START_BYTE;
        packet[1] = CMD_LOG_FILE_EOF;
        packet[2] = 0;
        packet[3] = calculate_crc8(&packet[1], 2);
        UART_SendRaw(packet, 4);
        return;
    }

    uint8_t read_buf[200]; // Keep payload < 255
    uint8_t packet[260];
    UINT bytesRead;

    while(1) {
        if (f_read(&file, read_buf, sizeof(read_buf), &bytesRead) != FR_OK || bytesRead == 0) {
            break;
        }

        // Construct Data Packet
        packet[0] = START_BYTE;
        packet[1] = CMD_LOG_FILE_DATA;
        packet[2] = (uint8_t)bytesRead;
        memcpy(&packet[3], read_buf, bytesRead);
        packet[3 + bytesRead] = calculate_crc8(&packet[1], 2 + bytesRead);

        UART_SendRaw(packet, 4 + bytesRead);

        // Small delay to prevent UART flooding (ESP32 buffer is limited)
        vTaskDelay(pdMS_TO_TICKS(10));
    }

    f_close(&file);

    // Send EOF
    uint8_t packet_eof[4];
    packet_eof[0] = START_BYTE;
    packet_eof[1] = CMD_LOG_FILE_EOF;
    packet_eof[2] = 0;
    packet_eof[3] = calculate_crc8(&packet_eof[1], 2);
    UART_SendRaw(packet_eof, 4);
}

void LogManager_DeleteAll(void) {
    if (!fs_mounted) return;

    DIR dir;
    FILINFO fno;
    if (f_opendir(&dir, "") == FR_OK) {
        while (1) {
            if (f_readdir(&dir, &fno) != FR_OK || fno.fname[0] == 0) break;
            if (!(fno.fattrib & AM_DIR)) {
                // Only delete .txt or .log files
                if (strstr(fno.fname, ".txt") || strstr(fno.fname, ".log")) {
                    f_unlink(fno.fname);
                }
            }
        }
        f_closedir(&dir);
    }
}

void LogManager_Format(void) {
    if (!fs_mounted) return;

    // MKFS
    BYTE work[FF_MAX_SS];
    f_mkfs("", 0, work, sizeof(work));
}

void LogManager_GetSpace(uint32_t *used_mb, uint32_t *total_mb) {
    FATFS *fs;
    DWORD fre_clust, fre_sect, tot_sect;

    if (f_getfree("", &fre_clust, &fs) == FR_OK) {
        tot_sect = (fs->n_fatent - 2) * fs->csize;
        fre_sect = fre_clust * fs->csize;

        *total_mb = tot_sect / 2048; // Assumes 512B sectors (512 * 2048 = 1MB)
        *used_mb = (tot_sect - fre_sect) / 2048;
    } else {
        *used_mb = 0;
        *total_mb = 0;
    }
}
