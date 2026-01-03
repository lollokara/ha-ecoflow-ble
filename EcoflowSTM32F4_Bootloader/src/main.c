#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdbool.h>

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

#define OTA_FLAG 0xDEADBEEF

UART_HandleTypeDef huart6;
IWDG_HandleTypeDef hiwdg;

typedef void (*pFunction)(void);
pFunction JumpToApplication;
uint32_t JumpAddress;

void SystemClock_Config(void);
void UART_Init(void);
void GPIO_Init(void);
void Bootloader_OTA_Loop(void);

static void MX_IWDG_Init(void) {
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_256;
    hiwdg.Init.Reload = 1250; // 10s
    if (HAL_IWDG_Init(&hiwdg) != HAL_OK) {
        // Error
    }
}

// Helper to de-initialize peripherals and interrupts
void DeInit(void) {
    HAL_UART_DeInit(&huart6);
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

// Get the actual App Address based on the Active Bank
// Note: Hardware aliases the Active Bank to 0x08000000 (usually) or at least 0x00000000.
// We jump to the fixed Boot Address 0x08008000 which should map to the Active Application.
uint32_t GetActiveAppAddress() {
    return 0x08008000;
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
    MX_IWDG_Init();

    // Enable Backup Access for RTC Flag
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    // Check OTA Flag in RTC Backup Register 0
    if (RTC->BKP0R == OTA_FLAG) {
        // Clear flag
        RTC->BKP0R = 0;
        // Enter OTA Mode
        Bootloader_OTA_Loop();
    }

    // Check App
    uint32_t app_addr = GetActiveAppAddress();
    uint32_t sp = *(__IO uint32_t*)app_addr;
    bool valid_app = ((sp & 0x2FFE0000) == 0x20000000);

    if (valid_app) {
        // Wait 500ms for OTA START - Blink Blue
        uint8_t buf[1];
        LED_B_On();
        if (HAL_UART_Receive(&huart6, buf, 1, 500) == HAL_OK) {
            if (buf[0] == START_BYTE) {
                Bootloader_OTA_Loop();
            }
        }
        LED_B_Off();

        // Jump - Green ON
        LED_G_On();
        DeInit();

        JumpAddress = *(__IO uint32_t*) (app_addr + 4);
        JumpToApplication = (pFunction) JumpAddress;
        __set_MSP(sp);
        JumpToApplication();
    } else {
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

void Bootloader_OTA_Loop(void) {
    uint8_t header[3];
    uint8_t payload[256];

    HAL_FLASH_Unlock();
    All_LEDs_Off();

    // OTA Ready: Slow Blue Blink
    bool ota_started = false;

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
            send_nack(); continue;
        }

        if (cmd == CMD_OTA_START) {
            ota_started = true;
            FLASH_EraseInitTypeDef EraseInitStruct;
            uint32_t SectorError;
            EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
            EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;

            // Target Inactive Bank (Always mapped to 0x08100000 / Sectors 12-23 in Dual Bank Mode)
            uint32_t start_sec = FLASH_SECTOR_12;
            uint32_t end_sec = FLASH_SECTOR_23;

            EraseInitStruct.NbSectors = 1;

            bool error = false;
            for (uint32_t sec = start_sec; sec <= end_sec; sec++) {
                LED_O_Toggle(); // Toggle Orange during erase
                EraseInitStruct.Sector = sec;

                // Clear flags before erase
                __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                                       FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

                if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
                    error = true; break;
                }

                // Refresh watchdog
                 HAL_IWDG_Refresh(&hiwdg);
            }

            if (!error) send_ack(); else send_nack();
            LED_O_Off();
        }
        else if (cmd == CMD_OTA_CHUNK) {
            uint32_t offset;
            memcpy(&offset, payload, 4);
            uint8_t *data = &payload[4];
            uint32_t data_len = len - 4;

            // Target Inactive Bank Base Address
            uint32_t base_addr = 0x08100000;

            // Write Address (Assumes factory_firmware.bin starts at offset 0 of the bank)
            uint32_t addr = base_addr + offset;

            bool ok = true;
            for (uint32_t i=0; i<data_len; i+=4) {
                uint32_t word = 0xFFFFFFFF;
                uint8_t copy_len = (data_len - i < 4) ? (data_len - i) : 4;
                memcpy(&word, &data[i], copy_len);

                if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i, word) != HAL_OK) {
                    ok = false; break;
                }
            }
            if (ok) send_ack(); else send_nack();
        }
        else if (cmd == CMD_OTA_END) {
             // Verify the written bank
             uint32_t base_addr = 0x08100000;

            // Simple Verification: Check if App Stack Pointer is valid at target
            // App is at +0x8000 in the bank
            uint32_t app_sp = *(__IO uint32_t*)(base_addr + 0x8000);
            if ((app_sp & 0x2FFE0000) == 0x20000000) {
                 send_ack();
                 All_LEDs_Off();
                 LED_G_On(); // Green Solid (Success)
            } else {
                 send_nack();
                 LED_R_On(); // Red Solid (Fail)
            }
        }
        else if (cmd == CMD_OTA_APPLY) {
            send_ack();
            HAL_Delay(100);

            // Toggle BFB2
            HAL_FLASH_Unlock();
            HAL_FLASH_OB_Unlock();
            FLASH_OBProgramInitTypeDef OBInit;
            HAL_FLASHEx_OBGetConfig(&OBInit);

            // Toggle the bit
            if ((OBInit.USERConfig & FLASH_OPTCR_BFB2) == FLASH_OPTCR_BFB2) {
                OBInit.USERConfig &= ~FLASH_OPTCR_BFB2; // Set to Bank 1
            } else {
                OBInit.USERConfig |= FLASH_OPTCR_BFB2;  // Set to Bank 2
            }

            OBInit.OptionType = OPTIONBYTE_USER;
            HAL_FLASHEx_OBProgram(&OBInit);
            HAL_FLASH_OB_Launch(); // This triggers reset

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
