#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>

// --- Pin Definitions ---
#define LED_GREEN_PIN       GPIO_PIN_6
#define LED_GREEN_PORT      GPIOG
#define LED_ORANGE_PIN      GPIO_PIN_4
#define LED_ORANGE_PORT     GPIOD
#define LED_RED_PIN         GPIO_PIN_5
#define LED_RED_PORT        GPIOD
#define LED_BLUE_PIN        GPIO_PIN_3
#define LED_BLUE_PORT       GPIOK

// --- Protocol Defines ---
#define CMD_OTA_START   0xA0
#define CMD_OTA_CHUNK   0xA1
#define CMD_OTA_END     0xA2
#define CMD_OTA_APPLY   0xA3
#define CMD_OTA_ACK     0x06
#define CMD_OTA_NACK    0x15

// --- Global Variables ---
UART_HandleTypeDef huart6; // OTA
UART_HandleTypeDef huart3; // Log

// --- Function Prototypes ---
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_USART6_UART_Init(void);
void Log(const char* fmt, ...);
void JumpToApplication(void);
void EnterBootloaderMode(void);
uint8_t CalculateCRC8(uint8_t *data, uint16_t len);
uint32_t CalculateCRC32(uint8_t *data, uint16_t len, uint32_t current_crc);

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART3_UART_Init();

    Log("\n\n--- Ecoflow Bootloader v1.0 ---\n");
    Log("Checking Boot Mode...\n");

    // Enable Backup Access to check Magic Flag
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    __HAL_RCC_RTC_ENABLE(); // Essential for Backup Register Access on some F4s

    // Check Magic Flag (RTC_BKP_DR0)
    // Note: Constants might be RTC_BKP_DR0 or similar depending on HAL version
    uint32_t magic = HAL_RTCEx_BKUPRead(&((RTC_HandleTypeDef){0}), RTC_BKP_DR0);
    bool otaRequest = (magic == 0xDEADBEEF);

    if (otaRequest) {
        Log("OTA Request Detected (Magic Flag).\n");
        // Clear flag
        HAL_RTCEx_BKUPWrite(&((RTC_HandleTypeDef){0}), RTC_BKP_DR0, 0);
    } else {
        Log("Normal Boot.\n");
    }

    // Check Application Validity at 0x08008000 (Sector 2)
    // First word is SP, Second is Reset Vector
    uint32_t appStack = *(__IO uint32_t*)0x08008000;
    uint32_t appEntry = *(__IO uint32_t*)0x08008004;

    bool appValid = (appStack >= 0x20000000 && appStack <= 0x2004FFFF) &&
                    (appEntry >= 0x08008000 && appEntry <= 0x08200000);

    if (appValid && !otaRequest) {
        Log("App Valid. Jumping...\n");
        HAL_Delay(10); // Flush logs
        JumpToApplication();
    } else {
        if (!appValid) Log("App Invalid.\n");
        EnterBootloaderMode();
    }

    while (1);
}

