#include "ota_core.h"
#include "flash_writer.h"
#include "../display_task.h"
#include "stm32f4xx_hal.h"
#include <string.h>

extern QueueHandle_t displayQueue;

#define CMD_OTA_START   0xF0
#define CMD_OTA_DATA    0xF1
#define CMD_OTA_END     0xF2

typedef enum {
    OTA_STATE_IDLE,
    OTA_STATE_STARTING,
    OTA_STATE_RECEIVING,
    OTA_STATE_DONE
} OtaState;

static OtaState _state = OTA_STATE_IDLE;
static uint32_t _totalSize = 0;
static uint32_t _receivedSize = 0;
static uint32_t _checksum = 0;
static uint32_t _runningCrc = 0xFFFFFFFF;

// Calculate STM32 Hardware Compatible CRC32
// Matches ESP32's stm32_crc32 implementation
static uint32_t bootloader_crc(uint8_t *data, uint32_t len) {
    // Updates global _runningCrc
    // Safe handling of unaligned access by reconstructing words from bytes

    // Note: Assuming len is multiple of 4, which is typical for chunks.
    // If not, tail bytes are ignored here (and in ESP32 implementation).
    uint32_t n_words = len / 4;

    for(uint32_t i=0; i<n_words; i++) {
        uint32_t val = 0;
        // Construct word from bytes (Little Endian)
        val |= (uint32_t)data[i*4 + 0] << 0;
        val |= (uint32_t)data[i*4 + 1] << 8;
        val |= (uint32_t)data[i*4 + 2] << 16;
        val |= (uint32_t)data[i*4 + 3] << 24;

        _runningCrc ^= val;
        for(int j=0; j<32; j++) {
             if (_runningCrc & 0x80000000) _runningCrc = (_runningCrc << 1) ^ 0x04C11DB7;
             else _runningCrc <<= 1;
        }
    }
    return _runningCrc;
}

void OtaCore_Init(void) {
    _state = OTA_STATE_IDLE;
}

void OtaCore_HandleCmd(uint8_t cmd_id, uint8_t *data, uint32_t len) {
    if (cmd_id == CMD_OTA_START) {
        // Data: [Size(4)][Checksum(4)]
        if (len < 8) return;
        memcpy(&_totalSize, data, 4);
        memcpy(&_checksum, data + 4, 4);

        _state = OTA_STATE_STARTING;
        _receivedSize = 0;
        _runningCrc = 0xFFFFFFFF;

        // Notify UI
        DisplayEvent event;
        event.type = DISPLAY_EVENT_OTA_PROGRESS;
        event.data.otaPercent = 0;
        xQueueSend(displayQueue, &event, 0);

        Flash_Unlock();
        if (!Flash_EraseBank2()) {
            // Error handling?
            return;
        }

        // Send ACK (CMD_OTA_START response)
        // Just send back the same payload or simple ACK
        // For simplicity, reusing same packet logic in uart_task might be hard without exposing transmit.
        // Let's assume ESP32 waits on time for now, as planned.

        _state = OTA_STATE_RECEIVING;

    } else if (cmd_id == CMD_OTA_DATA) {
        if (_state != OTA_STATE_RECEIVING) return;

        // Write to Flash
        if (!Flash_WriteChunk(_receivedSize, data, len)) {
            // Error
            return;
        }

        // Update CRC
        bootloader_crc(data, len);

        _receivedSize += len;

        // Update UI
        int percent = (_receivedSize * 100) / _totalSize;
        DisplayEvent event;
        event.type = DISPLAY_EVENT_OTA_PROGRESS;
        event.data.otaPercent = (uint8_t)percent;
        xQueueSend(displayQueue, &event, 0);

    } else if (cmd_id == CMD_OTA_END) {
        if (_state != OTA_STATE_RECEIVING) return;

        // Finalize
        // Verify Checksum
        if (_runningCrc != _checksum) {
             // Checksum Mismatch!
             // Do not set update flag.
             _state = OTA_STATE_IDLE; // Reset
             // Ideally report error to UI/UART
             return;
        }

        if (Flash_SetUpdateFlag(_totalSize, _checksum)) { // Use the received checksum which matches Bootloader expectation
             Flash_Lock();
             // UI Update is handled via DisplayQueue
             HAL_Delay(1000);
             NVIC_SystemReset();
        }
    }
}
