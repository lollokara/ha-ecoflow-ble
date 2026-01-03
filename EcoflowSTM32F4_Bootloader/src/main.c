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
#define FLASH_OPTCR_BFB2 (1 << 4) // Bit 23 in RM, but HAL def might vary. Check HAL.
// Actually in stm32f469xx.h: #define FLASH_OPTCR_BFB2_Pos (23U)
// #define FLASH_OPTCR_BFB2_Msk (0x1U << FLASH_OPTCR_BFB2_Pos)
#endif

// Sector Definitions for F469 (2MB)
// Bank 1: Sectors 0-11
// Bank 2: Sectors 12-23
// We only use the Application area (Sectors 2-11 in Bank 1, Sectors 14-23 in Bank 2)
// Bootloader + Config occupy Sectors 0/1 and 12/13.

UART_HandleTypeDef huart6; // OTA UART
UART_HandleTypeDef huart3; // Debug UART
IWDG_HandleTypeDef hiwdg;  // Watchdog

typedef void (*pFunction)(void);
pFunction JumpToApplication;
uint32_t JumpAddress;

void SystemClock_Config(void);
void UART6_Init(void);
void USART3_Init(void);
void GPIO_Init(void);
void IWDG_Init(void);
void Bootloader_OTA_Loop(void);

