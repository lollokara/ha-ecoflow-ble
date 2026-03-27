/**
 * @file main.c
 * @author Lollokara
 * @brief Simple Demo for BrainTransplantF4 (STM32F469I-Discovery)
 */

#ifndef HSE_VALUE
#define HSE_VALUE 8000000
#endif

#include "stm32f4xx_hal.h"
#include <stdio.h>
#include <string.h>

// UART Protocol Constants
#define START_BYTE 0xAA
#define CMD_OTA_START 0xA0
#define CMD_OTA_NACK 0x15

UART_HandleTypeDef huart3; // Debug
UART_HandleTypeDef huart6; // OTA / Control
IWDG_HandleTypeDef hiwdg;  // Watchdog

void SystemClock_Config(void);
static void MX_USART3_UART_Init(void);
static void MX_USART6_UART_Init(void);
static void MX_IWDG_Init(void);
void EnterBootloader(void);

// Buffer for OTA Command Detection
uint8_t rx_byte;

int main(void) {
    // Relocate Vector Table to Application Address
    SCB->VTOR = 0x08008000;
    __DSB();

    HAL_Init();
    SystemClock_Config();
    MX_USART3_UART_Init();
    MX_USART6_UART_Init();
    MX_IWDG_Init(); // Must be initialized if bootloader enabled it (or to prevent reset)

    printf("BrainTransplantF4 Demo Initialized.\r\n");

    // Start listening for OTA commands on UART6
    HAL_UART_Receive_IT(&huart6, &rx_byte, 1);

    while (1) {
        printf("Hello World\r\n");
        HAL_Delay(1000);
        HAL_IWDG_Refresh(&hiwdg); // Refresh Watchdog
    }
}

// UART RX Complete Callback
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    static uint8_t state = 0;
    if (huart->Instance == USART6) {
        // Simple State Machine to detect [START_BYTE][CMD_OTA_START]
        // ESP sends: [START][CMD][LEN][PAYLOAD][CRC]
        // We only need to detect START + CMD_OTA_START to trigger reset.

        if (state == 0) {
            if (rx_byte == START_BYTE) state = 1;
        } else if (state == 1) {
            if (rx_byte == CMD_OTA_START) {
                // Detected OTA Start Request!
                EnterBootloader();
            } else {
                state = 0; // Reset
            }
        }

        // Keep listening
        HAL_UART_Receive_IT(&huart6, &rx_byte, 1);
    }
}

void EnterBootloader(void) {
    printf("OTA Request Received. Entering Bootloader...\r\n");

    // 1. Send NACK to ESP32 (forces it to retry, giving us time to reboot)
    uint8_t nack[] = {START_BYTE, CMD_OTA_NACK, 0, 0}; // Simplified NACK
    // Calculate CRC8 for NACK (Cmd=0x15, Len=0) -> CRC=0x31 (if seeded 0)
    // Using dummy CRC for now or calculated if strict.
    // Bootloader calculates CRC8(0x15, 0x00) -> 0x15 ^ 0 -> 0x15. Loop 8 times.
    // Let's just send bytes. ESP expects 4 bytes + CRC?
    // Protocol: [START][CMD][LEN][PAYLOAD][CRC]
    // NACK Packet: AA 15 00 CRC
    // CRC8(15 00) = 0xE1 (Example calculation)
    // Actually, ESP OTA Logic just checks for CMD_OTA_NACK byte in stream or packet.
    HAL_UART_Transmit(&huart6, nack, 3, 100);

    // 2. Set Backup Flag
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    RTC->BKP0R = 0xDEADBEEF;

    // 3. Reset
    HAL_NVIC_SystemReset();
}

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
    HAL_RCC_OscConfig(&RCC_OscInitStruct);
    HAL_PWREx_EnableOverDrive();

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);
}

static void MX_USART3_UART_Init(void) {
    huart3.Instance = USART3;
    huart3.Init.BaudRate = 115200;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart3);
}

static void MX_USART6_UART_Init(void) {
    huart6.Instance = USART6;
    huart6.Init.BaudRate = 921600; // Must match ESP32 & Bootloader
    huart6.Init.WordLength = UART_WORDLENGTH_8B;
    huart6.Init.StopBits = UART_STOPBITS_1;
    huart6.Init.Parity = UART_PARITY_NONE;
    huart6.Init.Mode = UART_MODE_TX_RX;
    huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart6.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart6);
}

static void MX_IWDG_Init(void) {
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_256;
    hiwdg.Init.Reload = 1250; // 10s
    if (HAL_IWDG_Init(&hiwdg) != HAL_OK) {
        // Error
    }
}

void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if(uartHandle->Instance==USART3) {
        __HAL_RCC_USART3_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
        GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    } else if(uartHandle->Instance==USART6) {
        __HAL_RCC_USART6_CLK_ENABLE();
        __HAL_RCC_GPIOG_CLK_ENABLE();
        GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_14;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF8_USART6;
        HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

        // Enable Interrupt for UART6
        HAL_NVIC_SetPriority(USART6_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(USART6_IRQn);
    }
}
