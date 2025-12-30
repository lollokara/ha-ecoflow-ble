#include "uart_task.h"
#include "ecoflow_protocol.h"
#include "display_task.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdio.h>

UART_HandleTypeDef huart6;

// Ring Buffer Implementation
#define RING_BUFFER_SIZE 1024
typedef struct {
    uint8_t buffer[RING_BUFFER_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
} RingBuffer;

static RingBuffer rx_ring_buffer;

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
    PARSE_LEN,
    PARSE_PAYLOAD,
    PARSE_CRC
} ParseState;

static ParseState parseState = PARSE_START;
static uint8_t parseBuffer[MAX_PAYLOAD_LEN + 10]; // Enough for header + payload + CRC
static uint8_t parseIndex = 0;
static uint8_t expectedPayloadLen = 0;

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART6) {
        rb_push(&rx_ring_buffer, rx_byte_isr);
        HAL_UART_Receive_IT(&huart6, &rx_byte_isr, 1);
    }
}

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

static void process_packet(uint8_t *packet, uint8_t total_len) {
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

            // Send Ack
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
             // Pass the full structure, including ID
             memcpy(&event.data.deviceStatus, &status, sizeof(DeviceStatus));

             xQueueSend(displayQueue, &event, 0); // Don't block if full
        }
    }
}

void StartUARTTask(void * argument) {
    UART_Init();

    uint8_t tx_buf[32];
    int len;
    uint8_t b;
    TickType_t lastActivityTime = xTaskGetTickCount();

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
                        // Calculate CRC over CMD, LEN, PAYLOAD
                        // parseBuffer[0] is START
                        // Data starts at parseBuffer[1], length is parseIndex - 1
                        uint8_t calcd_crc = calculate_crc8(&parseBuffer[1], parseIndex - 1);

                        if (received_crc == calcd_crc) {
                            parseBuffer[parseIndex++] = b; // Store CRC byte just in case
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

        // 2. Handle State Machine (Periodic actions)
        // Reduced to 200ms polling for faster updates
        if ((xTaskGetTickCount() - lastActivityTime) > pdMS_TO_TICKS(200)) {
            lastActivityTime = xTaskGetTickCount();

            switch (protocolState) {
                case STATE_HANDSHAKE:
                    printf("UART: Sending Handshake...\n");
                    len = pack_handshake_message(tx_buf);
                    HAL_UART_Transmit(&huart6, tx_buf, len, 100);
                    protocolState = STATE_WAIT_HANDSHAKE_ACK;
                    // Wait 1s for handshake, don't spam it every 200ms
                    vTaskDelay(pdMS_TO_TICKS(800));
                    break;

                case STATE_WAIT_HANDSHAKE_ACK:
                    printf("UART: Timeout waiting for ACK, resending handshake...\n");
                    len = pack_handshake_message(tx_buf);
                    HAL_UART_Transmit(&huart6, tx_buf, len, 100);
                    vTaskDelay(pdMS_TO_TICKS(800));
                    break;

                case STATE_WAIT_DEVICE_LIST:
                    // Just wait
                     break;

                case STATE_POLLING:
                    if (knownDevices.count > 0) {
                        uint8_t dev_id = knownDevices.devices[currentDeviceIndex].id;
                        len = pack_get_device_status_message(tx_buf, dev_id);
                        HAL_UART_Transmit(&huart6, tx_buf, len, 100);

                        currentDeviceIndex++;
                        if (currentDeviceIndex >= knownDevices.count) currentDeviceIndex = 0;
                    }
                    break;
            }
        }

        vTaskDelay(pdMS_TO_TICKS(5)); // Yield to other tasks
    }
}
