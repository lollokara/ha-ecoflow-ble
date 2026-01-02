#include "stm32f4xx_hal.h"
#include <string.h>

/* Defines */
#define APP_ADDRESS     0x08008000
#define OTA_MAGIC       0xDEADBEEF
#define BKP_DR_OTA      RTC_BKP_DR0

/* UART Definitions */
UART_HandleTypeDef huart6;

/* LED Definitions */
#define LED_GREEN_PIN   GPIO_PIN_6
#define LED_GREEN_PORT  GPIOG
#define LED_ORANGE_PIN  GPIO_PIN_4
#define LED_ORANGE_PORT GPIOD
#define LED_RED_PIN     GPIO_PIN_5
#define LED_RED_PORT    GPIOD
#define LED_BLUE_PIN    GPIO_PIN_3
#define LED_BLUE_PORT   GPIOK

/* LED States - Active High (Standard Discovery) */
#define LED_PIN_ON      GPIO_PIN_SET
#define LED_PIN_OFF     GPIO_PIN_RESET

/* Protocol Definitions */
#define CMD_OTA_START   0xF0
#define CMD_OTA_DATA    0xF1
#define CMD_OTA_END     0xF2
#define CMD_ACK         0x06
#define CMD_NACK        0x15

/* Function Prototypes */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART6_UART_Init(void);
void Error_Handler(void);
void JumpToApplication(void);
void EnterOTAMode(void);
void BlinkLED(GPIO_TypeDef* port, uint16_t pin, int count, int delay);
uint8_t calculate_crc8(uint8_t *data, int len);

/* Global Variables */
RTC_HandleTypeDef hrtc;

int main(void) {
    /* MCU Configuration */
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init(); // This now handles Clock Enable AND Initial State properly

    /* Startup Visual: Blink Green 3x Fast */
    BlinkLED(LED_GREEN_PORT, LED_GREEN_PIN, 3, 100);

    /* Enable Power Clock and Backup Access */
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    /* Enable RTC Clock (LSI) */
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI;
    RCC_OscInitStruct.LSIState = RCC_LSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        BlinkLED(LED_ORANGE_PORT, LED_ORANGE_PIN, 5, 50);
    }

    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
    HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct);

    __HAL_RCC_RTC_ENABLE();
    hrtc.Instance = RTC;

    /* Check Backup Register for Magic Number */
    uint32_t magic = HAL_RTCEx_BKUPRead(&hrtc, BKP_DR_OTA);

    /* Check User Button (PA0) - Hold High to force OTA */
    int buttonState = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);

    if (magic == OTA_MAGIC || buttonState == GPIO_PIN_SET) {
        /* Clear Magic */
        HAL_RTCEx_BKUPWrite(&hrtc, BKP_DR_OTA, 0);
        EnterOTAMode();
    } else {
        JumpToApplication();
    }

    while (1) {
        // Should not reach here
        HAL_GPIO_TogglePin(LED_RED_PORT, LED_RED_PIN);
        HAL_Delay(200);
    }
}

void JumpToApplication(void) {
    uint32_t appStack = *(__IO uint32_t*)APP_ADDRESS;
    uint32_t appResetHandler = *(__IO uint32_t*)(APP_ADDRESS + 4);

    if ((appStack >= 0x20000000) && (appStack <= 0x20050000)) {
        /* Success Indication: Green On for 500ms */
        HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, LED_PIN_ON);
        HAL_Delay(500);
        HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, LED_PIN_OFF);

        HAL_RCC_DeInit();
        HAL_DeInit();
        SysTick->CTRL = 0;
        __disable_irq();

        void (*pResetHandler)(void) = (void (*)(void))appResetHandler;
        __set_MSP(appStack);
        pResetHandler();
    } else {
        /* Invalid App: Blink Red 3x */
        BlinkLED(LED_RED_PORT, LED_RED_PIN, 3, 200);
        EnterOTAMode();
    }
}

uint8_t calculate_crc8(uint8_t *data, int len) {
    uint8_t crc = 0;
    for (int i = 0; i < len; i++) {
        crc += data[i];
    }
    return crc;
}

