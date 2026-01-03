#include <stdbool.h>
#include "stm32f4xx_hal.h"
#include <string.h>

#define APP_ADDRESS         0x08008000
#define OTA_FLAG_VALUE      0xDEADBEEF
#define INACTIVE_BANK_ADDR  0x08100000

// OTA Commands
#define CMD_OTA_START       0xF0
#define CMD_OTA_CHUNK       0xF1
#define CMD_OTA_END         0xF2
#define CMD_ACK             0x21
#define CMD_NACK            0x22

UART_HandleTypeDef huart6;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART6_UART_Init(void);
void JumpToApplication(void);
void Process_OTA(void);
uint8_t Calculate_CRC8(uint8_t *data, uint16_t len);

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART6_UART_Init();

    // Check for OTA Request in Backup Register
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    uint32_t ota_flag = RTC->BKP0R;
    int force_ota = (ota_flag == OTA_FLAG_VALUE);

    if (force_ota) {
        RTC->BKP0R = 0;
        Process_OTA();
    } else {
        // Check if App exists (Check SP is in RAM)
        uint32_t app_sp = *(__IO uint32_t*)APP_ADDRESS;
        if (app_sp >= 0x20000000 && app_sp <= 0x20050000) {
            JumpToApplication();
        } else {
            Process_OTA();
        }
    }

    // Should not reach here
    while (1) {
        HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_5); // Blink Red
        HAL_Delay(500);
    }
}

