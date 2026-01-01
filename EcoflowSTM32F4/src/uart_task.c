#include "uart_task.h"
#include "ecoflow_protocol.h"
#include "display_task.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdio.h>
#include "queue.h"
#include "flash_ops.h"

UART_HandleTypeDef huart6;
extern IWDG_HandleTypeDef hiwdg; // External Watchdog handle from main.c

// Ring Buffer Implementation
#define RING_BUFFER_SIZE 1024
typedef struct {
    uint8_t buffer[RING_BUFFER_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
} RingBuffer;

static RingBuffer rx_ring_buffer;

// TX Queue for sending commands from UI Task to UART Task
// Simple generic struct for TX messages
typedef enum {
    MSG_WAVE2_SET,
    MSG_AC_SET,
    MSG_DC_SET,
    MSG_SET_VALUE,
    MSG_POWER_OFF,
    MSG_GET_DEBUG_INFO,
    MSG_CONNECT_DEVICE,
    MSG_FORGET_DEVICE
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
    // Else: Buffer overflow, drop byte
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
    STATE_POLLING,
    STATE_OTA
} ProtocolState;

static ProtocolState protocolState = STATE_HANDSHAKE;
static DeviceList knownDevices = {0};
static uint8_t currentDeviceIndex = 0;

// OTA State
static uint32_t ota_size = 0;
static uint32_t ota_base_addr = 0x08100000; // Bank 2

// Packet Parsing State
typedef enum {
    PARSE_START,
    PARSE_CMD,
    PARSE_LEN,
    PARSE_PAYLOAD,
    PARSE_CRC
} ParseState;

static ParseState parseState = PARSE_START;
static uint8_t parseBuffer[MAX_PAYLOAD_LEN + 10 + sizeof(DeviceStatus)]; // Ensure buffer is large enough for big payloads
static uint16_t parseIndex = 0; // Payload can be > 255
static uint8_t expectedPayloadLen = 0;

// Moved HAL_UART_RxCpltCallback to stm32f4xx_it.c or handled globally

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
    huart6.Init.BaudRate = 115200;
    huart6.Init.WordLength = UART_WORDLENGTH_8B;
    huart6.Init.StopBits = UART_STOPBITS_1;
    huart6.Init.Parity = UART_PARITY_NONE;
    huart6.Init.Mode = UART_MODE_TX_RX;
    huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart6.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart6);

    HAL_NVIC_SetPriority(USART6_IRQn, 5, 0);
    HAL_NVIC_EnableIRQ(USART6_IRQn);

    // Initialize Ring Buffer
    rb_init(&rx_ring_buffer);

    // Start Reception
    HAL_UART_Receive_IT(&huart6, &rx_byte_isr, 1);
}

