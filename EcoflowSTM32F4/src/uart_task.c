#include "uart_task.h"
#include "protocol.h"
#include "display_task.h"
#include "stm32f4xx_hal.h"
#include <string.h>

UART_HandleTypeDef huart1;
#define RX_BUFFER_SIZE 256
uint8_t rx_byte;
uint8_t rx_buffer[RX_BUFFER_SIZE];
uint8_t rx_index = 0;
uint8_t rx_msg_len = 0;

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance != USART1) return;

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
        // Format: START(1) CMD(1) LEN(1) PAYLOAD(N) CRC(1)
        // Total = 3 + N + 1 = 4 + N
        if (rx_index > 3 && rx_index == (4 + rx_msg_len)) {
            // Verify CRC
            uint8_t received_crc = rx_buffer[rx_index - 1];
            uint8_t calcd_crc = calculate_crc8(rx_buffer, rx_index - 1);

            if (received_crc == calcd_crc) {
                // Packet Valid, dispatch event
                uint8_t cmd = rx_buffer[1];
                uint8_t *payload = &rx_buffer[3];

                DisplayEvent event;
                if (cmd == CMD_BATTERY_STATUS) {
                    event.type = DISPLAY_EVENT_UPDATE_BATTERY;

                    // Direct cast assumes little-endian and packed struct matching.
                    // For safety in production, should unpack byte-by-byte.
                    // Given the constraint of time, and same architecture (ARM LE),
                    // we assume alignment is handled or packed.
                    // Actually, BatteryStatus struct might have padding.
                    // protocol.h struct:
                    // uint8_t soc;
                    // int16_t power_w; (alignment 2) -> 1 byte padding after soc?
                    // uint16_t voltage_v;
                    // uint8_t connected;

                    // We should unpack manually to be safe.
                    event.data.battery.soc = payload[0];
                    // power_w at offset 1, 2 bytes (Little Endian)
                    event.data.battery.power_w = (int16_t)(payload[1] | (payload[2] << 8));
                    // voltage_v at offset 3, 2 bytes
                    event.data.battery.voltage_v = (uint16_t)(payload[3] | (payload[4] << 8));
                    // connected at offset 5
                    event.data.battery.connected = payload[5];

                    xQueueSendFromISR(displayQueue, &event, NULL);
                }
            }
            rx_index = 0;
        }
    }

    // Re-arm interrupt
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
}

static void UART_Init(void) {
    __HAL_RCC_USART1_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    // PA9 (TX), PA10 (RX)
    GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    huart1.Instance = USART1;
    huart1.Init.BaudRate = 460800;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart1);

    HAL_NVIC_SetPriority(USART1_IRQn, 5, 0); // Lower priority than FreeRTOS Max Syscall (5)
    HAL_NVIC_EnableIRQ(USART1_IRQn);

    // Start RX
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);
}

void StartUARTTask(void * argument) {
    UART_Init();

    for(;;) {
        // Send Status Request every 5 seconds
        // Packet: START(AA) CMD(10) LEN(00) CRC(X)
        uint8_t tx_buf[4];
        tx_buf[0] = START_BYTE;
        tx_buf[1] = CMD_REQUEST_STATUS;
        tx_buf[2] = 0;
        tx_buf[3] = calculate_crc8(tx_buf, 3);

        HAL_UART_Transmit(&huart1, tx_buf, 4, 100);

        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
