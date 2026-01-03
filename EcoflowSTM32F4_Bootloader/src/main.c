#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>

// Define Application Address (Sector 2)
#define APP_ADDRESS 0x08008000

// UART Protocol
#define START_BYTE 0xAA
#define CMD_OTA_START 0xA0
#define CMD_OTA_CHUNK 0xA1
#define CMD_OTA_END   0xA2
#define CMD_OTA_APPLY 0xA3
#define CMD_OTA_ACK   0x06
#define CMD_OTA_NACK  0x15

// Register Definitions
#ifndef FLASH_OPTCR_BFB2
#define FLASH_OPTCR_BFB2 (1 << 4)
#endif

// Sector Definitions for F469 (2MB)
// Bank 1: Sectors 0-11
// Bank 2: Sectors 12-23
// We only use the Application area (Sectors 2-11 in Bank 1, Sectors 14-23 in Bank 2)
// Bootloader + Config occupy Sectors 0/1 and 12/13.

UART_HandleTypeDef huart6;
UART_HandleTypeDef huart3; // Debug UART

typedef void (*pFunction)(void);
pFunction JumpToApplication;
uint32_t JumpAddress;

void SystemClock_Config(void);
void UART_Init(void);
void UART3_Init(void);
void GPIO_Init(void);
void Bootloader_OTA_Loop(void);

// Custom printf for Bootloader
void debug_log(const char *fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    int len = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    HAL_UART_Transmit(&huart3, (uint8_t*)buf, len, 100);
}

// Helper to de-initialize peripherals and interrupts
void DeInit(void) {
    debug_log("Bootloader DeInit...\r\n");
    HAL_UART_DeInit(&huart6);
    HAL_UART_DeInit(&huart3);
    // De-init LEDs
    HAL_GPIO_DeInit(GPIOG, GPIO_PIN_6);
    HAL_GPIO_DeInit(GPIOD, GPIO_PIN_4 | GPIO_PIN_5);
    HAL_GPIO_DeInit(GPIOK, GPIO_PIN_3);

    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;
    __disable_irq();
    for (int i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }
    HAL_DeInit();
}

// LED Helpers
void LED_G_On() { HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_RESET); }
void LED_G_Off() { HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_SET); }
void LED_G_Toggle() { HAL_GPIO_TogglePin(GPIOG, GPIO_PIN_6); }

void LED_O_On() { HAL_GPIO_WritePin(GPIOD, GPIO_PIN_4, GPIO_PIN_RESET); }
void LED_O_Off() { HAL_GPIO_WritePin(GPIOD, GPIO_PIN_4, GPIO_PIN_SET); }
void LED_O_Toggle() { HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_4); }

void LED_R_On() { HAL_GPIO_WritePin(GPIOD, GPIO_PIN_5, GPIO_PIN_RESET); }
void LED_R_Off() { HAL_GPIO_WritePin(GPIOD, GPIO_PIN_5, GPIO_PIN_SET); }
void LED_R_Toggle() { HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_5); }

void LED_B_On() { HAL_GPIO_WritePin(GPIOK, GPIO_PIN_3, GPIO_PIN_RESET); }
void LED_B_Off() { HAL_GPIO_WritePin(GPIOK, GPIO_PIN_3, GPIO_PIN_SET); }
void LED_B_Toggle() { HAL_GPIO_TogglePin(GPIOK, GPIO_PIN_3); }

void All_LEDs_Off() {
    LED_G_Off(); LED_O_Off(); LED_R_Off(); LED_B_Off();
}

int main(void) {
    HAL_Init();
    GPIO_Init();

    // 1. Startup Sequence
    LED_B_On(); HAL_Delay(100); LED_B_Off();
    LED_O_On(); HAL_Delay(100); LED_O_Off();
    LED_R_On(); HAL_Delay(100); LED_R_Off();
    LED_G_On(); HAL_Delay(100); LED_G_Off();

    SystemClock_Config();
    UART_Init();
    UART3_Init();

    debug_log("\r\n--- STM32F4 Bootloader Start ---\r\n");

    // Check Backup Register for OTA Flag
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    bool ota_flag = (RTC->BKP0R == 0xDEADBEEF);
    debug_log("OTA Flag: %d (RTC_BKP0R=0x%08X)\r\n", ota_flag, RTC->BKP0R);

    // Check App Validity
    uint32_t sp = *(__IO uint32_t*)APP_ADDRESS;
    bool valid_app = ((sp & 0x2FFE0000) == 0x20000000);
    debug_log("App SP: 0x%08X, Valid: %d\r\n", sp, valid_app);

    if (ota_flag) {
        debug_log("Forcing OTA Loop...\r\n");
        // Clear flag
        RTC->BKP0R = 0;
        Bootloader_OTA_Loop();
    } else if (valid_app) {
        // Wait 500ms for OTA START - Blink Blue (Scenario 1)
        uint8_t buf[1];
        LED_B_On();
        if (HAL_UART_Receive(&huart6, buf, 1, 500) == HAL_OK) {
            if (buf[0] == START_BYTE) {
                 debug_log("Received START byte during timeout. Entering OTA Loop.\r\n");
                 Bootloader_OTA_Loop();
            }
        }
        LED_B_Off();

        // Jump - Green ON
        LED_G_On();
        debug_log("Jumping to Application at 0x%08X\r\n", APP_ADDRESS);
        DeInit();

        JumpAddress = *(__IO uint32_t*) (APP_ADDRESS + 4);
        JumpToApplication = (pFunction) JumpAddress;
        __set_MSP(sp);
        JumpToApplication();
    } else {
        debug_log("No valid app. Entering OTA Loop.\r\n");
        // No valid app, force OTA
        Bootloader_OTA_Loop();
    }

    while (1) {}
}

