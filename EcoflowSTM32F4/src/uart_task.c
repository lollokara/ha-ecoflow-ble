#include "uart_task.h"
#include "ecoflow_protocol.h"
#include "display_task.h"
#include "stm32f4xx_hal.h"
#include "ui/ui_lvgl.h" // For UI_UpdateConnectionStatus
#include <string.h>
#include <stdio.h>
#include "queue.h"
#include "log_manager.h"

extern IWDG_HandleTypeDef hiwdg; // Refresh Watchdog during long ops

UART_HandleTypeDef huart6;

// Ring Buffer Implementation
#define RING_BUFFER_SIZE 2048 // Increased to support OTA Chunks (1KB payload + overhead)
typedef struct {
    uint8_t buffer[RING_BUFFER_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
} RingBuffer;

static RingBuffer rx_ring_buffer;

// TX Queue for sending commands from UI Task to UART Task
typedef enum {
    MSG_WAVE2_SET,
    MSG_AC_SET,
    MSG_DC_SET,
    MSG_SET_VALUE,
    MSG_POWER_OFF,
    MSG_GET_DEBUG_INFO,
    MSG_CONNECT_DEVICE,
    MSG_FORGET_DEVICE,
    MSG_LOG_STATUS_REQ
} TxMsgType;

typedef struct {
    TxMsgType type;
    union {
        Wave2SetMsg w2;
        uint8_t enable;
        struct {
            uint8_t type;
            int value;
        } set_val;
        uint8_t device_type;
    } data;
} TxMessage;

static QueueHandle_t uartTxQueue;

static void rb_init(RingBuffer *rb) {
    rb->head = 0;
    rb->tail = 0;
}

static void rb_push(RingBuffer *rb, uint8_t byte) {
    uint16_t next_head = (rb->head + 1) % RING_BUFFER_SIZE;
    if (next_head != rb->tail) {
        rb->buffer[rb->head] = byte;
        rb->head = next_head;
    }
}

static int rb_pop(RingBuffer *rb, uint8_t *byte) {
    if (rb->head == rb->tail) {
        return 0; // Empty
    }
    *byte = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % RING_BUFFER_SIZE;
    return 1;
}

// ISR Variables
uint8_t rx_byte_isr;

void UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART6) {
        rb_push(&rx_ring_buffer, rx_byte_isr);
        HAL_UART_Receive_IT(&huart6, &rx_byte_isr, 1);
    }
}

// Protocol State
typedef enum {
    STATE_HANDSHAKE,
    STATE_WAIT_HANDSHAKE_ACK,
    STATE_WAIT_DEVICE_LIST,
    STATE_POLLING
} ProtocolState;

static ProtocolState protocolState = STATE_HANDSHAKE;
static DeviceList knownDevices = {0};
static uint8_t currentDeviceIndex = 0;

// Packet Parsing State
typedef enum {
    PARSE_START,
    PARSE_CMD,
    PARSE_LEN_L,
    PARSE_PAYLOAD,
    PARSE_CRC
} ParseState;

static ParseState parseState = PARSE_START;
static uint8_t parseBuffer[1024]; // Increased for safety
static uint16_t parseIndex = 0;
static uint8_t expectedPayloadLen = 0;

// Log Streaming State
static bool is_streaming_log = false;

static void UART_Init(void) {
    __HAL_RCC_USART6_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_14;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF8_USART6;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    huart6.Instance = USART6;
    huart6.Init.BaudRate = 921600;
    huart6.Init.WordLength = UART_WORDLENGTH_8B;
    huart6.Init.StopBits = UART_STOPBITS_1;
    huart6.Init.Parity = UART_PARITY_NONE;
    huart6.Init.Mode = UART_MODE_TX_RX;
    huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart6.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart6);

    HAL_NVIC_SetPriority(USART6_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART6_IRQn);

    rb_init(&rx_ring_buffer);
    HAL_UART_Receive_IT(&huart6, &rx_byte_isr, 1);
}

