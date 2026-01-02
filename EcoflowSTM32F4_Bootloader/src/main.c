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

UART_HandleTypeDef huart6;

typedef void (*pFunction)(void);
pFunction JumpToApplication;
uint32_t JumpAddress;

void SystemClock_Config(void);
void UART_Init(void);
void Bootloader_OTA_Loop(void);

// Helper to de-initialize peripherals and interrupts
void DeInit(void) {
    HAL_UART_DeInit(&huart6); // De-init UART
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

int main(void) {
    HAL_Init();
    SystemClock_Config();
    UART_Init();

    // Check if valid stack pointer at App Address
    // Valid Range: 0x20000000 - 0x20050000 (320KB RAM)
    uint32_t sp = *(__IO uint32_t*)APP_ADDRESS;

    // Check if Force OTA Pin (e.g. User Button PA0) is pressed?
    // Let's assume if SP is invalid, we go to OTA.
    // Or if we wait 1 sec and receive "OTA START".

    // Simple logic: Wait 500ms for OTA START. If not, try to boot.
    // Ideally: If no valid app, wait forever.

    bool valid_app = ((sp & 0x2FFE0000) == 0x20000000);

    if (valid_app) {
        // Try to check for OTA Entry Packet for 500ms?
        // Blocking read with timeout?
        uint8_t buf[1];
        if (HAL_UART_Receive(&huart6, buf, 1, 100) == HAL_OK) { // 100ms wait
            if (buf[0] == START_BYTE) {
                // Potential OTA start, enter loop
                Bootloader_OTA_Loop();
            }
        }

        DeInit();
        JumpAddress = *(__IO uint32_t*) (APP_ADDRESS + 4);
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
}

void send_nack() {
    uint8_t buf[4] = {START_BYTE, CMD_OTA_NACK, 0, 0};
    buf[3] = calculate_crc8(&buf[1], 2);
    HAL_UART_Transmit(&huart6, buf, 4, 100);
}

void Bootloader_OTA_Loop(void) {
    // Protocol: [START][CMD][LEN][PAYLOAD][CRC]
    uint8_t header[3]; // START, CMD, LEN
    uint8_t payload[256];

    // Unlock Flash
    HAL_FLASH_Unlock();

    while(1) {
        // 1. Wait for START
        uint8_t b;
        if (HAL_UART_Receive(&huart6, &b, 1, HAL_MAX_DELAY) != HAL_OK) continue;
        if (b != START_BYTE) continue;

        // 2. Read CMD, LEN
        if (HAL_UART_Receive(&huart6, &header[1], 2, 100) != HAL_OK) continue;
        uint8_t cmd = header[1];
        uint8_t len = header[2];

        // 3. Read Payload + CRC
        if (HAL_UART_Receive(&huart6, payload, len + 1, 500) != HAL_OK) continue; // +1 for CRC

        // 4. Verify CRC
        // CRC is over CMD, LEN, PAYLOAD
        // We construct a temp buffer to calc CRC
        // Or calc incrementally. Simpler to put header[1..2] + payload in one buf?
        // Optimization: Calc CRC on the fly or just use payload buf with offset.

        // Checksum verification
        // Checksum byte is payload[len]
        uint8_t recv_crc = payload[len];
        // Calc CRC over CMD, LEN, PAYLOAD
        uint8_t check_buf[300];
        check_buf[0] = cmd;
        check_buf[1] = len;
        memcpy(&check_buf[2], payload, len);

        if (calculate_crc8(check_buf, 2 + len) != recv_crc) {
            send_nack();
            continue;
        }

        // 5. Handle Command
        if (cmd == CMD_OTA_START) {
            // Erase Application Sectors (2 to 23? Or just bank 1?)
            // We assume simple single bank update for recovery
            // Sectors 2 to 11 (Bank 1) usually.
            // Or mass erase Bank 1?
            // Let's erase Sectors 2-11 (up to 1MB)
            FLASH_EraseInitTypeDef EraseInitStruct;
            uint32_t SectorError;
            EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
            EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
            EraseInitStruct.Sector = FLASH_SECTOR_2;
            EraseInitStruct.NbSectors = 10; // 2 to 11

            if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) == HAL_OK) {
                send_ack();
            } else {
                send_nack();
            }
        }
        else if (cmd == CMD_OTA_CHUNK) {
            // Payload: [OFFSET(4)][DATA(N)]
            uint32_t offset;
            memcpy(&offset, payload, 4);
            uint8_t *data = &payload[4];
            uint32_t data_len = len - 4;

            uint32_t addr = APP_ADDRESS + offset;
            bool ok = true;
            for (uint32_t i=0; i<data_len; i+=4) {
                uint32_t word;
                memcpy(&word, &data[i], 4);
                if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i, word) != HAL_OK) {
                    ok = false; break;
                }
            }
            if (ok) send_ack(); else send_nack();
        }
        else if (cmd == CMD_OTA_END) {
            send_ack();
        }
        else if (cmd == CMD_OTA_APPLY) {
            send_ack();
            HAL_Delay(100);
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
  HAL_RCC_OscConfig(&RCC_OscInitStruct);

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