void EnterBootloaderMode(void) {
    Log("Entering OTA Mode. Listening on USART6...\n");
    MX_USART6_UART_Init();

    // Signal Ready (Slow Blue Blink handled in loop, Solid Orange for RX waiting)
    HAL_GPIO_WritePin(LED_ORANGE_PORT, LED_ORANGE_PIN, GPIO_PIN_SET);

    uint8_t rxHeader[16];
    uint8_t rxPayload[1024];
    uint32_t flashWriteOffset = 0x08100000; // Start of Inactive Bank (Logical)
    bool sessionActive = false;
    uint32_t globalCRC = 0xFFFFFFFF;

    // Determine Sectors to Erase
    FLASH_OBProgramInitTypeDef OBInit;
    HAL_FLASHEx_OBGetConfig(&OBInit);
    bool bfb2 = (OBInit.USERConfig & 0x80) != 0; // Bit 23 ? No, in HAL it's different structure
    // We'll read register directly to be sure
    bfb2 = (FLASH->OPTCR & (1 << 23)) != 0;

    Log("Current Bank: %d (BFB2=%d)\n", bfb2 ? 2 : 1, bfb2);
    // If BFB2=0: Running Bank 1. Target Bank 2 (Sectors 12-23).
    // If BFB2=1: Running Bank 2. Target Bank 1 (Sectors 0-11).
    uint32_t startSector = bfb2 ? 0 : 12;

    Log("Target Sector Start: %d\n", startSector);

    while (1) {
        // Blink Blue LED
        static uint32_t lastTick = 0;
        if (HAL_GetTick() - lastTick > 200) {
            HAL_GPIO_TogglePin(LED_BLUE_PORT, LED_BLUE_PIN);
            lastTick = HAL_GetTick();
        }

        uint8_t startByte;
        if (HAL_UART_Receive(&huart6, &startByte, 1, 10) == HAL_OK) {
            if (startByte == 0xAA) { // START_BYTE
                uint8_t header[2]; // CMD, LEN
                if (HAL_UART_Receive(&huart6, header, 2, 100) == HAL_OK) {
                    uint8_t cmd = header[0];
                    uint8_t len = header[1];

                    if (len > sizeof(rxPayload)) len = sizeof(rxPayload);

                    if (HAL_UART_Receive(&huart6, rxPayload, len, 500) == HAL_OK) {
                        uint8_t crc;
                        HAL_UART_Receive(&huart6, &crc, 1, 50);

                        // Verify CRC8 (CMD + LEN + PAYLOAD)
                        uint8_t checkBuf[300]; // Max payload 255 + 2 header
                        checkBuf[0] = cmd;
                        checkBuf[1] = len;
                        memcpy(&checkBuf[2], rxPayload, len);
                        if (CalculateCRC8(checkBuf, len + 2) != crc) {
                            Log("CRC8 Fail. Expected %02X\n", CalculateCRC8(checkBuf, len + 2));
                            uint8_t nack = CMD_OTA_NACK;
                            HAL_UART_Transmit(&huart6, &nack, 1, 100);
                            continue;
                        }

                        if (cmd == CMD_OTA_START) {
                            Log("OTA Start Received.\n");
                            globalCRC = 0xFFFFFFFF; // Reset CRC

                            // Erase Flash (Blocking)
                            HAL_GPIO_WritePin(LED_ORANGE_PORT, LED_ORANGE_PIN, GPIO_PIN_RESET); // Toggle

                            HAL_FLASH_Unlock();
                            __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                                                   FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

                            FLASH_EraseInitTypeDef EraseInitStruct;
                            EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
                            EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
                            EraseInitStruct.Sector = startSector;
                            EraseInitStruct.NbSectors = 12; // Erase Full Bank

                            uint32_t SectorError;
                            Log("Erasing Flash...\n");
                            if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
                                Log("Erase Error: %d\n", SectorError);
                                uint8_t nack = CMD_OTA_NACK;
                                HAL_UART_Transmit(&huart6, &nack, 1, 100);
                            } else {
                                Log("Erase Done.\n");
                                sessionActive = true;
                                flashWriteOffset = 0x08100000;
                                uint8_t ack = CMD_OTA_ACK;
                                HAL_UART_Transmit(&huart6, &ack, 1, 100);
                            }
                            HAL_FLASH_Lock();
                        }
                        else if (cmd == CMD_OTA_CHUNK && sessionActive) {
                            // Update CRC32
                            globalCRC = CalculateCRC32(rxPayload, len, globalCRC);

                            // Pad to 4 bytes for writing
                            if (len % 4 != 0) {
                                uint8_t pad = 4 - (len % 4);
                                memset(rxPayload + len, 0xFF, pad);
                                len += pad;
                            }

                            HAL_FLASH_Unlock();
                            for (int i=0; i<len; i+=4) {
                                uint32_t data = rxPayload[i] | (rxPayload[i+1] << 8) | (rxPayload[i+2] << 16) | (rxPayload[i+3] << 24);
                                HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, flashWriteOffset, data);
                                flashWriteOffset += 4;
                            }
                            HAL_FLASH_Lock();

                            uint8_t ack = CMD_OTA_ACK;
                            HAL_UART_Transmit(&huart6, &ack, 1, 100);
                        }
                        else if (cmd == CMD_OTA_END && sessionActive) {
                            // Verify Checksum
                            // Payload should contain CRC32 (4 bytes)
                            if (len >= 4) {
                                uint32_t expectedCRC = rxPayload[0] | (rxPayload[1] << 8) | (rxPayload[2] << 16) | (rxPayload[3] << 24);
                                globalCRC = globalCRC ^ 0xFFFFFFFF; // Finalize

                                if (globalCRC == expectedCRC) {
                                    Log("OTA End. Checksum OK (%08X).\n", globalCRC);
                                    uint8_t ack = CMD_OTA_ACK;
                                    HAL_UART_Transmit(&huart6, &ack, 1, 100);
                                    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_SET);
                                } else {
                                    Log("OTA Fail. Checksum Mismatch. Calc: %08X, Exp: %08X\n", globalCRC, expectedCRC);
                                    uint8_t nack = CMD_OTA_NACK;
                                    HAL_UART_Transmit(&huart6, &nack, 1, 100);
                                    sessionActive = false; // Abort
                                }
                            } else {
                                Log("OTA End. Missing Checksum.\n");
                                uint8_t nack = CMD_OTA_NACK;
                                HAL_UART_Transmit(&huart6, &nack, 1, 100);
                            }
                        }
                        else if (cmd == CMD_OTA_APPLY && sessionActive) {
                            Log("Applying Update (Swap Bank)...\n");

                            HAL_FLASH_Unlock();
                            HAL_FLASH_OB_Unlock();

                            FLASH->OPTCR ^= (1 << 23); // Toggle BFB2

                            HAL_FLASH_OB_Launch(); // Resets device

                            HAL_FLASH_OB_Lock();
                            HAL_FLASH_Lock();
                        }
                    }
                }
            }
        }
    }
}