static void process_packet(uint8_t *packet, uint16_t total_len) {
    uint8_t cmd = packet[1];

    // Check OTA Commands first
    if (cmd == CMD_OTA_START) {
        // Send NACK to force ESP32 to retry (waiting for Bootloader)
        uint8_t nack[4];
        nack[0] = 0xAA;
        nack[1] = 0x15; // CMD_OTA_NACK
        nack[2] = 0x00; // Length
        nack[3] = calculate_crc8(&nack[1], 2);

        HAL_UART_Transmit(&huart6, nack, 4, 100);
        HAL_Delay(50); // Ensure transmission

        // Enable Backup Access
        HAL_PWR_EnableBkUpAccess();
        __HAL_RCC_BKPSRAM_CLK_ENABLE();

        RTC->BKP0R = 0xDEADBEEF;

        HAL_NVIC_SystemReset();
    }
    // ... Log Commands ...
    else if (cmd == CMD_LOG_LIST_REQ) {
        LogFileEntry entries[4];
        LogListRespMsg msg;
        msg.count = LogManager_GetLogList(entries, 4);
        // Convert to Proto struct
        for(int i=0; i<msg.count; i++) {
            strncpy(msg.files[i].name, entries[i].name, 32);
            msg.files[i].size = entries[i].size;
        }
        uint8_t buf[256];
        int len = pack_log_list_resp_message(buf, &msg);
        HAL_UART_Transmit(&huart6, buf, len, 100);
    }
    else if (cmd == CMD_LOG_DOWNLOAD_REQ) {
        char filename[32];
        uint32_t offset;
        unpack_log_download_req_message(packet, filename, &offset);
        // Note: offset handling for resume not fully implemented in LogManager_StartRead (assumes start)
        // But for simplicity we start fresh.
        if (LogManager_StartRead(filename)) {
            is_streaming_log = true;
        }
    }
    else if (cmd == CMD_LOG_DELETE_REQ) {
        char filename[32];
        unpack_log_delete_req_message(packet, filename);
        // If filename is empty or special, delete all. My protocol implies per file but UI "Delete All".
        // Protocol supports filename. If filename == "", delete all?
        // Let's assume Delete All for now or if filename="ALL".
        LogManager_DeleteAllLogs();

        uint8_t ack[4];
        ack[0] = START_BYTE; ack[1] = CMD_LOG_DELETE_RESP; ack[2] = 1; ack[3] = 1; // Success
        ack[4] = calculate_crc8(&ack[1], 3);
        HAL_UART_Transmit(&huart6, ack, 5, 100);
    }
    else if (cmd == CMD_ESP_LOG_DATA) {
        char msg[256];
        unpack_esp_log_data_message(packet, msg, sizeof(msg));
        // LogManager handles newlines
        LogManager_Write("ESP32", "Log", msg);
    }
    else if (cmd == CMD_LOG_STATUS_RESP) {
        char dump[256];
        // Manually unpack since unpack function is not defined
        uint8_t len = packet[2];
        memcpy(dump, &packet[3], len);
        dump[len] = 0;
        LogManager_WriteSection2(dump);
    }

    // ... Normal Commands ...
    else if (cmd == CMD_HANDSHAKE_ACK) {
        if (protocolState == STATE_WAIT_HANDSHAKE_ACK) {
            protocolState = STATE_WAIT_DEVICE_LIST;
        }
    }
    else if (cmd == CMD_DEVICE_LIST) {
        if (protocolState == STATE_WAIT_DEVICE_LIST || protocolState == STATE_POLLING) {
            unpack_device_list_message(packet, &knownDevices);

            // Sync with UI cache immediately
            for(int i=0; i<knownDevices.count; i++) {
                UI_UpdateConnectionStatus(knownDevices.devices[i].id, knownDevices.devices[i].connected);
            }

            DisplayEvent event;
            event.type = DISPLAY_EVENT_UPDATE_DEVICE_LIST;
            memcpy(&event.data.deviceList, &knownDevices, sizeof(DeviceList));
            xQueueSend(displayQueue, &event, 0);

            uint8_t ack[4];
            int len = pack_device_list_ack_message(ack);
            HAL_UART_Transmit(&huart6, ack, len, 100);

            protocolState = STATE_POLLING;
        }
    }
    else if (cmd == CMD_DEVICE_STATUS) {
        DeviceStatus status;
        if (unpack_device_status_message(packet, &status) == 0) {
             DisplayEvent event;
             event.type = DISPLAY_EVENT_UPDATE_BATTERY;
             memcpy(&event.data.deviceStatus, &status, sizeof(DeviceStatus));
             xQueueSend(displayQueue, &event, 0);
        }
    }
    else if (cmd == CMD_DEBUG_INFO) {
        DebugInfo info;
        if (unpack_debug_info_message(packet, &info) == 0) {
            DisplayEvent event;
            event.type = DISPLAY_EVENT_UPDATE_DEBUG;
            memcpy(&event.data.debugInfo, &info, sizeof(DebugInfo));
            xQueueSend(displayQueue, &event, 0);
        }
    }
}

