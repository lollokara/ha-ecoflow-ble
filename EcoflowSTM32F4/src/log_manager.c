#include "log_manager.h"
#include "sd_card.h"
#include "uart_task.h"
#include "ecoflow_protocol.h"
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define MAX_LOG_SIZE (5 * 1024 * 1024) // 5MB
#define CURRENT_LOG_FILE "log_current.txt"

static bool logging_enabled = true;
static FIL current_file;
static bool file_open = false;

// Streaming State
static bool streaming = false;
static char stream_file[32];
static uint32_t stream_offset = 0;
static FIL stream_fp;

void LogManager_Rotate(void);
void LogManager_WriteInternal(const char* buffer);

void LogManager_Init(void) {
    if (SD_Mount()) {
        // Check if current log exists and size
        FILINFO fno;
        if (f_stat(CURRENT_LOG_FILE, &fno) == FR_OK) {
            if (fno.fsize >= MAX_LOG_SIZE) {
                LogManager_Rotate();
            }
        }
    }
}

void LogManager_SetLogging(bool enabled) {
    logging_enabled = enabled;
}

bool LogManager_IsLogging(void) {
    return logging_enabled;
}

void LogManager_GetSpace(uint32_t *total, uint32_t *free) {
    SD_GetSpace(total, free);
}

void LogManager_Log(const char* fmt, ...) {
    if (!logging_enabled) return;

    char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);

    LogManager_WriteInternal(buffer);
}

void LogManager_WriteInternal(const char* buffer) {
    if (!file_open) {
        if (f_open(&current_file, CURRENT_LOG_FILE, FA_OPEN_ALWAYS | FA_WRITE | FA_OPEN_APPEND) == FR_OK) {
            file_open = true;

            // Check size again
            if (f_size(&current_file) >= MAX_LOG_SIZE) {
                f_close(&current_file);
                file_open = false;
                LogManager_Rotate();
                // Recursively call to open new file
                LogManager_WriteInternal(buffer);
                return;
            }
        } else {
            return;
        }
    }

    UINT bw;
    f_write(&current_file, buffer, strlen(buffer), &bw);
    f_sync(&current_file);

    // Check size
    if (f_size(&current_file) >= MAX_LOG_SIZE) {
        f_close(&current_file);
        file_open = false;
        LogManager_Rotate();
    }
}

void LogManager_Rotate(void) {
    char new_name[32];
    int index = 0;
    FILINFO fno;

    // Find next available index
    do {
        snprintf(new_name, sizeof(new_name), "log_%d.txt", index);
        index++;
    } while (f_stat(new_name, &fno) == FR_OK);

    f_rename(CURRENT_LOG_FILE, new_name);

    // Start new file
    if (f_open(&current_file, CURRENT_LOG_FILE, FA_CREATE_ALWAYS | FA_WRITE) == FR_OK) {
        file_open = true;
        // Section 1 Header
        f_printf(&current_file, "--- Firmware Version ---\nF4: 1.0.0\nESP32: 1.0.0\n\n");

        // Request ESP Info (Section 2 & 3)
        UART_SendData(CMD_LOG_REQ_INFO, NULL, 0);

        f_sync(&current_file);
    }
}

void LogManager_HandleList(void) {
    DIR dir;
    FILINFO fno;
    LogListMsg msg;
    msg.count = 0;

    if (f_opendir(&dir, "") == FR_OK) {
        while (f_readdir(&dir, &fno) == FR_OK && msg.count < 5) {
            if (fno.fname[0] == 0) break;
            if (strstr(fno.fname, ".txt")) {
                strncpy(msg.files[msg.count].filename, fno.fname, 31);
                msg.files[msg.count].size = fno.fsize;
                msg.count++;
            }
        }
        f_closedir(&dir);
    }

    // Send List
    uint8_t buffer[sizeof(LogListMsg) + 4];
    int len = pack_log_list_message(buffer, &msg);
    UART_SendData(CMD_LOG_LIST, &buffer[3], len-4); // SendData adds header, so pass payload
    // Wait, UART_SendData takes cmd, data, len.
    // pack function returns FULL packet.
    // I should extract payload.
    // Actually, `pack_log_list_message` populates `buffer` with [AA][CMD][LEN][PAYLOAD][CRC].
    // `UART_SendData` takes `data` and wraps it.
    // This is double wrapping!
    // I need to use `UART_SendData` with just the struct content?
    // `ecoflow_protocol.h` pack functions create the full packet.
    // I should probably use `HAL_UART_Transmit` directly OR `UART_SendData` with struct.
    // `UART_SendData` signature: `void UART_SendData(uint8_t cmd, uint8_t* data, uint8_t len)`
    // It constructs the packet.
    // So I should NOT use `pack_log_list_message` if I use `UART_SendData`.
    // Instead I should just send the struct.
    UART_SendData(CMD_LOG_LIST, (uint8_t*)&msg, sizeof(LogListMsg)); // Simplified
}

void LogManager_HandleDownload(const char* filename, uint32_t offset) {
    // Stop any existing stream
    if (streaming) {
        f_close(&stream_fp);
        streaming = false;
    }

    if (f_open(&stream_fp, filename, FA_READ) == FR_OK) {
        if (f_lseek(&stream_fp, offset) == FR_OK) {
            streaming = true;
            strncpy(stream_file, filename, 31);
            stream_offset = offset;
        } else {
             f_close(&stream_fp);
        }
    }
}

void StartLogTask(void * argument) {
    // Initial delay to allow SD to stabilize
    vTaskDelay(pdMS_TO_TICKS(1000));

    LogManager_Init();

    for(;;) {
        if (streaming) {
            uint8_t buffer[200]; // Max chunk size
            UINT br;

            if (f_read(&stream_fp, buffer, sizeof(buffer), &br) == FR_OK) {
                if (br > 0) {
                    UART_SendData(CMD_LOG_STREAM_DATA, buffer, br);
                    stream_offset += br;
                    // Small delay to prevent UART saturation
                    vTaskDelay(pdMS_TO_TICKS(5));
                } else {
                    // EOF
                    streaming = false;
                    f_close(&stream_fp);
                    UART_SendData(CMD_LOG_STREAM_DATA, NULL, 0);
                }
            } else {
                streaming = false;
                f_close(&stream_fp);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

void LogManager_HandleDelete(const char* filename) {
    f_unlink(filename);
    LogManager_HandleList(); // Refresh list
}

void LogManager_HandleDeleteAll(void) {
    DIR dir;
    FILINFO fno;

    // Close current if open
    if (file_open) {
        f_close(&current_file);
        file_open = false;
    }

    if (f_opendir(&dir, "") == FR_OK) {
        while (f_readdir(&dir, &fno) == FR_OK) {
            if (fno.fname[0] == 0) break;
            if (strstr(fno.fname, ".txt")) {
                f_unlink(fno.fname);
            }
        }
        f_closedir(&dir);
    }

    // Restart logging
    LogManager_Init();
}

void LogManager_HandleFormat(void) {
    if (file_open) {
        f_close(&current_file);
        file_open = false;
    }
    SD_Format();
    LogManager_Init();
}

void LogManager_HandlePushData(uint8_t type, const char* msg) {
    // Received from ESP. Write to log.
    // Format: [Source] Msg
    LogManager_Log("[ESP32] %s\n", msg);
}
