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
    MX_GPIO_Init();

    /* 1. LEDs Init: Force All OFF */
    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_ORANGE_PORT, LED_ORANGE_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_BLUE_PORT, LED_BLUE_PIN, GPIO_PIN_RESET);

    /* 2. Startup Visual: Blink Blue 2x */
    BlinkLED(LED_BLUE_PORT, LED_BLUE_PIN, 2, 200);

    /* Enable Power Clock and Backup Access */
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    /* Enable RTC Clock (LSI) */
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI;
    RCC_OscInitStruct.LSIState = RCC_LSI_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_NONE;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        // Clock Error: Blink Orange
        BlinkLED(LED_ORANGE_PORT, LED_ORANGE_PIN, 5, 100);
    }

    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_RTC;
    PeriphClkInitStruct.RTCClockSelection = RCC_RTCCLKSOURCE_LSI;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
        // Periph Error
    }
    __HAL_RCC_RTC_ENABLE();

    hrtc.Instance = RTC; // Needed for HAL_RTCEx calls

    /* Check Backup Register for Magic Number */
    uint32_t magic = HAL_RTCEx_BKUPRead(&hrtc, BKP_DR_OTA);

    /* Check User Button (PA0) - Hold High to force OTA */
    int buttonState = HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);

    if (magic == OTA_MAGIC || buttonState == GPIO_PIN_SET) {
        /* Clear Magic */
        HAL_RTCEx_BKUPWrite(&hrtc, BKP_DR_OTA, 0);

        /* Enter OTA */
        EnterOTAMode();
    } else {
        /* Jump to App */
        JumpToApplication();
    }

    /* Should not reach here */
    while (1) {
        // Fatal Logic Error: Blink Red Slow
        HAL_GPIO_TogglePin(LED_RED_PORT, LED_RED_PIN);
        HAL_Delay(500);
    }
}

void JumpToApplication(void) {
    uint32_t appStack = *(__IO uint32_t*)APP_ADDRESS;
    uint32_t appResetHandler = *(__IO uint32_t*)(APP_ADDRESS + 4);

    /* Validate Stack Pointer (RAM range 0x20000000 - 0x20050000) */
    if ((appStack >= 0x20000000) && (appStack <= 0x20050000)) {
        /* Visual: Green Pulse */
        HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_SET);
        HAL_Delay(200);
        HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_RESET);

        /* De-init Peripherals */
        HAL_RCC_DeInit();
        HAL_DeInit();
        SysTick->CTRL = 0;

        /* Disable Interrupts */
        __disable_irq();

        /* Jump */
        void (*pResetHandler)(void) = (void (*)(void))appResetHandler;
        __set_MSP(appStack);
        pResetHandler();
    } else {
        /* Invalid App */
        // Blink Red 3x then Enter OTA
        BlinkLED(LED_RED_PORT, LED_RED_PIN, 3, 200);
        EnterOTAMode();
    }
}

/* Flash Helper Functions */
uint32_t GetSector(uint32_t Address) {
    if((Address >= 0x08000000) && (Address < 0x08004000)) return FLASH_SECTOR_0;
    if((Address >= 0x08004000) && (Address < 0x08008000)) return FLASH_SECTOR_1;
    if((Address >= 0x08008000) && (Address < 0x0800C000)) return FLASH_SECTOR_2;
    if((Address >= 0x0800C000) && (Address < 0x08010000)) return FLASH_SECTOR_3;
    if((Address >= 0x08010000) && (Address < 0x08020000)) return FLASH_SECTOR_4;
    if((Address >= 0x08020000) && (Address < 0x08040000)) return FLASH_SECTOR_5;
    if((Address >= 0x08040000) && (Address < 0x08060000)) return FLASH_SECTOR_6;
    if((Address >= 0x08060000) && (Address < 0x08080000)) return FLASH_SECTOR_7;
    // ...
    return FLASH_SECTOR_7;
}

uint8_t calculate_crc8(uint8_t *data, int len) {
    uint8_t crc = 0;
    for (int i = 0; i < len; i++) {
        crc += data[i]; // Simple Checksum for now to match ESP32 OtaManager.cpp implementation
                        // (ESP32 code: for(int i=1; i < 3 + readLen; i++) sum += buf[i];)
                        // Note: ESP32 calc includes CMD, LEN, PAYLOAD.
                        // My buffer passed here should be that part.
    }
    return crc;
}