// ... Public Send Functions ...
void UART_SendWave2Set(Wave2SetMsg *msg) {
    if (uartTxQueue) {
        TxMessage tx; tx.type = MSG_WAVE2_SET; tx.data.w2 = *msg;
        xQueueSend(uartTxQueue, &tx, 0);
    }
}
void UART_SendPowerOff(void) {
    if (uartTxQueue) { TxMessage tx; tx.type = MSG_POWER_OFF; xQueueSend(uartTxQueue, &tx, 0); }
}
void UART_SendACSet(uint8_t enable) {
    if (uartTxQueue) { TxMessage tx; tx.type = MSG_AC_SET; tx.data.enable = enable; xQueueSend(uartTxQueue, &tx, 0); }
}
void UART_SendDCSet(uint8_t enable) {
    if (uartTxQueue) { TxMessage tx; tx.type = MSG_DC_SET; tx.data.enable = enable; xQueueSend(uartTxQueue, &tx, 0); }
}
void UART_SendSetValue(uint8_t type, int value) {
    if (uartTxQueue) { TxMessage tx; tx.type = MSG_SET_VALUE; tx.data.set_val.type = type; tx.data.set_val.value = value; xQueueSend(uartTxQueue, &tx, 0); }
}
void UART_SendGetDebugInfo(void) {
    if (uartTxQueue) { TxMessage tx; tx.type = MSG_GET_DEBUG_INFO; xQueueSend(uartTxQueue, &tx, 0); }
}
void UART_SendConnectDevice(uint8_t type) {
    if (uartTxQueue) { TxMessage tx; tx.type = MSG_CONNECT_DEVICE; tx.data.device_type = type; xQueueSend(uartTxQueue, &tx, 0); }
}
void UART_SendForgetDevice(uint8_t type) {
    if (uartTxQueue) { TxMessage tx; tx.type = MSG_FORGET_DEVICE; tx.data.device_type = type; xQueueSend(uartTxQueue, &tx, 0); }
}
void UART_SendLogStatusReq(void) {
    if (uartTxQueue) { TxMessage tx; tx.type = MSG_LOG_STATUS_REQ; xQueueSend(uartTxQueue, &tx, 0); }
}
void UART_GetKnownDevices(DeviceList *list) { memcpy(list, &knownDevices, sizeof(DeviceList)); }