void EnterOTAMode(void) {
    MX_USART6_UART_Init();

    // OTA State: Blue LED Blink (Heartbeat)
    // Turn others off
    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, LED_PIN_OFF);
    HAL_GPIO_WritePin(LED_ORANGE_PORT, LED_ORANGE_PIN, LED_PIN_OFF);
    HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, LED_PIN_OFF);

    uint8_t rxBuffer[300];
    uint32_t writeAddr = APP_ADDRESS;
    uint32_t lastTick = HAL_GetTick();

    while (1) {
        // Heartbeat Blue LED
        if (HAL_GetTick() - lastTick > 500) {
            HAL_GPIO_TogglePin(LED_BLUE_PORT, LED_BLUE_PIN);
            lastTick = HAL_GetTick();
        }

        uint8_t byte = 0;
        // Short timeout to allow heartbeat
        if (HAL_UART_Receive(&huart6, &byte, 1, 10) == HAL_OK) {
            if (byte == 0xAA) {
                // Packet Start
                HAL_GPIO_WritePin(LED_BLUE_PORT, LED_BLUE_PIN, LED_PIN_ON); // Solid Blue during packet

                uint8_t header[2];
                if (HAL_UART_Receive(&huart6, header, 2, 100) == HAL_OK) {
                    uint8_t cmd = header[0];
                    uint8_t len = header[1];

                    if (HAL_UART_Receive(&huart6, rxBuffer, len, 500) == HAL_OK) {
                        uint8_t crc_rx = 0;
                        if (HAL_UART_Receive(&huart6, &crc_rx, 1, 100) == HAL_OK) {

                            uint8_t sum = cmd + len;
                            for(int i=0; i<len; i++) sum += rxBuffer[i];

                            if (sum == crc_rx) {
                                if (cmd == CMD_OTA_START) {
                                    // Erase - Turn Orange ON
                                    HAL_GPIO_WritePin(LED_ORANGE_PORT, LED_ORANGE_PIN, LED_PIN_ON);
                                    HAL_FLASH_Unlock();
                                    FLASH_EraseInitTypeDef EraseInitStruct;
                                    uint32_t SectorError;
                                    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
                                    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
                                    EraseInitStruct.Sector = FLASH_SECTOR_2;
                                    EraseInitStruct.NbSectors = 6;

                                    // Clear Flags
                                    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                                                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

                                    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) == HAL_OK) {
                                        uint8_t ack = CMD_ACK;
                                        HAL_UART_Transmit(&huart6, &ack, 1, 100);
                                        writeAddr = APP_ADDRESS;
                                    } else {
                                        uint8_t nack = CMD_NACK;
                                        HAL_UART_Transmit(&huart6, &nack, 1, 100);
                                        BlinkLED(LED_RED_PORT, LED_RED_PIN, 5, 50);
                                    }
                                    HAL_FLASH_Lock();
                                    HAL_GPIO_WritePin(LED_ORANGE_PORT, LED_ORANGE_PIN, LED_PIN_OFF);
                                }
                                else if (cmd == CMD_OTA_DATA) {
                                    // Write - Toggle Green
                                    HAL_GPIO_TogglePin(LED_GREEN_PORT, LED_GREEN_PIN);
                                    HAL_FLASH_Unlock();
                                    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                                                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

                                    for (int i = 0; i < len; i++) {
                                        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, writeAddr++, rxBuffer[i]) != HAL_OK) {
                                            // Write Error - Orange Pulse
                                            HAL_GPIO_WritePin(LED_ORANGE_PORT, LED_ORANGE_PIN, LED_PIN_ON);
                                        }
                                    }
                                    HAL_FLASH_Lock();
                                    HAL_GPIO_WritePin(LED_ORANGE_PORT, LED_ORANGE_PIN, LED_PIN_OFF);

                                    uint8_t ack = CMD_ACK;
                                    HAL_UART_Transmit(&huart6, &ack, 1, 100);
                                }
                                else if (cmd == CMD_OTA_END) {
                                    uint8_t ack = CMD_ACK;
                                    HAL_UART_Transmit(&huart6, &ack, 1, 100);
                                    HAL_Delay(200);
                                    HAL_NVIC_SystemReset();
                                }
                            } else {
                                uint8_t nack = CMD_NACK;
                                HAL_UART_Transmit(&huart6, &nack, 1, 100);
                            }
                        }
                    }
                }
            }
        }
    }
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
    RCC_OscInitStruct.PLL.PLLR = 2;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    if (HAL_PWREx_EnableOverDrive() != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_USART6_UART_Init(void) {
    huart6.Instance = USART6;
    huart6.Init.BaudRate = 115200;
    huart6.Init.WordLength = UART_WORDLENGTH_8B;
    huart6.Init.StopBits = UART_STOPBITS_1;
    huart6.Init.Parity = UART_PARITY_NONE;
    huart6.Init.Mode = UART_MODE_TX_RX;
    huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart6.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart6) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOK_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();

    /* 1. Set OFF State (High) BEFORE Config to avoid glitching Low */
    HAL_GPIO_WritePin(GPIOG, LED_GREEN_PIN, LED_PIN_OFF);
    HAL_GPIO_WritePin(GPIOD, LED_ORANGE_PIN|LED_RED_PIN, LED_PIN_OFF);
    HAL_GPIO_WritePin(GPIOK, LED_BLUE_PIN, LED_PIN_OFF);

    /* 2. Configure */
    GPIO_InitStruct.Pin = LED_GREEN_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LED_ORANGE_PIN|LED_RED_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LED_BLUE_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOK, &GPIO_InitStruct);

    /* Button */
    GPIO_InitStruct.Pin = GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
}

void Error_Handler(void) {
    __disable_irq();
    while (1) {
        HAL_GPIO_TogglePin(LED_RED_PORT, LED_RED_PIN);
        HAL_Delay(100);
    }
}

void BlinkLED(GPIO_TypeDef* port, uint16_t pin, int count, int delay) {
    for(int i=0; i<count; i++) {
        HAL_GPIO_WritePin(port, pin, LED_PIN_ON);
        HAL_Delay(delay);
        HAL_GPIO_WritePin(port, pin, LED_PIN_OFF);
        HAL_Delay(delay);
    }
}