// Serial Printf Helper
void Serial_Printf(const char *fmt, ...) {
    char buf[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    HAL_UART_Transmit(&huart3, (uint8_t*)buf, strlen(buf), 100);
}

// Helper to de-initialize peripherals and interrupts
void DeInit(void) {
    Serial_Printf("De-initializing...\n");
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
    USART3_Init(); // Debug UART early
    Serial_Printf("\n\n--- Bootloader Started ---\n");

    // 1. Startup Sequence
    LED_B_On(); HAL_Delay(100); LED_B_Off();
    LED_O_On(); HAL_Delay(100); LED_O_Off();
    LED_R_On(); HAL_Delay(100); LED_R_Off();
    LED_G_On(); HAL_Delay(100); LED_G_Off();

    SystemClock_Config();
    UART6_Init();
    IWDG_Init(); // Start Watchdog

    // Check Backup Register for OTA Flag
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    bool ota_flag = (RTC->BKP0R == 0xDEADBEEF);

    // Check App Validity (SP in RAM and Reset Vector in Flash)
    uint32_t sp = *(__IO uint32_t*)APP_ADDRESS;
    uint32_t rv = *(__IO uint32_t*)(APP_ADDRESS + 4);

    // RAM is 0x20000000 - 0x20050000 (320K)
    bool valid_sp = ((sp & 0xFFF80000) == 0x20000000);
    // RV should be in Flash Range (Active Bank 0x08008000 - 0x080FFFFF)
    // Actually, it can be in 0x08000000 - 0x081FFFFF
    bool valid_rv = ((rv & 0xFFF00000) == 0x08000000) || ((rv & 0xFFF00000) == 0x08100000);

    bool valid_app = valid_sp && valid_rv;

    Serial_Printf("App Check: Addr=0x%08X, SP=0x%08X, RV=0x%08X, Valid=%d\n", APP_ADDRESS, sp, rv, valid_app);
    Serial_Printf("OTA Flag: %d (0x%08X)\n", ota_flag, RTC->BKP0R);

    if (ota_flag) {
        Serial_Printf("Forcing OTA Mode from Flag.\n");
        // Clear flag
        RTC->BKP0R = 0;
        Bootloader_OTA_Loop();
    } else if (valid_app) {
        // Wait 500ms for OTA START - Blink Blue (Scenario 1)
        uint8_t buf[1];
        LED_B_On();
        HAL_IWDG_Refresh(&hiwdg);

        // Non-blocking peek or short timeout check
        if (HAL_UART_Receive(&huart6, buf, 1, 500) == HAL_OK) {
            if (buf[0] == START_BYTE) {
                 Serial_Printf("Received START_BYTE during boot wait. Entering OTA.\n");
                 Bootloader_OTA_Loop();
            }
        }
        LED_B_Off();

        // Double check for any character in buffer? No, simple is better.

        // Jump - Green ON
        Serial_Printf("Jumping to Application...\n");
        LED_G_On();
        DeInit();

        JumpAddress = *(__IO uint32_t*) (APP_ADDRESS + 4);
        JumpToApplication = (pFunction) JumpAddress;
        __set_MSP(sp);
        JumpToApplication();
    } else {
        // No valid app, force OTA
        Serial_Printf("No Valid App Found. Entering OTA Mode.\n");
        Bootloader_OTA_Loop();
    }

    while (1) {
        HAL_IWDG_Refresh(&hiwdg);
    }
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
    uint8_t buf[4] = {START_BYTE, CMD_OTA_ACK, 0, 0};
    buf[3] = calculate_crc8(&buf[1], 2);
    HAL_UART_Transmit(&huart6, buf, 4, 100);
    // Green Flash
    LED_G_On(); HAL_Delay(50); LED_G_Off();
}

void send_nack() {
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

    HAL_FLASH_Unlock();
    ClearFlashFlags(); // Clear flags on entry
    All_LEDs_Off();

    // Determine Active Bank and Target Bank
    FLASH_OBProgramInitTypeDef OBInit;
    HAL_FLASHEx_OBGetConfig(&OBInit);
    // Correct way to check BFB2
    // If BFB2 bit is SET, we booted from Bank 2 (mapped to 0x08000000).
    // The inactive bank is physically Bank 1, mapped at 0x08100000.
    bool bfb2_active = ((OBInit.USERConfig & FLASH_OPTCR_BFB2) == FLASH_OPTCR_BFB2);

    // Target Inactive Bank (Always 0x08100000 due to remapping?)
    // On F469, when booting from Bank 2, Bank 2 is at 0x08000000. Bank 1 is at 0x08100000.
    // When booting from Bank 1, Bank 1 is at 0x08000000. Bank 2 is at 0x08100000.
    // So writing to 0x08100000 ALWAYS targets the inactive bank.
    uint32_t target_bank_addr = 0x08100000;

    uint32_t start_sector, end_sector;
    // We need to erase physical sectors corresponding to 0x08100000.
    // If BFB2=1 (Active=Bank2), Inactive=Bank1 (Physical Sectors 0-11).
    // If BFB2=0 (Active=Bank1), Inactive=Bank2 (Physical Sectors 12-23).
    if (bfb2_active) {
        start_sector = FLASH_SECTOR_0;
        end_sector = FLASH_SECTOR_11;
        Serial_Printf("Active: Bank 2. Target: Bank 1 (Sectors 0-11)\n");
    } else {
        start_sector = FLASH_SECTOR_12;
        end_sector = FLASH_SECTOR_23;
        Serial_Printf("Active: Bank 1. Target: Bank 2 (Sectors 12-23)\n");
    }

    bool ota_started = false;
    uint32_t bytes_written = 0;

    Serial_Printf("Waiting for OTA Packets...\n");

    while(1) {
        HAL_IWDG_Refresh(&hiwdg);

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
            LED_O_Off(); continue;
        }
        uint8_t cmd = header[1];
        uint8_t len = header[2];

        if (HAL_UART_Receive(&huart6, payload, len + 1, 500) != HAL_OK) {
            LED_O_Off(); continue;
        }

        LED_O_Off(); // RX Done

        uint8_t recv_crc = payload[len];
        uint8_t check_buf[300];
        check_buf[0] = cmd;
        check_buf[1] = len;
        memcpy(&check_buf[2], payload, len);

        if (calculate_crc8(check_buf, 2 + len) != recv_crc) {
            Serial_Printf("CRC Error! Cmd: %02X\n", cmd);
            send_nack(); continue;
        }

        if (cmd == CMD_OTA_START) {
            ota_started = true;
            bytes_written = 0;
            Serial_Printf("CMD_OTA_START Received. Erasing Sectors %d to %d...\n", start_sector, end_sector);

            FLASH_EraseInitTypeDef EraseInitStruct;
            uint32_t SectorError;
            EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
            EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
            EraseInitStruct.NbSectors = 1;

            ClearFlashFlags(); // Clear flags before Erase

            bool error = false;
            // Erase the ENTIRE Inactive Bank (Bootloader area included)
            for (uint32_t sec = start_sector; sec <= end_sector; sec++) {
                HAL_IWDG_Refresh(&hiwdg); // IMPORTANT: Refresh during erase
                LED_O_Toggle(); // Toggle Orange during erase
                EraseInitStruct.Sector = sec;

                // Serial_Printf("Erasing Sector %d...\n", sec);
                if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
                    Serial_Printf("Erase Error at Sector %d (Code: 0x%X)\n", sec, HAL_FLASH_GetError());
                    error = true; break;
                }
            }

            if (!error) {
                Serial_Printf("Erase Complete. Sending ACK.\n");
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

            // Write to Inactive Bank
            uint32_t addr = target_bank_addr + offset;

            // Serial_Printf("Chunk: Offset=%d, Len=%d, Addr=0x%08X\n", offset, data_len, addr);

            bool ok = true;
            for (uint32_t i=0; i<data_len; i+=4) {
                // HAL_IWDG_Refresh(&hiwdg); // Refresh occasionally during write? 256 bytes is fast enough.

                uint32_t word = 0xFFFFFFFF;
                uint8_t copy_len = (data_len - i < 4) ? (data_len - i) : 4;
                memcpy(&word, &data[i], copy_len);

                if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i, word) != HAL_OK) {
                    Serial_Printf("Write Error at 0x%08X\n", addr + i);
                    ok = false; break;
                }

                // Read-Back Verify
                if (*(uint32_t*)(addr + i) != word) {
                    Serial_Printf("Verify Error at 0x%08X\n", addr + i);
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
            Serial_Printf("CMD_OTA_END. Total Written: %d bytes.\n", bytes_written);
            send_ack();
            All_LEDs_Off();
            LED_G_On(); // Green Solid
        }
        else if (cmd == CMD_OTA_APPLY) {
            Serial_Printf("CMD_OTA_APPLY. Toggling BFB2 and Resetting...\n");
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

            if(HAL_FLASHEx_OBProgram(&OBInit) != HAL_OK) {
                 Serial_Printf("OB Program Failed!\n");
            } else {
                 HAL_FLASH_OB_Launch(); // Resets system
            }

            // Should not reach here
            Serial_Printf("Reset Failed?\n");
            HAL_NVIC_SystemReset();
        }
    }
}

void UART6_Init(void) {
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

void USART3_Init(void) {
    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

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

void IWDG_Init(void) {
    // IWDG runs on LSI (32kHz). Prescaler 256 -> 125Hz.
    // Reload 1250 -> 10 seconds.
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_256;
    hiwdg.Init.Reload = 1250;
    if (HAL_IWDG_Init(&hiwdg) != HAL_OK) {
        Serial_Printf("IWDG Init Failed!\n");
    }
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