void EnterOTAMode(void) {
    MX_USART6_UART_Init();

    /* Indicate OTA Mode: Blue Constant ON */
    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_ORANGE_PORT, LED_ORANGE_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_BLUE_PORT, LED_BLUE_PIN, GPIO_PIN_SET);

    uint8_t rxBuffer[300];
    uint32_t writeAddr = APP_ADDRESS;

    while (1) {
        /* Poll for Start Byte (0xAA) */
        uint8_t byte = 0;
        if (HAL_UART_Receive(&huart6, &byte, 1, 100) == HAL_OK) {
            if (byte == 0xAA) {
                // Header: [CMD][LEN]
                uint8_t header[2];
                if (HAL_UART_Receive(&huart6, header, 2, 100) == HAL_OK) {
                    uint8_t cmd = header[0];
                    uint8_t len = header[1];

                    // Payload
                    if (HAL_UART_Receive(&huart6, rxBuffer, len, 500) == HAL_OK) {
                        // CRC
                        uint8_t crc_rx = 0;
                        if (HAL_UART_Receive(&huart6, &crc_rx, 1, 100) == HAL_OK) {

                            // Validate Checksum (Sum of CMD + LEN + PAYLOAD)
                            uint8_t sum = cmd + len;
                            for(int i=0; i<len; i++) sum += rxBuffer[i];

                            if (sum == crc_rx) {
                                // Packet OK
                                if (cmd == CMD_OTA_START) {
                                    // Erase
                                    HAL_GPIO_WritePin(LED_ORANGE_PORT, LED_ORANGE_PIN, GPIO_PIN_SET);

                                    HAL_FLASH_Unlock();
                                    FLASH_EraseInitTypeDef EraseInitStruct;
                                    uint32_t SectorError;
                                    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
                                    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
                                    EraseInitStruct.Sector = FLASH_SECTOR_2;
                                    EraseInitStruct.NbSectors = 6; // Erase 2,3,4,5,6,7 (up to 0x08080000?)
                                    // Sector 2: 16KB
                                    // Sector 3: 16KB
                                    // Sector 4: 64KB
                                    // Sector 5: 128KB
                                    // Sector 6: 128KB
                                    // Sector 7: 128KB
                                    // Total: ~480KB covered. If app is larger, need more sectors.
                                    // Assuming app fits in first 1MB bank mostly.

                                    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) == HAL_OK) {
                                        uint8_t ack = CMD_ACK;
                                        HAL_UART_Transmit(&huart6, &ack, 1, 100);
                                        writeAddr = APP_ADDRESS;
                                    } else {
                                        uint8_t nack = CMD_NACK;
                                        HAL_UART_Transmit(&huart6, &nack, 1, 100);
                                        BlinkLED(LED_RED_PORT, LED_RED_PIN, 2, 100);
                                    }
                                    HAL_FLASH_Lock();
                                    HAL_GPIO_WritePin(LED_ORANGE_PORT, LED_ORANGE_PIN, GPIO_PIN_RESET);
                                }
                                else if (cmd == CMD_OTA_DATA) {
                                    // Write
                                    HAL_GPIO_TogglePin(LED_GREEN_PORT, LED_GREEN_PIN);
                                    HAL_FLASH_Unlock();
                                    for (int i = 0; i < len; i++) {
                                        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, writeAddr++, rxBuffer[i]) != HAL_OK) {
                                            // Write Error
                                        }
                                    }
                                    HAL_FLASH_Lock();
                                    uint8_t ack = CMD_ACK;
                                    HAL_UART_Transmit(&huart6, &ack, 1, 100);
                                }
                                else if (cmd == CMD_OTA_END) {
                                    uint8_t ack = CMD_ACK;
                                    HAL_UART_Transmit(&huart6, &ack, 1, 100);
                                    HAL_Delay(500);
                                    HAL_NVIC_SystemReset();
                                }
                            } else {
                                // CRC Fail
                                uint8_t nack = CMD_NACK;
                                HAL_UART_Transmit(&huart6, &nack, 1, 100);
                                BlinkLED(LED_RED_PORT, LED_RED_PIN, 1, 50);
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

    /* Configure LEDs */
    HAL_GPIO_WritePin(GPIOG, LED_GREEN_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOD, LED_ORANGE_PIN|LED_RED_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOK, LED_BLUE_PIN, GPIO_PIN_RESET);

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
        HAL_GPIO_WritePin(port, pin, GPIO_PIN_SET);
        HAL_Delay(delay);
        HAL_GPIO_WritePin(port, pin, GPIO_PIN_RESET);
        HAL_Delay(delay);
    }
}