uint8_t calculate_crc8(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) crc = (crc << 1) ^ 0x31; else crc <<= 1;
        }
    }
    return crc;
}

void send_ack() {
    debug_log("TX ACK\r\n");
    uint8_t buf[4] = {START_BYTE, CMD_OTA_ACK, 0, 0};
    buf[3] = calculate_crc8(&buf[1], 2);
    HAL_UART_Transmit(&huart6, buf, 4, 100);
    // Green Flash
    LED_G_On(); HAL_Delay(50); LED_G_Off();
}

void send_nack() {
    debug_log("TX NACK\r\n");
    uint8_t buf[4] = {START_BYTE, CMD_OTA_NACK, 0, 0};
    buf[3] = calculate_crc8(&buf[1], 2);
    HAL_UART_Transmit(&huart6, buf, 4, 100);
    // Red Flash
    LED_R_On(); HAL_Delay(500); LED_R_Off();
}

void ClearFlashFlags() {
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR | FLASH_FLAG_RDERR);
}

void Bootloader_OTA_Loop(void) {
    uint8_t header[3];
    uint8_t payload[256];

    debug_log("Entering OTA Loop\r\n");

    HAL_FLASH_Unlock();
    ClearFlashFlags(); // Clear flags on entry
    All_LEDs_Off();

    // Determine Active Bank and Target Bank
    FLASH_OBProgramInitTypeDef OBInit;
    HAL_FLASHEx_OBGetConfig(&OBInit);
    bool bfb2_active = ((OBInit.USERConfig & FLASH_OPTCR_BFB2) == FLASH_OPTCR_BFB2);

    debug_log("Option Bytes: 0x%08X, BFB2: %d\r\n", OBInit.USERConfig, bfb2_active);

    // Target Inactive Bank
    uint32_t target_bank_addr = 0x08100000;

    uint32_t start_sector, end_sector;
    if (bfb2_active) {
        start_sector = FLASH_SECTOR_0;
        end_sector = FLASH_SECTOR_11;
        debug_log("Active: Bank 2. Target: Bank 1 (Sectors 0-11)\r\n");
    } else {
        start_sector = FLASH_SECTOR_12;
        end_sector = FLASH_SECTOR_23;
        debug_log("Active: Bank 1. Target: Bank 2 (Sectors 12-23)\r\n");
    }

    bool ota_started = false;
    uint32_t bytes_written = 0;

    while(1) {
        // Heartbeat: Blue Toggle
        static uint32_t last_tick = 0;
        if (HAL_GetTick() - last_tick > (ota_started ? 200 : 1000)) {
            LED_B_Toggle();
            last_tick = HAL_GetTick();
        }

        uint8_t b;
        if (HAL_UART_Receive(&huart6, &b, 1, 10) != HAL_OK) continue;
        if (b != START_BYTE) continue;

        // RX Activity: Orange On
        LED_O_On();

        if (HAL_UART_Receive(&huart6, &header[1], 2, 100) != HAL_OK) {
            debug_log("Header Timeout\r\n");
            LED_O_Off(); continue;
        }
        uint8_t cmd = header[1];
        uint8_t len = header[2];

        // debug_log("RX CMD: 0x%02X LEN: %d\r\n", cmd, len);

        if (HAL_UART_Receive(&huart6, payload, len + 1, 500) != HAL_OK) {
            debug_log("Payload Timeout\r\n");
            LED_O_Off(); continue;
        }

        LED_O_Off(); // RX Done

        uint8_t recv_crc = payload[len];
        uint8_t check_buf[300];
        check_buf[0] = cmd;
        check_buf[1] = len;
        memcpy(&check_buf[2], payload, len);

        if (calculate_crc8(check_buf, 2 + len) != recv_crc) {
            debug_log("CRC Fail. Recv: 0x%02X\r\n", recv_crc);
            send_nack(); continue;
        }

        if (cmd == CMD_OTA_START) {
            debug_log("CMD_OTA_START Received\r\n");
            ota_started = true;
            bytes_written = 0;

            FLASH_EraseInitTypeDef EraseInitStruct;
            uint32_t SectorError;
            EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
            EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
            EraseInitStruct.NbSectors = 1;

            ClearFlashFlags(); // Clear flags before Erase

            bool error = false;
            // Erase the ENTIRE Inactive Bank (Bootloader area included)
            for (uint32_t sec = start_sector; sec <= end_sector; sec++) {
                LED_O_Toggle(); // Toggle Orange during erase
                EraseInitStruct.Sector = sec;

                debug_log("Erasing Sector %d... ", sec);
                if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
                    debug_log("Fail! Error: 0x%08X\r\n", HAL_FLASH_GetError());
                    error = true; break;
                }
                debug_log("OK\r\n");
            }

            if (!error) {
                ClearFlashFlags(); // Clear flags after Erase, before Write
                send_ack();
            } else {
                send_nack();
            }
            LED_O_Off();
        }
        else if (cmd == CMD_OTA_CHUNK) {
            uint32_t offset;
            memcpy(&offset, payload, 4);
            uint8_t *data = &payload[4];
            uint32_t data_len = len - 4;

            // debug_log("CMD_OTA_CHUNK Offset: %d Len: %d\r\n", offset, data_len);

            // Write to Inactive Bank
            uint32_t addr = target_bank_addr + offset;

            bool ok = true;
            for (uint32_t i=0; i<data_len; i+=4) {
                uint32_t word = 0xFFFFFFFF;
                uint8_t copy_len = (data_len - i < 4) ? (data_len - i) : 4;
                memcpy(&word, &data[i], copy_len);

                if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i, word) != HAL_OK) {
                    debug_log("Write Fail at 0x%08X. Error: 0x%08X\r\n", addr+i, HAL_FLASH_GetError());
                    ok = false; break;
                }

                // Read-Back Verify
                if (*(uint32_t*)(addr + i) != word) {
                    debug_log("Verify Fail at 0x%08X. Read: 0x%08X Expected: 0x%08X\r\n", addr+i, *(uint32_t*)(addr+i), word);
                    ok = false; break;
                }
            }
            if (ok) {
                bytes_written += data_len;
                send_ack();
            } else {
                send_nack();
            }
        }
        else if (cmd == CMD_OTA_END) {
            debug_log("CMD_OTA_END. Bytes Written: %d\r\n", bytes_written);
            send_ack();
            All_LEDs_Off();
            LED_G_On(); // Green Solid
        }
        else if (cmd == CMD_OTA_APPLY) {
            debug_log("CMD_OTA_APPLY. Swapping Bank...\r\n");
            send_ack();
            HAL_Delay(100);

            // Toggle BFB2
            HAL_FLASH_Unlock();
            HAL_FLASH_OB_Unlock();
            FLASH_OBProgramInitTypeDef OBInit;
            HAL_FLASHEx_OBGetConfig(&OBInit);

            // Toggle
            OBInit.OptionType = OPTIONBYTE_USER;
            if (bfb2_active) {
                OBInit.USERConfig &= ~FLASH_OPTCR_BFB2; // Disable BFB2
            } else {
                OBInit.USERConfig |= FLASH_OPTCR_BFB2; // Enable BFB2
            }

            debug_log("Writing Option Bytes...\r\n");
            HAL_FLASHEx_OBProgram(&OBInit);
            HAL_FLASH_OB_Launch(); // Resets system

            // Should not reach here
            HAL_NVIC_SystemReset();
        }
    }
}

void UART_Init(void) {
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
}

void UART3_Init(void) {
    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    // PB10 -> TX, PB11 -> RX
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

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


void GPIO_Init(void) {
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOK_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // PG6 (Green)
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    // PD4 (Orange), PD5 (Red)
    GPIO_InitStruct.Pin = GPIO_PIN_4 | GPIO_PIN_5;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    // PK3 (Blue)
    GPIO_InitStruct.Pin = GPIO_PIN_3;
    HAL_GPIO_Init(GPIOK, &GPIO_InitStruct);

    All_LEDs_Off();
}

void SystemClock_Config(void)
{
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
  RCC_OscInitStruct.PLL.PLLQ = 4;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if(HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
      while(1) { LED_R_On(); HAL_Delay(50); LED_R_Off(); HAL_Delay(50); }
  }

  HAL_PWREx_EnableOverDrive();

  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);
}

void SysTick_Handler(void)
{
  HAL_IncTick();
}
