#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdbool.h>

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
// STM32F469: BFB2 is Bit 23
#define FLASH_OPTCR_BFB2 (1 << 23)
#endif

// RTC Backup Register for OTA Flag
#define RTC_BKP_DR0 0x00 // Index 0
#define OTA_MAGIC_NUMBER 0xDEADBEEF

UART_HandleTypeDef huart6;
RTC_HandleTypeDef hrtc;

typedef void (*pFunction)(void);
pFunction JumpToApplication;
uint32_t JumpAddress;

void SystemClock_Config(void);
void UART_Init(void);
void GPIO_Init(void);
void RTC_Init(void);
void Bootloader_OTA_Loop(void);

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
    RTC_Init();

    // Check Magic Number
    bool force_ota = false;
    if (HAL_RTCEx_BKUPRead(&hrtc, RTC_BKP_DR0) == OTA_MAGIC_NUMBER) {
        force_ota = true;
        // Clear Magic
        HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, 0);
    }

    // Check App Validity
    uint32_t sp = *(__IO uint32_t*)APP_ADDRESS;
    bool valid_app = ((sp & 0x2FFE0000) == 0x20000000);

    if (force_ota || !valid_app) {
        Bootloader_OTA_Loop();
    } else {
        // Normal Boot - Wait briefly for UART (Rescue)
        uint8_t buf[1];
        LED_B_On();
        if (HAL_UART_Receive(&huart6, buf, 1, 100) == HAL_OK) { // Short window
             if (buf[0] == START_BYTE) {
                 Bootloader_OTA_Loop();
             }
        }
        LED_B_Off();

        // Jump to App
        LED_G_On();
        DeInit();
        JumpAddress = *(__IO uint32_t*) (APP_ADDRESS + 4);
        JumpToApplication = (pFunction) JumpAddress;
        __set_MSP(sp);
        JumpToApplication();
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
    LED_G_On(); HAL_Delay(10); LED_G_Off();
}

void send_nack() {
    uint8_t buf[4] = {START_BYTE, CMD_OTA_NACK, 0, 0};
    buf[3] = calculate_crc8(&buf[1], 2);
    HAL_UART_Transmit(&huart6, buf, 4, 100);
    LED_R_On(); HAL_Delay(100); LED_R_Off();
}

void Bootloader_OTA_Loop(void) {
    uint8_t header[3];
    uint8_t payload[256]; // Max chunk size
    uint32_t total_received_checksum = 0; // Checksum of received payload data
    uint32_t total_bytes_received = 0;

    // Determine Bank Status
    HAL_FLASH_Unlock();
    FLASH_OBProgramInitTypeDef OBInit;
    HAL_FLASHEx_OBGetConfig(&OBInit);
    bool bank2_active = (OBInit.USERConfig & FLASH_OPTCR_BFB2) == FLASH_OPTCR_BFB2;

    // Inactive Bank Address is always 0x08100000 in Dual Bank Mode
    uint32_t inactive_bank_addr = 0x08100000;

    All_LEDs_Off();
    // Blink Blue to indicate Ready
    bool ota_started = false;

    while(1) {
        static uint32_t last_tick = 0;
        if (HAL_GetTick() - last_tick > (ota_started ? 100 : 500)) {
            LED_B_Toggle();
            last_tick = HAL_GetTick();
        }

        uint8_t b;
        if (HAL_UART_Receive(&huart6, &b, 1, 5) != HAL_OK) continue;
        if (b != START_BYTE) continue;

        if (HAL_UART_Receive(&huart6, &header[1], 2, 50) != HAL_OK) continue;
        uint8_t cmd = header[1];
        uint8_t len = header[2];

        if (HAL_UART_Receive(&huart6, payload, len + 1, 500) != HAL_OK) continue;

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
            total_received_checksum = 0;
            total_bytes_received = 0;

            // Erase Inactive Bank
            // If Bank 2 Active (BFB2=1) -> Erase Bank 1 (Sectors 0-11)
            // If Bank 1 Active (BFB2=0) -> Erase Bank 2 (Sectors 12-23)

            uint32_t start_sector = bank2_active ? FLASH_SECTOR_0 : FLASH_SECTOR_12;
            uint32_t end_sector = bank2_active ? FLASH_SECTOR_11 : FLASH_SECTOR_23;

            FLASH_EraseInitTypeDef EraseInitStruct;
            uint32_t SectorError;
            EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
            EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
            EraseInitStruct.NbSectors = 1;

            bool error = false;
            LED_O_On(); // Orange On during erase
            for (uint32_t sec = start_sector; sec <= end_sector; sec++) {
                LED_O_Toggle();
                EraseInitStruct.Sector = sec;
                 // __HAL_IWDG_RELOAD_COUNTER(&hiwdg); // Removed as handle not local

                if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
                    error = true; break;
                }
            }
            LED_O_Off();

            if (!error) send_ack(); else send_nack();
        }
        else if (cmd == CMD_OTA_CHUNK) {
            uint32_t offset;
            memcpy(&offset, payload, 4);
            uint8_t *data = &payload[4];
            uint32_t data_len = len - 4;

            uint32_t addr = inactive_bank_addr + offset;

            bool ok = true;
            for (uint32_t i=0; i<data_len; i+=4) {
                uint32_t word = 0xFFFFFFFF;
                uint8_t copy_len = (data_len - i < 4) ? (data_len - i) : 4;
                memcpy(&word, &data[i], copy_len);

                if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i, word) != HAL_OK) {
                    ok = false; break;
                }

                // Add to simple checksum (sum of words)
                // Note: This sums the *payload* as we write it.
                // A better check is to read back.
                // We will read back in APPLY.
            }

            if (ok) {
                // Calculate Checksum of RECEIVED data for later verification
                for(uint32_t i=0; i<data_len; i++) {
                    total_received_checksum += data[i];
                }
                total_bytes_received += data_len;
                send_ack();
            } else {
                send_nack();
            }
        }
        else if (cmd == CMD_OTA_END) {
             send_ack();
             LED_B_Off(); LED_G_On(); // Green to indicate done receiving
        }
        else if (cmd == CMD_OTA_APPLY) {
            // Verify Checksum
            // Read back flash and compare sum
            uint32_t read_checksum = 0;
            for (uint32_t i=0; i<total_bytes_received; i++) {
                read_checksum += *(__IO uint8_t*)(inactive_bank_addr + i);
            }

            bool verify_ok = (read_checksum == total_received_checksum);

            // Also check SP
            uint32_t *check_ptr = (uint32_t*)inactive_bank_addr;
            if ((*check_ptr & 0x2FFE0000) != 0x20000000) {
                 verify_ok = false;
            }

            if (verify_ok) {
                send_ack();
                HAL_Delay(100);

                HAL_FLASH_Unlock();
                HAL_FLASH_OB_Unlock();

                HAL_FLASHEx_OBGetConfig(&OBInit);
                OBInit.OptionType = OPTIONBYTE_USER;

                // Toggle BFB2
                if (bank2_active) {
                     OBInit.USERConfig &= ~FLASH_OPTCR_BFB2;
                } else {
                     OBInit.USERConfig |= FLASH_OPTCR_BFB2;
                }

                if (HAL_FLASHEx_OBProgram(&OBInit) == HAL_OK) {
                    HAL_FLASH_OB_Launch(); // This resets the system
                }
                HAL_NVIC_SystemReset();
            } else {
                send_nack(); // Verification Failed
            }
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

void RTC_Init(void) {
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    __HAL_RCC_RTC_ENABLE();

    hrtc.Instance = RTC;
    // Basic init if needed, mostly we just need the clock enabled for BKP registers
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
