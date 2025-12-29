#include "uart_task.h"
#include "ecoflow_protocol.h"
#include "display_task.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdio.h>

UART_HandleTypeDef huart6;
#define RX_BUFFER_SIZE 256
uint8_t rx_byte;
uint8_t rx_buffer[RX_BUFFER_SIZE];
uint8_t rx_index = 0;
uint8_t rx_msg_len = 0;

static uint8_t debug_crc8(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x31;
            } else {
                crc <<= 1;
            }
        }
    }
    return crc;
}

// State management for protocol
typedef enum {
    STATE_HANDSHAKE,
    STATE_WAIT_HANDSHAKE_ACK,
    STATE_WAIT_DEVICE_LIST,
    STATE_POLLING
} UartState;

static UartState currentState = STATE_HANDSHAKE;
static DeviceList knownDevices = {0};
static uint8_t currentDeviceIndex = 0;

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance != USART6) return;

    if (rx_index == 0) {
        if (rx_byte == START_BYTE) {
            rx_buffer[rx_index++] = rx_byte;
        }
    } else {
        rx_buffer[rx_index++] = rx_byte;

        // Parse Length
        if (rx_index == 3) {
            rx_msg_len = rx_buffer[2];
            if (rx_msg_len > MAX_PAYLOAD_LEN) {
                rx_index = 0; // Reset on invalid length
            }
        }

        // Check for complete packet
        if (rx_index > 3 && rx_index == (4 + rx_msg_len)) {
            uint8_t received_crc = rx_buffer[rx_index - 1];
            uint8_t calcd_crc = calculate_crc8(&rx_buffer[1], rx_index - 2);

            if (received_crc == calcd_crc) {
                uint8_t cmd = rx_buffer[1];
                printf("UART: Pkt OK. Cmd: 0x%02X, Len: %d\n", cmd, rx_msg_len);

                if (cmd == CMD_HANDSHAKE_ACK) {
                    if (currentState == STATE_WAIT_HANDSHAKE_ACK) {
                        printf("UART: Handshake ACK received. State -> WAIT_DEVICE_LIST\n");
                        currentState = STATE_WAIT_DEVICE_LIST;
                    }
                }
                else if (cmd == CMD_DEVICE_LIST) {
                    if (currentState == STATE_WAIT_DEVICE_LIST || currentState == STATE_POLLING) {
                        unpack_device_list_message(rx_buffer, &knownDevices);
                        printf("UART: Device List received. Count: %d\n", knownDevices.count);

                        // Send Ack
                        uint8_t ack[4];
                        int len = pack_device_list_ack_message(ack);
                        HAL_UART_Transmit(&huart6, ack, len, 100);

                        currentState = STATE_POLLING;
                    }
                }
                else if (cmd == CMD_DEVICE_STATUS) {
                    DeviceStatus status;
                    if (unpack_device_status_message(rx_buffer, &status) == 0) {
                         printf("UART: Status Update. SOC: %d, Pwr: %d\n", status.status.soc, status.status.power_w);
                         DisplayEvent event;
                         event.type = DISPLAY_EVENT_UPDATE_BATTERY;
                         event.data.battery.soc = status.status.soc;
                         event.data.battery.power_w = status.status.power_w;
                         event.data.battery.voltage_v = status.status.voltage_v;
                         event.data.battery.connected = status.status.connected;

                         xQueueSendFromISR(displayQueue, &event, NULL);
                    }
                }
            } else {
                printf("UART: CRC Fail. Cmd: 0x%02X, Len: %d, Rx: 0x%02X, Calc: 0x%02X\n", rx_buffer[1], rx_msg_len, received_crc, calcd_crc);
            }
            rx_index = 0;
        }
    }

    // Re-arm interrupt
    HAL_UART_Receive_IT(&huart6, &rx_byte, 1);
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

    HAL_UART_Receive_IT(&huart6, &rx_byte, 1);
}

void StartUARTTask(void * argument) {
    UART_Init();

    // Self-Test CRC
    uint8_t test_data[] = {0x21, 0x00};
    uint8_t test_crc = calculate_crc8(test_data, 2);
    printf("UART: Self-Test CRC(21 00) = 0x%02X\n", test_crc);

    // Initial Handshake
    uint8_t tx_buf[32];
    int len;

    for(;;) {
        switch (currentState) {
            case STATE_HANDSHAKE:
                printf("UART: Sending Handshake...\n");
                len = pack_handshake_message(tx_buf);
                HAL_UART_Transmit(&huart6, tx_buf, len, 100);
                currentState = STATE_WAIT_HANDSHAKE_ACK;
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;

            case STATE_WAIT_HANDSHAKE_ACK:
                printf("UART: Waiting for ACK, resending handshake...\n");
                // Retransmit if stuck
                len = pack_handshake_message(tx_buf);
                HAL_UART_Transmit(&huart6, tx_buf, len, 100);
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;

            case STATE_WAIT_DEVICE_LIST:
                // Waiting for list...
                 vTaskDelay(pdMS_TO_TICKS(100));
                 break;

            case STATE_POLLING:
                if (knownDevices.count > 0) {
                    uint8_t dev_id = knownDevices.devices[currentDeviceIndex].id;
                    len = pack_get_device_status_message(tx_buf, dev_id);
                    HAL_UART_Transmit(&huart6, tx_buf, len, 100);

                    currentDeviceIndex++;
                    if (currentDeviceIndex >= knownDevices.count) currentDeviceIndex = 0;
                }
                // Poll every 1 second
                vTaskDelay(pdMS_TO_TICKS(1000));
                break;
        }
    }
}