static void process_packet(uint8_t *packet, uint16_t total_len) {
    // packet[0] is START, packet[1] is CMD, packet[2] is LEN
    uint8_t cmd = packet[1];

    if (cmd == CMD_HANDSHAKE_ACK) {
        if (protocolState == STATE_WAIT_HANDSHAKE_ACK) {
            printf("UART: Handshake ACK received. State -> WAIT_DEVICE_LIST\n");
            protocolState = STATE_WAIT_DEVICE_LIST;
        }
    }
    else if (cmd == CMD_DEVICE_LIST) {
        if (protocolState == STATE_WAIT_DEVICE_LIST || protocolState == STATE_POLLING) {
            printf("UART: Device List received.\n");
            unpack_device_list_message(packet, &knownDevices);

            // Notify UI
            DisplayEvent event;
            event.type = DISPLAY_EVENT_UPDATE_DEVICE_LIST;
            memcpy(&event.data.deviceList, &knownDevices, sizeof(DeviceList));
            xQueueSend(displayQueue, &event, 0);

            // Send Ack
            uint8_t ack[4];
            int len = pack_device_list_ack_message(ack);
            HAL_UART_Transmit(&huart6, ack, len, 100);

            protocolState = STATE_POLLING;
        }
    }
    else if (cmd == CMD_DEVICE_STATUS) {
        // printf("UART: Parsing Status...\n");
        DeviceStatus status;
        if (unpack_device_status_message(packet, &status) == 0) {
             DisplayEvent event;
             event.type = DISPLAY_EVENT_UPDATE_BATTERY;
             // Pass the full structure, including ID
             memcpy(&event.data.deviceStatus, &status, sizeof(DeviceStatus));

             if(xQueueSend(displayQueue, &event, 0) == pdTRUE) {
                 // printf("UART: Status Queued\n");
             } else {
                 printf("UART: Queue Full!\n");
             }
        } else {
            printf("UART: Unpack Failed\n");
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
    else if (cmd == CMD_OTA_START) {
        // [Len 4B][CRC 4B]
        uint32_t size;
        memcpy(&size, &packet[3], 4);

        ota_size = size;
        // Verify size limits (2MB max)
        if (ota_size > 0x100000) { // Max 1MB
            uint8_t nack[] = {START_BYTE, CMD_OTA_NACK, 0, 0};
            nack[3] = calculate_crc8(&nack[1], 2);
            HAL_UART_Transmit(&huart6, nack, 4, 100);
            return;
        }

        protocolState = STATE_OTA;

        // Unlock Flash
        Flash_Unlock();

        // Mass Erase Bank 2? Or Sector by Sector?
        // Let's erase Sector 12 (0x08100000) for now, or just handle it in chunk write?
        // Better to erase fully here or on demand.
        // Erasing 1MB takes time. We will erase sector by sector as needed or do it here blocking.
        // Let's assume we erase first 4 sectors (16+16+16+64 = 112KB) + others.
        // This is slow. We should send ACK first?

        printf("UART: OTA Start. Size: %lu\n", ota_size);

        uint8_t ack[] = {START_BYTE, CMD_OTA_ACK, 0, 0};
        ack[3] = calculate_crc8(&ack[1], 2);
        HAL_UART_Transmit(&huart6, ack, 4, 100);

        // Notify UI to show "Updating..."
        DisplayEvent event;
        event.type = DISPLAY_EVENT_UPDATE_DEBUG; // Abuse debug event or add new
        xQueueSend(displayQueue, &event, 0);
    }
    else if (cmd == CMD_OTA_CHUNK) {
        // [Offset 4B][Data N]
        uint32_t offset;
        memcpy(&offset, &packet[3], 4);
        uint8_t data_len = packet[2] - 4;
        uint8_t *data = &packet[7];

        // Determine sector address
        uint32_t addr = ota_base_addr + offset;

        // Check if we need to erase this sector?
        // Naive: Erase sector if offset aligns with sector start.
        // Or track erased sectors.
        // Safe bet: Erase sector if (addr % SECTOR_SIZE) == 0? No, sectors vary.
        // Let's assume the Start command erased everything or we check if dirty.
        // For robustness, we should erase.
        // But erasing takes time (secs).
        // Let's assume we ERASED everything in START_OTA blocking (bad for UART).

        // Better: Erase the specific sector if it's the first write to it.
        // Since we write sequentially, we can track "last erased address".
        static uint32_t last_erased_sector_addr = 0;

        // Simplification: We assume START command does the erase of the needed range.
        // But doing it blocking inside ISR/Task might drop bytes.
        // Since we are in OTA state, we stop polling.

        // Actually, let's just write. We assume FLASH is erased.
        // If we fail to write, we NACK.

        // But we MUST erase. Let's do it in START for simplicity, even if it blocks for 2-3s.
        // ESP32 waits 5s for ACK.

        if (offset == 0) {
             printf("UART: OTA First Chunk. Erasing Flash...\n");
             // First chunk, ensure unlocked.
             Flash_Unlock();

             // Smart Erase Loop covering ota_size
             uint32_t current = 0x08100000;
             uint32_t end = current + ota_size;

             // Determine start sector
             // Iterate through 2MB space (Bank 2 starts at sector 12)
             // We know Bank 2 sectors: 12-15 (16K), 16 (64K), 17-23 (128K)
             // Simplified loop: Erase 128KB chunks? No, strict sectors.

             // Brute force sectors 12 to 23 checking overlap
             // Sector 12: 0x08100000 - 0x08104000 (16K)
             // Sector 13: 0x08104000 - 0x08108000 (16K)
             // Sector 14: 0x08108000 - 0x0810C000 (16K)
             // Sector 15: 0x0810C000 - 0x08110000 (16K)
             // Sector 16: 0x08110000 - 0x08120000 (64K)
             // Sector 17: 0x08120000 - 0x08140000 (128K)
             // ...

             // For robustness against IWDG, we refresh inside this loop if we can iterate sectors.
             // Since Flash_EraseSector takes address, let's call it for the start address of each sector.
             // If we just loop by 16KB increments, it's safe (Flash_EraseSector determines sector from addr).

             while(current < end) {
                 printf("UART: Erasing @ %08lX\n", current);
                 Flash_EraseSector(current);
                 HAL_IWDG_Refresh(&hiwdg); // KICK THE DOG!

                 // Advance current address based on sector size to avoid redundant erase calls
                 // (Though Flash_EraseSector might be idempotent if carefully written,
                 // standard HAL erase returns error if busy or just works).
                 // Safe increment: 4KB (smallest possible sector on some STMs, here 16KB).
                 // Let's increment by 16KB. If we are in a large sector, GetSector returns same sector,
                 // and we erase it again?
                 // HAL_FLASHEx_Erase erases the sector. If we call it again, it erases again (waste of time).
                 // Optimization: Move to next sector boundary.

                 if (current < 0x08110000) current += 0x4000; // 16KB (Sectors 12-15)
                 else if (current < 0x08120000) current += 0x10000; // 64KB (Sector 16)
                 else current += 0x20000; // 128KB (Sectors 17-23)
             }
             printf("UART: Flash Erase Complete.\n");
        }

        // Write
        if (Flash_Write(addr, data, data_len) == 0) {
            uint8_t ack[] = {START_BYTE, CMD_OTA_ACK, 0, 0};
            ack[3] = calculate_crc8(&ack[1], 2);
            HAL_UART_Transmit(&huart6, ack, 4, 100);
        } else {
            printf("UART: Flash Write Error @ %08lX\n", addr);
            uint8_t nack[] = {START_BYTE, CMD_OTA_NACK, 0, 0};
            nack[3] = calculate_crc8(&nack[1], 2);
            HAL_UART_Transmit(&huart6, nack, 4, 100);
        }
    }
    else if (cmd == CMD_OTA_END) {
        printf("UART: OTA End received. Locking Flash.\n");
        Flash_Lock();
        // Send final ACK
        uint8_t ack[] = {START_BYTE, CMD_OTA_ACK, 0, 0};
        ack[3] = calculate_crc8(&ack[1], 2);
        HAL_UART_Transmit(&huart6, ack, 4, 100);
    }
    else if (cmd == CMD_OTA_APPLY) {
        printf("UART: OTA Apply. Swapping Bank and Resetting...\n");
        HAL_Delay(100); // Wait for UART to flush
        Flash_SwapBank();
    }
}

// Public function to queue a Wave 2 command
void UART_SendWave2Set(Wave2SetMsg *msg) {
    if (uartTxQueue) {
        TxMessage tx;
        tx.type = MSG_WAVE2_SET;
        tx.data.w2 = *msg;
        xQueueSend(uartTxQueue, &tx, 0);
    }
}

void UART_SendPowerOff(void) {
    if (uartTxQueue) {
        TxMessage tx;
        tx.type = MSG_POWER_OFF;
        xQueueSend(uartTxQueue, &tx, 0);
    }
}

void UART_SendACSet(uint8_t enable) {
    if (uartTxQueue) {
        TxMessage tx;
        tx.type = MSG_AC_SET;
        tx.data.enable = enable;
        xQueueSend(uartTxQueue, &tx, 0);
    }
}

void UART_SendDCSet(uint8_t enable) {
    if (uartTxQueue) {
        TxMessage tx;
        tx.type = MSG_DC_SET;
        tx.data.enable = enable;
        xQueueSend(uartTxQueue, &tx, 0);
    }
}

void UART_SendSetValue(uint8_t type, int value) {
    if (uartTxQueue) {
        TxMessage tx;
        tx.type = MSG_SET_VALUE;
        tx.data.set_val.type = type;
        tx.data.set_val.value = value;
        xQueueSend(uartTxQueue, &tx, 0);
    }
}

void UART_SendGetDebugInfo(void) {
    if (uartTxQueue) {
        TxMessage tx;
        tx.type = MSG_GET_DEBUG_INFO;
        xQueueSend(uartTxQueue, &tx, 0);
    }
}

void UART_SendConnectDevice(uint8_t type) {
    if (uartTxQueue) {
        TxMessage tx;
        tx.type = MSG_CONNECT_DEVICE;
        tx.data.device_type = type;
        xQueueSend(uartTxQueue, &tx, 0);
    }
}

void UART_SendForgetDevice(uint8_t type) {
    if (uartTxQueue) {
        TxMessage tx;
        tx.type = MSG_FORGET_DEVICE;
        tx.data.device_type = type;
        xQueueSend(uartTxQueue, &tx, 0);
    }
}

// Return a copy of known devices for UI
void UART_GetKnownDevices(DeviceList *list) {
    // Note: Not thread safe but low risk for read-only UI display
    // Ideally use a mutex if critical
    memcpy(list, &knownDevices, sizeof(DeviceList));
}

void StartUARTTask(void * argument) {
    UART_Init();

    // Create TX Queue
    uartTxQueue = xQueueCreate(10, sizeof(TxMessage));

    uint8_t tx_buf[32];
    int len;
    uint8_t b;
    TickType_t lastActivityTime = xTaskGetTickCount();
    TickType_t lastHandshakeTime = 0; // Non-blocking timer for handshake

    for(;;) {
        // 1. Process Incoming Data
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
                    parseState = PARSE_LEN;
                    break;

                case PARSE_LEN:
                    parseBuffer[parseIndex++] = b;
                    expectedPayloadLen = b;
                    if (expectedPayloadLen > MAX_PAYLOAD_LEN) {
                        parseState = PARSE_START; // Invalid length, reset
                        parseIndex = 0;
                    } else {
                        if (expectedPayloadLen == 0) {
                            parseState = PARSE_CRC;
                        } else {
                            parseState = PARSE_PAYLOAD;
                        }
                    }
                    break;

                case PARSE_PAYLOAD:
                    parseBuffer[parseIndex++] = b;
                    if (parseIndex == (3 + expectedPayloadLen)) {
                        parseState = PARSE_CRC;
                    }
                    break;

                case PARSE_CRC:
                    // Last byte is CRC
                    {
                        uint8_t received_crc = b;
                        uint8_t calcd_crc = calculate_crc8(&parseBuffer[1], parseIndex - 1);

                        if (received_crc == calcd_crc) {
                            parseBuffer[parseIndex++] = b;
                            process_packet(parseBuffer, parseIndex);
                        } else {
                            printf("UART: CRC Fail. Rx=%02X Calc=%02X\n", received_crc, calcd_crc);
                        }
                    }
                    parseState = PARSE_START;
                    parseIndex = 0;
                    break;
            }
        }

        // 2. Check for Outgoing Commands (Priority)
        TxMessage tx;
        if (xQueueReceive(uartTxQueue, &tx, 0) == pdTRUE) {
            uint8_t buf[32];
            int len = 0;
            if (tx.type == MSG_WAVE2_SET) {
                len = pack_set_wave2_message(buf, tx.data.w2.type, tx.data.w2.value);
            } else if (tx.type == MSG_AC_SET) {
                len = pack_set_ac_message(buf, tx.data.enable);
            } else if (tx.type == MSG_DC_SET) {
                len = pack_set_dc_message(buf, tx.data.enable);
            } else if (tx.type == MSG_SET_VALUE) {
                len = pack_set_value_message(buf, tx.data.set_val.type, tx.data.set_val.value);
            } else if (tx.type == MSG_POWER_OFF) {
                len = pack_power_off_message(buf);
            } else if (tx.type == MSG_GET_DEBUG_INFO) {
                len = pack_get_debug_info_message(buf);
            } else if (tx.type == MSG_CONNECT_DEVICE) {
                len = pack_connect_device_message(buf, tx.data.device_type);
            } else if (tx.type == MSG_FORGET_DEVICE) {
                len = pack_forget_device_message(buf, tx.data.device_type);
            }
            if (len > 0) {
                HAL_UART_Transmit(&huart6, buf, len, 100);
            }
        }

        // 3. Handle State Machine (Non-blocking)
        if ((xTaskGetTickCount() - lastActivityTime) > pdMS_TO_TICKS(200)) {
            lastActivityTime = xTaskGetTickCount();

            switch (protocolState) {
                case STATE_HANDSHAKE:
                    // Only send every 1 second
                    if ((xTaskGetTickCount() - lastHandshakeTime) > pdMS_TO_TICKS(1000)) {
                        lastHandshakeTime = xTaskGetTickCount();
                        printf("UART: Sending Handshake...\n");
                        len = pack_handshake_message(tx_buf);
                        HAL_UART_Transmit(&huart6, tx_buf, len, 100);
                        protocolState = STATE_WAIT_HANDSHAKE_ACK;
                    }
                    break;

                case STATE_WAIT_HANDSHAKE_ACK:
                    // Retry every 1 second
                    if ((xTaskGetTickCount() - lastHandshakeTime) > pdMS_TO_TICKS(1000)) {
                        lastHandshakeTime = xTaskGetTickCount();
                        printf("UART: Timeout waiting for ACK, resending handshake...\n");
                        len = pack_handshake_message(tx_buf);
                        HAL_UART_Transmit(&huart6, tx_buf, len, 100);
                    }
                    break;

                case STATE_WAIT_DEVICE_LIST:
                    // Just wait
                     break;

                case STATE_POLLING:
                    if (knownDevices.count > 0) {
                        // Skip if index out of bounds (safety)
                        if (currentDeviceIndex >= knownDevices.count) currentDeviceIndex = 0;

                        // Only poll if connected
                        if (knownDevices.devices[currentDeviceIndex].connected) {
                            uint8_t dev_id = knownDevices.devices[currentDeviceIndex].id;
                            len = pack_get_device_status_message(tx_buf, dev_id);
                            HAL_UART_Transmit(&huart6, tx_buf, len, 100);
                        }

                        currentDeviceIndex++;
                        if (currentDeviceIndex >= knownDevices.count) currentDeviceIndex = 0;
                    }
                    break;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5)); // Yield to allow other tasks to run
    }
}