void JumpToApplication(void) {
    // Disable Peripherals
    HAL_UART_DeInit(&huart3);
    HAL_DeInit();

    // Disable Interrupts
    __disable_irq();

    // Set Vector Table
    SCB->VTOR = 0x08008000;

    // Set Stack Pointer
    uint32_t appStack = *(__IO uint32_t*)0x08008000;
    __set_MSP(appStack);

    // Jump
    uint32_t appEntry = *(__IO uint32_t*)0x08008004;
    void (*pApp)(void) = (void (*)(void))appEntry;
    pApp();
}

void Log(const char* fmt, ...) {
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    HAL_UART_Transmit(&huart3, (uint8_t*)buf, strlen(buf), 100);
}

// System Clock Config (typical for Discovery F469)
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
    while(1);
  }

  if (HAL_PWREx_EnableOverDrive() != HAL_OK) {
    while(1);
  }

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
    while(1);
  }
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
    if (HAL_UART_Init(&huart3) != HAL_OK) while(1);
}

static void MX_USART6_UART_Init(void) {
    huart6.Instance = USART6;
    huart6.Init.BaudRate = 921600; // Fast for OTA
    huart6.Init.WordLength = UART_WORDLENGTH_8B;
    huart6.Init.StopBits = UART_STOPBITS_1;
    huart6.Init.Parity = UART_PARITY_NONE;
    huart6.Init.Mode = UART_MODE_TX_RX;
    huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart6.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart6) != HAL_OK) while(1);
}

static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOK_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE(); // USART3
    // USART6 pins clk enabled by HAL_UART_MspInit usually, but here we do it manual or rely on HAL_UART_Init calling MspInit
    // For simplicity, we'll enable clocks here and configure pins if we don't implement full MSP

    // LEDs
    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_ORANGE_PORT, LED_ORANGE_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_BLUE_PORT, LED_BLUE_PIN, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = LED_GREEN_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(LED_GREEN_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LED_ORANGE_PIN | LED_RED_PIN;
    HAL_GPIO_Init(LED_ORANGE_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LED_BLUE_PIN;
    HAL_GPIO_Init(LED_BLUE_PORT, &GPIO_InitStruct);
}

// Minimal MSP Init for UARTs (linking pins)
void HAL_UART_MspInit(UART_HandleTypeDef* huart) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if(huart->Instance==USART3) {
        __HAL_RCC_USART3_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();
        // PB10=TX, PB11=RX
        GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    }
    else if(huart->Instance==USART6) {
        __HAL_RCC_USART6_CLK_ENABLE();
        __HAL_RCC_GPIOG_CLK_ENABLE();
        // PG14=TX, PG9=RX
        GPIO_InitStruct.Pin = GPIO_PIN_14|GPIO_PIN_9;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF8_USART6;
        HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
    }
}

void HAL_UART_MspDeInit(UART_HandleTypeDef* huart) {
    if(huart->Instance==USART3) {
        __HAL_RCC_USART3_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOB, GPIO_PIN_10|GPIO_PIN_11);
    }
    else if(huart->Instance==USART6) {
        __HAL_RCC_USART6_CLK_DISABLE();
        HAL_GPIO_DeInit(GPIOG, GPIO_PIN_14|GPIO_PIN_9);
    }
}

uint8_t CalculateCRC8(uint8_t *data, uint16_t len) {
    uint8_t crc = 0;
    for (uint16_t i = 0; i < len; i++) {
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

uint32_t CalculateCRC32(uint8_t *data, uint16_t len, uint32_t current_crc) {
    for (uint16_t i = 0; i < len; i++) {
        uint32_t byte = data[i];
        current_crc = current_crc ^ byte;
        for (int j = 0; j < 8; j++) {
            if (current_crc & 1)
                current_crc = (current_crc >> 1) ^ 0xEDB88320;
            else
                current_crc = current_crc >> 1;
        }
    }
    return current_crc;
}