void Process_OTA(void) {
    uint8_t rx_buf[300];
    uint8_t tx_buf[10];
    uint32_t current_offset = 0;

    // Determine target sectors
    uint32_t first_sector = 0;
    uint32_t nb_sectors = 12;

    FLASH_OBProgramInitTypeDef OBInit;
    HAL_FLASH_OB_Unlock();
    HAL_FLASHEx_OBGetConfig(&OBInit);
    HAL_FLASH_OB_Lock();

    uint8_t bfb2 = (FLASH->OPTCR & FLASH_OPTCR_BFB2) ? 1 : 0;

    if (bfb2 == 0) {
        // Active: Bank 1. Inactive: Bank 2 (Sectors 12-23)
        first_sector = 12;
    } else {
        // Active: Bank 2. Inactive: Bank 1 (Sectors 0-11)
        first_sector = 0;
    }

    // Blue Blink to indicate Ready
    while (1) {
        HAL_GPIO_TogglePin(GPIOK, GPIO_PIN_3);

        // Receive Header: [0xAA] [CMD] [LEN]
        // Note: We read byte-by-byte or small chunks to ensure we don't over-read into payload
        if (HAL_UART_Receive(&huart6, rx_buf, 1, 100) == HAL_OK) {
            if (rx_buf[0] == 0xAA) {
                if (HAL_UART_Receive(&huart6, &rx_buf[1], 2, 100) == HAL_OK) {
                    uint8_t cmd = rx_buf[1];
                    uint8_t len = rx_buf[2];

                    // Receive Payload + CRC
                    // Payload size is 'len'. CRC is 1 byte. Total to read: len + 1
                    // rx_buf[3] is where payload starts
                    if (HAL_UART_Receive(&huart6, &rx_buf[3], len + 1, 500) == HAL_OK) {
                         // Calc CRC over CMD, LEN, PAYLOAD
                         // Buffer for CRC calc starts at rx_buf[1] (CMD)
                         // Length is 1(CMD) + 1(LEN) + len(PAYLOAD) = len + 2
                         uint8_t calc_crc = Calculate_CRC8(&rx_buf[1], len + 2);
                         uint8_t recv_crc = rx_buf[3 + len];

                         if (calc_crc == recv_crc) {
                             if (cmd == CMD_OTA_START) {
                                 // Turn Orange (Erasing)
                                 HAL_GPIO_WritePin(GPIOK, GPIO_PIN_3, GPIO_PIN_RESET);
                                 HAL_GPIO_WritePin(GPIOD, GPIO_PIN_4, GPIO_PIN_SET);

                                 HAL_FLASH_Unlock();
                                 __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR | FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

                                 FLASH_EraseInitTypeDef EraseInitStruct;
                                 EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
                                 EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
                                 EraseInitStruct.Sector = first_sector;
                                 EraseInitStruct.NbSectors = nb_sectors;

                                 uint32_t SectorError;
                                 if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
                                     // Erase Failed
                                     tx_buf[0] = 0xAA;
                                     tx_buf[1] = CMD_NACK;
                                     tx_buf[2] = 0x00;
                                     tx_buf[3] = Calculate_CRC8(&tx_buf[1], 2);
                                     HAL_UART_Transmit(&huart6, tx_buf, 4, 100);
                                     continue;
                                 }

                                 // ACK
                                 tx_buf[0] = 0xAA;
                                 tx_buf[1] = CMD_ACK;
                                 tx_buf[2] = 0x00;
                                 tx_buf[3] = Calculate_CRC8(&tx_buf[1], 2);
                                 HAL_UART_Transmit(&huart6, tx_buf, 4, 100);

                                 current_offset = 0;
                             }
                             else if (cmd == CMD_OTA_CHUNK) {
                                 // Payload: [Offset(4)][Data...]
                                 uint32_t chunk_offset = 0;
                                 memcpy(&chunk_offset, &rx_buf[3], 4);

                                 if (chunk_offset != current_offset) {
                                     tx_buf[0] = 0xAA;
                                     tx_buf[1] = CMD_NACK;
                                     tx_buf[2] = 0x00;
                                     tx_buf[3] = Calculate_CRC8(&tx_buf[1], 2);
                                     HAL_UART_Transmit(&huart6, tx_buf, 4, 100);
                                     continue;
                                 }

                                 uint8_t *data_ptr = &rx_buf[7];
                                 uint8_t data_len = len - 4; // Minus offset bytes

                                 HAL_GPIO_TogglePin(GPIOK, GPIO_PIN_3); // Blue Toggle

                                 uint32_t address = INACTIVE_BANK_ADDR + chunk_offset;
                                 bool write_error = false;

                                 for (int i=0; i<data_len; i+=4) {
                                     uint32_t val;
                                     memcpy(&val, &data_ptr[i], 4);
                                     if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, address + i, val) != HAL_OK) {
                                         write_error = true;
                                         break;
                                     }
                                 }

                                 // Verify Write
                                 if (!write_error) {
                                     if (memcmp((void*)address, data_ptr, data_len) != 0) {
                                         write_error = true;
                                     }
                                 }

                                 if (write_error) {
                                     tx_buf[0] = 0xAA;
                                     tx_buf[1] = CMD_NACK;
                                     tx_buf[2] = 0x00;
                                     tx_buf[3] = Calculate_CRC8(&tx_buf[1], 2);
                                     HAL_UART_Transmit(&huart6, tx_buf, 4, 100);
                                 } else {
                                     current_offset += data_len;

                                     tx_buf[0] = 0xAA;
                                     tx_buf[1] = CMD_ACK;
                                     tx_buf[2] = 0x00;
                                     tx_buf[3] = Calculate_CRC8(&tx_buf[1], 2);
                                     HAL_UART_Transmit(&huart6, tx_buf, 4, 100);
                                 }
                             }
                             else if (cmd == CMD_OTA_END) {
                                 // Green LED
                                 HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_SET);
                                 HAL_Delay(1000);

                                 HAL_FLASH_OB_Unlock();
                                 if ((FLASH->OPTCR & FLASH_OPTCR_BFB2) == FLASH_OPTCR_BFB2) {
                                     FLASH->OPTCR &= ~FLASH_OPTCR_BFB2;
                                 } else {
                                     FLASH->OPTCR |= FLASH_OPTCR_BFB2;
                                 }
                                 HAL_FLASH_OB_Launch();
                             }
                         } else {
                             // CRC Mismatch
                             tx_buf[0] = 0xAA;
                             tx_buf[1] = CMD_NACK;
                             tx_buf[2] = 0x00;
                             tx_buf[3] = Calculate_CRC8(&tx_buf[1], 2);
                             HAL_UART_Transmit(&huart6, tx_buf, 4, 100);
                         }
                    }
                }
            }
        }
    }
}

void JumpToApplication(void) {
    uint32_t JumpAddress = *(__IO uint32_t*) (APP_ADDRESS + 4);
    void (*pJumpToApplication)(void) = (void (*)(void)) JumpAddress;

    HAL_UART_DeInit(&huart6);
    HAL_RCC_DeInit();
    HAL_DeInit();

    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;

    __disable_irq();
    SCB->VTOR = APP_ADDRESS;
    __set_MSP(*(__IO uint32_t*) APP_ADDRESS);

    pJumpToApplication();
}

uint8_t Calculate_CRC8(uint8_t *data, uint16_t len) {
    uint8_t crc = 0;
    for (uint16_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) crc = (crc << 1) ^ 0x31;
            else crc <<= 1;
        }
    }
    return crc;
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
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    HAL_PWREx_EnableOverDrive();

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);
}

static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOK_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_4|GPIO_PIN_5;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = GPIO_PIN_3;
    HAL_GPIO_Init(GPIOK, &GPIO_InitStruct);
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
    __HAL_RCC_USART6_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_9|GPIO_PIN_14;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF8_USART6;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    HAL_UART_Init(&huart6);
}

void SysTick_Handler(void) {
    HAL_IncTick();
}
