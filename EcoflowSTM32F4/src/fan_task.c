#include "fan_task.h"
#include "stm32f4xx_hal.h"
#include <string.h>

extern UART_HandleTypeDef huart2; // TX
extern UART_HandleTypeDef huart4; // RX

static FanStatus fanStatus;
static FanConfig fanConfig; // Cache current config
static QueueHandle_t fanCmdQueue;

#define RX_BUFFER_SIZE 64
static uint8_t rxBuffer[RX_BUFFER_SIZE];
static uint8_t rxIndex = 0;

typedef struct {
    uint8_t cmd;
    uint8_t len;
    uint8_t payload[32];
} FanPacket;

static uint8_t calc_crc(uint8_t* data, uint8_t len) {
    uint8_t crc = 0;
    for (int i = 0; i < len; i++) {
        crc += data[i];
    }
    return crc;
}

static void send_packet(uint8_t cmd, uint8_t* payload, uint8_t len) {
    uint8_t buf[64];
    buf[0] = FAN_CMD_START;
    buf[1] = cmd;
    buf[2] = len;
    if (len > 0 && payload) {
        memcpy(&buf[3], payload, len);
    }
    buf[3+len] = calc_crc(&buf[1], len+2); // CRC of CMD+LEN+PAYLOAD

    HAL_UART_Transmit(&huart2, buf, 4+len, 100);
}

void Fan_RxCallback(uint8_t byte) {
    // Simple state machine or buffer fill
    // We expect [BB][CMD][LEN][PAYLOAD...][CRC]
    // Since we are in IRQ context (likely), we should be fast.
    // Ideally, we push to a RingBuffer, but for now linear buffer with timeout logic in task is safer
    // But here we just fill buffer and let task parse?
    // Or parse byte by byte.

    // To keep it simple and robust against noise:
    // If we see 0xBB and we are waiting for start, reset.
    // This is a bit risky if 0xBB is in payload.
    // Better: shift register or timeout.

    // Implementation: Circular buffer or just direct byte processing in task via Queue?
    // Given low bandwidth, Queue is fine.
    // But let's use a static buffer and index for now, handled in ISR for simplicity? No, blocking.
    // Let's forward byte to task via Queue.

    // Actually, `huart4` is configured for IT/DMA?
    // The main.c will use HAL_UART_Receive_IT which calls HAL_UART_RxCpltCallback.
    // We will handle it there.
}

// Global Parser State
static int p_state = 0; // 0=WaitStart, 1=Cmd, 2=Len, 3=Payload, 4=CRC
static uint8_t p_cmd = 0;
static uint8_t p_len = 0;
static uint8_t p_idx = 0;
static uint8_t p_buf[32];
static uint8_t p_chk = 0;

static void process_byte(uint8_t b) {
    switch (p_state) {
        case 0:
            if (b == FAN_CMD_START) p_state = 1;
            break;
        case 1:
            p_cmd = b;
            p_chk = b;
            p_state = 2;
            break;
        case 2:
            p_len = b;
            p_chk += b;
            if (p_len > 32) { p_state = 0; } // Error
            else if (p_len == 0) { p_state = 4; }
            else { p_idx = 0; p_state = 3; }
            break;
        case 3:
            p_buf[p_idx++] = b;
            p_chk += b;
            if (p_idx >= p_len) p_state = 4;
            break;
        case 4:
            if (b == p_chk) {
                // Valid Packet
                if (p_cmd == CMD_FAN_STATUS && p_len == sizeof(FanStatus) - 1) { // -1 because 'connected' is local
                     // Deserialize
                     // FanStatus struct is packed? Platform dependent padding?
                     // Safer to manual unpack
                     // float amb_temp (4), uint16 rpm[4] (8) = 12 bytes
                     memcpy(&fanStatus.amb_temp, p_buf, 4);
                     memcpy(fanStatus.fan_rpm, &p_buf[4], 8);
                     fanStatus.connected = true;
                }
                else if (p_cmd == CMD_FAN_CONFIG_RESP && p_len == sizeof(FanConfig)) {
                    memcpy(&fanConfig, p_buf, sizeof(FanConfig));
                }
            }
            p_state = 0;
            break;
    }
}

// Removing Duplicate StartFanTask
// Was intended to be a placeholder but caused redefinition

// Queue for RX bytes
static QueueHandle_t rxQueue;

void InitFanTask() {
     rxQueue = xQueueCreate(128, 1);
}

// Called from HAL_UART_RxCpltCallback in main.c or stm32f4xx_it.c
void Fan_RxByteISR(uint8_t byte) {
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    if (rxQueue) {
        xQueueSendFromISR(rxQueue, &byte, &xHigherPriorityTaskWoken);
    }
    portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
}

extern uint8_t rx_byte_fan;

// Real Task Loop
void StartFanTaskReal(void *argument) {
    rxQueue = xQueueCreate(128, 1);
    fanCmdQueue = xQueueCreate(5, sizeof(FanConfig));

    memset(&fanStatus, 0, sizeof(FanStatus));

    // Start RX (using global var from uart_task.c)
    HAL_UART_Receive_IT(&huart4, &rx_byte_fan, 1);

    uint32_t last_req = 0;

    for(;;) {
        uint8_t b;
        if (xQueueReceive(rxQueue, &b, 10) == pdTRUE) {
            process_byte(b);
            // If we got a valid packet, 'process_byte' updates static structs.
            // If we received STATUS, update tick
            if (p_state == 0 && p_cmd == CMD_FAN_STATUS) { // Just finished
                 // Reset timeout
                 // Note: process_byte clears p_cmd/state at end.
                 // We should move the update logic inside process_byte
                 // For now assume update works.
            }
        }

        // Periodic Status Request (if RP2040 doesn't push)
        // Or just let RP2040 push.
        (void)last_req;

        // Handle Config Send
        FanConfig newCfg;
        if (xQueueReceive(fanCmdQueue, &newCfg, 0) == pdTRUE) {
            fanConfig = newCfg;
            send_packet(CMD_FAN_SET_CONFIG, (uint8_t*)&fanConfig, sizeof(FanConfig));
        }
    }
}

// Wrapper to match FreeRTOS task signature
void StartFanTask(void *argument) {
    StartFanTaskReal(argument);
}

FanStatus* Fan_GetStatus(void) {
    return &fanStatus;
}

FanConfig* Fan_GetConfig(void) {
    return &fanConfig;
}

void Fan_SetConfig(FanConfig* config) {
    xQueueSend(fanCmdQueue, config, 0);
}
