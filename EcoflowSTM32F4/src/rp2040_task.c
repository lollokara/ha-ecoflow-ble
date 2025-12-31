#include "rp2040_task.h"
#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "queue.h"
#include <string.h>
#include <stdio.h>

// UART Handles
UART_HandleTypeDef huart2; // TX (PA2)
UART_HandleTypeDef huart4; // RX (PA1)

static RP2040_Status global_status = {0};
static QueueHandle_t txQueue;

typedef struct {
    uint8_t data[20];
    uint8_t len;
} TxPacket;

// Protocol Constants
#define PKT_START 0xAA
#define CMD_SET_CONFIG 0x10
#define CMD_GET_STATUS 0x11
#define CMD_GET_CONFIG 0x12
#define RESP_STATUS    0x20
#define RESP_CONFIG    0x21

static ConfigCallback config_cb = NULL;

void RP2040_SetConfigCallback(ConfigCallback cb) {
    config_cb = cb;
}

void RP2040_UART_Init(void) {
    // Enable Clocks
    __HAL_RCC_USART2_CLK_ENABLE();
    __HAL_RCC_UART4_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // PA2 -> USART2_TX
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // PA1 -> UART4_RX
    GPIO_InitStruct.Pin = GPIO_PIN_1;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF8_UART4;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    // Init USART2 (TX)
    huart2.Instance = USART2;
    huart2.Init.BaudRate = 115200;
    huart2.Init.WordLength = UART_WORDLENGTH_8B;
    huart2.Init.StopBits = UART_STOPBITS_1;
    huart2.Init.Parity = UART_PARITY_NONE;
    huart2.Init.Mode = UART_MODE_TX;
    huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart2.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart2);

    // Init UART4 (RX)
    huart4.Instance = UART4;
    huart4.Init.BaudRate = 115200;
    huart4.Init.WordLength = UART_WORDLENGTH_8B;
    huart4.Init.StopBits = UART_STOPBITS_1;
    huart4.Init.Parity = UART_PARITY_NONE;
    huart4.Init.Mode = UART_MODE_RX;
    huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart4.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart4);
}

void RP2040_SendConfig(uint8_t group, FanGroupConfig* config) {
    if (!config) return;
    TxPacket pkt;
    pkt.data[0] = PKT_START;
    pkt.data[1] = CMD_SET_CONFIG;
    pkt.data[2] = 7; // Len
    pkt.data[3] = group;
    pkt.data[4] = (config->min_speed >> 8) & 0xFF;
    pkt.data[5] = config->min_speed & 0xFF;
    pkt.data[6] = (config->max_speed >> 8) & 0xFF;
    pkt.data[7] = config->max_speed & 0xFF;
    pkt.data[8] = config->start_temp;
    pkt.data[9] = config->max_temp;

    uint8_t crc = 0;
    for(int i=0; i<7; i++) crc += pkt.data[3+i];
    pkt.data[10] = crc;
    pkt.len = 11;

    xQueueSend(txQueue, &pkt, 0);
    printf("[RP2040] TX Config Group %d\n", group);
}

void RP2040_RequestConfig(uint8_t group) {
    TxPacket pkt;
    pkt.data[0] = PKT_START;
    pkt.data[1] = CMD_GET_CONFIG;
    pkt.data[2] = 1; // Len
    pkt.data[3] = group;
    // CRC
    pkt.data[4] = group; // Sum
    pkt.len = 5;
    xQueueSend(txQueue, &pkt, 0);
    printf("[RP2040] TX Request Config Group %d\n", group);
}

RP2040_Status* RP2040_GetStatus(void) {
    return &global_status;
}

bool RP2040_IsConnected(void) {
    return global_status.connected;
}

void StartRP2040Task(void *argument) {
    RP2040_UART_Init();
    txQueue = xQueueCreate(5, sizeof(TxPacket));

    uint8_t rx_buf[1];
    uint8_t pkt_buf[32];
    uint8_t pkt_idx = 0;
    uint8_t payload_len = 0;
    uint32_t last_rx_time = 0;

    while(1) {
        // Handle TX
        TxPacket txPkt;
        if (xQueueReceive(txQueue, &txPkt, 0) == pdTRUE) {
            HAL_UART_Transmit(&huart2, txPkt.data, txPkt.len, 100);
        }

        // Handle RX - Drain buffer
        while (HAL_UART_Receive(&huart4, rx_buf, 1, 0) == HAL_OK) {
            // Un-commented for debug
            printf("[RP2040] RX %02X\n", rx_buf[0]);

            if (pkt_idx == 0) {
                if (rx_buf[0] == PKT_START) {
                    pkt_buf[pkt_idx++] = rx_buf[0];
                }
            } else if (pkt_idx == 1) {
                if (rx_buf[0] == RESP_STATUS || rx_buf[0] == RESP_CONFIG) {
                    pkt_buf[pkt_idx++] = rx_buf[0];
                } else {
                    pkt_idx = 0; // Invalid
                }
            } else if (pkt_idx == 2) {
                payload_len = rx_buf[0];
                pkt_buf[pkt_idx++] = rx_buf[0];
                if (payload_len > 28) pkt_idx = 0; // Bad len
            } else {
                pkt_buf[pkt_idx++] = rx_buf[0];
                if (pkt_idx == 3 + payload_len + 1) { // Header + Payload + CRC
                    // Verify CRC
                    uint8_t calc_crc = 0;
                    for(int i=0; i<payload_len; i++) {
                        calc_crc += pkt_buf[3+i];
                    }
                    if (calc_crc == pkt_buf[pkt_idx-1]) {
                        // Valid Packet
                        uint8_t cmd = pkt_buf[1];
                        uint8_t* p = &pkt_buf[3];
                        // printf("[RP2040] RX Valid Pkt CMD %02X\n", cmd);

                        if (cmd == RESP_STATUS) {
                            // Payload: [T_int] [T_dec] [F1H F1L] ...
                            global_status.temp = p[0] + (p[1] / 100.0f);
                            for(int i=0; i<4; i++) {
                                global_status.rpm[i] = (p[2 + i*2] << 8) | p[3 + i*2];
                            }
                            global_status.last_update = xTaskGetTickCount();
                            global_status.connected = true;
                            last_rx_time = xTaskGetTickCount();
                        }
                        else if (cmd == RESP_CONFIG) {
                            printf("[RP2040] RX Config Group %d\n", p[0]);
                            // Payload: [Group] [MinH] [MinL] [MaxH] [MaxL] [Start] [Max]
                            if (config_cb) {
                                FanGroupConfig cfg;
                                uint8_t g = p[0];
                                cfg.min_speed = (p[1] << 8) | p[2];
                                cfg.max_speed = (p[3] << 8) | p[4];
                                cfg.start_temp = p[5];
                                cfg.max_temp = p[6];
                                config_cb(g, &cfg);
                            }
                        }
                    } else {
                        printf("[RP2040] RX CRC Error Calc:%02X Rx:%02X\n", calc_crc, pkt_buf[pkt_idx-1]);
                    }
                    pkt_idx = 0;
                }
            }
        }

        // Timeout check
        if (xTaskGetTickCount() - last_rx_time > 2000) {
            global_status.connected = false;
        }

        vTaskDelay(5); // Increased polling rate
    }
}