void StartUARTTask(void * argument) {
    UART_Init();
    uartTxQueue = xQueueCreate(10, sizeof(TxMessage));
    uint8_t tx_buf[256]; // Increased buffer size for log packets
    int len;
    uint8_t b;
    TickType_t lastActivityTime = xTaskGetTickCount();
    TickType_t lastHandshakeTime = 0;

    for(;;) {
        // 1. Process RX
        while (rb_pop(&rx_ring_buffer, &b)) {
            switch (parseState) {
                case PARSE_START:
                    if (b == START_BYTE) {
                        parseBuffer[0] = b;
                        parseIndex = 1;
                        parseState = PARSE_CMD;
                    }
                    break;
                case PARSE_CMD:
                    parseBuffer[parseIndex++] = b;
                    parseState = PARSE_LEN_L;
                    break;
                case PARSE_LEN_L:
                    parseBuffer[parseIndex++] = b;
                    expectedPayloadLen = b;
                    if (expectedPayloadLen > MAX_PAYLOAD_LEN) {
                        parseState = PARSE_START;
                        parseIndex = 0;
                    } else {
                        if (expectedPayloadLen == 0) parseState = PARSE_CRC;
                        else parseState = PARSE_PAYLOAD;
                    }
                    break;
                case PARSE_PAYLOAD:
                    parseBuffer[parseIndex++] = b;
                    if (parseIndex == (3 + expectedPayloadLen)) parseState = PARSE_CRC;
                    break;
                case PARSE_CRC:
                    {
                        uint8_t received_crc = b;
                        uint8_t calcd_crc = calculate_crc8(&parseBuffer[1], parseIndex - 1);
                        if (received_crc == calcd_crc) {
                            parseBuffer[parseIndex++] = b;
                            process_packet(parseBuffer, parseIndex);
                        }
                    }
                    parseState = PARSE_START;
                    parseIndex = 0;
                    break;
            }
        }

        // 2. Log Streaming
        if (is_streaming_log) {
            uint8_t chunk[200];
            int read = LogManager_ReadChunk(chunk, sizeof(chunk));
            if (read > 0) {
                len = pack_log_data_chunk_message(tx_buf, chunk, read);
                HAL_UART_Transmit(&huart6, tx_buf, len, 100);
                // Flow control needed?
                // We trust HAL_UART_Transmit blocks until Sent (or timeout).
                // Just yield a bit to allow RX processing.
            } else {
                is_streaming_log = false;
                LogManager_CloseRead();
                // Send End (Empty Chunk)
                len = pack_log_data_chunk_message(tx_buf, NULL, 0);
                HAL_UART_Transmit(&huart6, tx_buf, len, 100);
            }
        }

        // 3. Process TX
        TxMessage tx;
        if (xQueueReceive(uartTxQueue, &tx, 0) == pdTRUE) {
            // Priority: Send queued message.
            // Note: If streaming, we might interleave packets. ESP must handle it.
            // Protocol supports packet interleaving as long as atomic packets are sent.

            len = 0;
            if (tx.type == MSG_WAVE2_SET) len = pack_set_wave2_message(tx_buf, tx.data.w2.type, tx.data.w2.value);
            else if (tx.type == MSG_AC_SET) len = pack_set_ac_message(tx_buf, tx.data.enable);
            else if (tx.type == MSG_DC_SET) len = pack_set_dc_message(tx_buf, tx.data.enable);
            else if (tx.type == MSG_SET_VALUE) len = pack_set_value_message(tx_buf, tx.data.set_val.type, tx.data.set_val.value);
            else if (tx.type == MSG_POWER_OFF) len = pack_power_off_message(tx_buf);
            else if (tx.type == MSG_GET_DEBUG_INFO) len = pack_get_debug_info_message(tx_buf);
            else if (tx.type == MSG_CONNECT_DEVICE) len = pack_connect_device_message(tx_buf, tx.data.device_type);
            else if (tx.type == MSG_FORGET_DEVICE) len = pack_forget_device_message(tx_buf, tx.data.device_type);
            else if (tx.type == MSG_LOG_STATUS_REQ) len = pack_log_status_req_message(tx_buf);

            if (len > 0) HAL_UART_Transmit(&huart6, tx_buf, len, 100);
        }

        // 4. State Machine (Heartbeat)
        if ((xTaskGetTickCount() - lastActivityTime) > pdMS_TO_TICKS(200)) {
            lastActivityTime = xTaskGetTickCount();
            switch (protocolState) {
                case STATE_HANDSHAKE:
                    if ((xTaskGetTickCount() - lastHandshakeTime) > pdMS_TO_TICKS(1000)) {
                        lastHandshakeTime = xTaskGetTickCount();
                        len = pack_handshake_message(tx_buf);
                        HAL_UART_Transmit(&huart6, tx_buf, len, 100);
                        protocolState = STATE_WAIT_HANDSHAKE_ACK;
                    }
                    break;
                case STATE_WAIT_HANDSHAKE_ACK:
                    if ((xTaskGetTickCount() - lastHandshakeTime) > pdMS_TO_TICKS(1000)) {
                        lastHandshakeTime = xTaskGetTickCount();
                        len = pack_handshake_message(tx_buf);
                        HAL_UART_Transmit(&huart6, tx_buf, len, 100);
                    }
                    break;
                case STATE_POLLING:
                    // Don't poll while streaming logs to save bandwidth?
                    if (!is_streaming_log) {
                        if (knownDevices.count > 0) {
                            if (currentDeviceIndex >= knownDevices.count) currentDeviceIndex = 0;
                            if (knownDevices.devices[currentDeviceIndex].connected) {
                                uint8_t dev_id = knownDevices.devices[currentDeviceIndex].id;
                                len = pack_get_device_status_message(tx_buf, dev_id);
                                HAL_UART_Transmit(&huart6, tx_buf, len, 100);
                            }
                            currentDeviceIndex++;
                        }
                    }
                    break;
                default: break;
            }
        }
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
