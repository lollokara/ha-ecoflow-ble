/**
 * @file main.c
 * @brief Main application file for the STM32F469IDISCOVERY board.
 *
 * This file contains the primary application logic for the display and UI,
 * including:
 * - Initialization of STM32Cube HAL.
 * - Configuration and handling of UART communication with the ESP32.
 * - A placeholder for the LCD display logic.
 */

#include "stm32f4xx_hal.h"
#include "ecoflow_protocol.h" // Shared library
#include <string.h>
#include <stdio.h>

// Function Prototypes
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void Display_Init(void);
static void Display_Message(const char* msg);
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart);

// UART handle
UART_HandleTypeDef huart1;
#define UART_BUFFER_SIZE 256
uint8_t uart_rx_buffer[UART_BUFFER_SIZE];
uint8_t rx_byte;

// Main application
int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART1_UART_Init();
    Display_Init();

    Display_Message("STM32F4 Initialized");

    // Start listening for UART data from ESP32
    HAL_UART_Receive_IT(&huart1, &rx_byte, 1);

    uint32_t last_request_time = HAL_GetTick();

    while (1) {
        // Periodically request a status update from the ESP32
        if (HAL_GetTick() - last_request_time > 5000) {
            last_request_time = HAL_GetTick();
            uint8_t tx_buffer[4];
            int len = pack_request_status_message(tx_buffer);
            HAL_UART_Transmit(&huart1, tx_buffer, len, 100);
            Display_Message("Sent Status Request");
        }
    }
}

// UART Receive Callback
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    static uint8_t buffer[UART_BUFFER_SIZE];
    static uint8_t index = 0;
    static uint8_t msg_len = 0;

    if (index == 0 && rx_byte != START_BYTE) {
        return; // Wait for start byte
    }

    buffer[index++] = rx_byte;

    if (index == 3) { // We have START, CMD, LEN
        msg_len = buffer[2];
        if (msg_len > MAX_PAYLOAD_LEN) {
            index = 0; // Invalid length
            return;
        }
    }

    if (index > 3 && index == (3 + msg_len + 1)) { // Full message received
        if (buffer[1] == CMD_BATTERY_STATUS) {
            BatteryStatus status;
            if (unpack_battery_status_message(buffer, &status) == 0) {
                char display_str[100];
                sprintf(display_str, "SOC:%u%% Pwr:%dW V:%0.1fV",
                        status.soc, status.power_w, status.voltage_v / 100.0f);
                Display_Message(display_str);
            } else {
                Display_Message("CRC Error!");
            }
        }
        index = 0;
    }

    // Re-arm the interrupt
    HAL_UART_Receive_IT(huart, &rx_byte, 1);
}


// Placeholder for Display functions
static void Display_Init(void) {
    // In a real implementation, this would initialize the LCD.
    // For now, we just print to the console via semihosting or SWO.
    printf("Display Initialized\n");
}

static void Display_Message(const char* msg) {
    // Placeholder to show messages on the LCD.
    printf("DISPLAY: %s\n", msg);
}


// System Clock Configuration
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8;
    RCC_OscInitStruct.PLL.PLLN = 360;
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        // Error_Handler();
    }

    if (HAL_PWREx_EnableOverDrive() != HAL_OK) {
        // Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        // Error_Handler();
    }
}

// UART Initialization
static void MX_USART1_UART_Init(void) {
    huart1.Instance = USART1;
    huart1.Init.BaudRate = 460800;
    huart1.Init.WordLength = UART_WORDLENGTH_8B;
    huart1.Init.StopBits = UART_STOPBITS_1;
    huart1.Init.Parity = UART_PARITY_NONE;
    huart1.Init.Mode = UART_MODE_TX_RX;
    huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart1.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart1) != HAL_OK) {
        // Error_Handler();
    }
}

// GPIO Initialization
static void MX_GPIO_Init(void) {
    // GPIO Ports Clock Enable
    __HAL_RCC_GPIOA_CLK_ENABLE();
}

// HAL MSP Initializations (required by HAL)
void HAL_UART_MspInit(UART_HandleTypeDef* huart) {
    if(huart->Instance==USART1) {
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        __HAL_RCC_USART1_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        // PA9 (TX), PA10 (RX)
        GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_10;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART1;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        // UART interrupt
        HAL_NVIC_SetPriority(USART1_IRQn, 0, 0);
        HAL_NVIC_EnableIRQ(USART1_IRQn);
    }
}

// SysTick Handler for HAL
void SysTick_Handler(void) {
    HAL_IncTick();
}

// USART1 Interrupt Handler
void USART1_IRQHandler(void) {
    HAL_UART_IRQHandler(&huart1);
}

#ifdef  USE_FULL_ASSERT
void assert_failed(uint8_t *file, uint32_t line) {
    // User can add his own implementation to report the file name and line number
}
#endif
