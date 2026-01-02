#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdbool.h>

// --- Definitions ---
// App is linked at 0x08008000 (Sector 2)
#define APP_ADDR 0x08008000

// Inactive Bank (Physical 0x0810xxxx)
// If BFB2=0 (Active Bank 1): Inactive is Bank 2 (0x0810xxxx)
// If BFB2=1 (Active Bank 2): Inactive is Bank 1 (0x0810xxxx due to aliasing)
// So Target Base is ALWAYS 0x08100000
#define TARGET_BANK_ADDR 0x08100000
#define TARGET_APP_ADDR  (TARGET_BANK_ADDR + 0x8000)

// UART OTA Commands
#define CMD_OTA_START 0xF0
#define CMD_OTA_DATA  0xF1
#define CMD_OTA_END   0xF2
#define CMD_OTA_ACK   0xF3
#define CMD_OTA_NACK  0xF4

// --- Globals ---
UART_HandleTypeDef huart6;
IWDG_HandleTypeDef hiwdg;

typedef void (*pFunction)(void);
pFunction JumpToApplication;
uint32_t JumpAddress;

// OTA State
uint32_t ota_total_size = 0;
uint32_t ota_written_size = 0;
uint32_t ota_crc_current = 0xFFFFFFFF;
uint32_t ota_crc_target = 0;
bool ota_active = false;

// --- Prototypes ---
void SystemClock_Config(void);
static void Error_Handler(void);
static void UART_Init(void);
static void IWDG_Init(void);
void JumpTo(uint32_t address);
void ToggleBank(void);
static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len);
static uint8_t calc_crc8(uint8_t *data, int len);
void SendAck(uint8_t cmd_id);
void SendNack(uint8_t cmd_id);
HAL_StatusTypeDef Flash_Erase_Target_App(void);
HAL_StatusTypeDef Flash_Copy_Bootloader(void);

// --- Main ---
int main(void) {
    HAL_Init();

    // Initialize LEDs immediately for diagnostics
    __HAL_RCC_GPIOG_CLK_ENABLE(); // PG6 (Green)
    __HAL_RCC_GPIOD_CLK_ENABLE(); // PD4 (Orange), PD5 (Red)
    __HAL_RCC_GPIOK_CLK_ENABLE(); // PK3 (Blue)

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // PG6 Green
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    // PD4 Orange, PD5 Red
    GPIO_InitStruct.Pin = GPIO_PIN_4 | GPIO_PIN_5;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    // PK3 Blue
    GPIO_InitStruct.Pin = GPIO_PIN_3;
    HAL_GPIO_Init(GPIOK, &GPIO_InitStruct);

    // Turn ON Red (Indicate Boot Start)
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_5, GPIO_PIN_RESET); // LED3 Red ON (Schematic says Active High? No, usually LEDs on Disco are Active High. Wait. Manual says "LD3 (red) connected to PD5". Usually driving High turns on. Let's assume High=ON for Disco boards. Previous code used WritePin(.. RESET) for ON?
    // F469 Disco User Manual: "LD1..LD4... connected to PG6, PD4, PD5, PK3".
    // Schematic: PD5 -> LED -> GND. So HIGH is ON.
    // My previous code: `HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_RESET); // ON` -> This was likely wrong if Active High.
    // Let's assume HIGH is ON.

    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_5, GPIO_PIN_SET); // Red ON
    HAL_Delay(200);

    SystemClock_Config();

    // Turn ON Orange (Indicate Clock OK)
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_4, GPIO_PIN_SET); // Orange ON
    HAL_Delay(200);

    UART_Init();

    // Turn ON Green (Indicate UART OK)
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_SET); // Green ON
    HAL_Delay(200);

    IWDG_Init();

    // Startup Sequence Complete: All ON.
    HAL_Delay(500);
    // All OFF
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_4, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_5, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOK, GPIO_PIN_3, GPIO_PIN_RESET);

    uint8_t rx_byte;
    uint8_t buffer[260];
    uint16_t buf_idx = 0;
    uint8_t expected_len = 0;
    uint32_t start_tick = HAL_GetTick();
    // Check Stack Pointer of App
    bool app_valid = (((*(__IO uint32_t*)APP_ADDR) & 0x2FFE0000) == 0x20000000);

    while (1) {
        HAL_IWDG_Refresh(&hiwdg);

        // Check Timeout if not OTA active
        if (!ota_active && (HAL_GetTick() - start_tick > 500)) {
            if (app_valid) {
                JumpTo(APP_ADDR);
            } else {
                // No App: Blink Red/Orange fast
                HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_5); // Red
                HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_4); // Orange
                HAL_Delay(100);
                continue;
            }
        }

        // Waiting for OTA: Blink Green slow
        if (!ota_active) {
             if ((HAL_GetTick() / 500) % 2) {
                 HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_SET);
             } else {
                 HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_RESET);
             }
        }

        // Poll UART
        if (HAL_UART_Receive(&huart6, &rx_byte, 1, 0) == HAL_OK) {
            if (buf_idx == 0) {
                if (rx_byte == 0xAA) buffer[buf_idx++] = rx_byte;
            } else if (buf_idx == 1) {
                buffer[buf_idx++] = rx_byte; // CMD
            } else if (buf_idx == 2) {
                buffer[buf_idx++] = rx_byte; // LEN
                expected_len = rx_byte;
            } else {
                buffer[buf_idx++] = rx_byte;
                if (buf_idx == (4 + expected_len)) {
                    // Packet Complete
                    uint8_t cmd = buffer[1];
                    uint8_t crc_rx = buffer[buf_idx-1];
                    if (calc_crc8(&buffer[1], buf_idx-2) == crc_rx) {
                        // Process CMD
                        if (cmd == CMD_OTA_START) {
                            ota_active = true;
                            // Turn ON Blue (Busy)
                            HAL_GPIO_WritePin(GPIOK, GPIO_PIN_3, GPIO_PIN_SET);

                            memcpy(&ota_total_size, &buffer[3], 4);
                            memcpy(&ota_crc_target, &buffer[7], 4);

                            // 1. Copy Bootloader to Inactive Bank (Sectors 0-1 -> Target 0-1)
                            if (Flash_Copy_Bootloader() != HAL_OK) {
                                SendNack(cmd);
                                ota_active = false;
                                HAL_GPIO_WritePin(GPIOK, GPIO_PIN_3, GPIO_PIN_RESET);
                                HAL_GPIO_WritePin(GPIOD, GPIO_PIN_5, GPIO_PIN_SET); // Red Error
                            } else {
                                // 2. Erase App Sectors in Inactive Bank
                                if (Flash_Erase_Target_App() != HAL_OK) {
                                    SendNack(cmd);
                                    ota_active = false;
                                    HAL_GPIO_WritePin(GPIOK, GPIO_PIN_3, GPIO_PIN_RESET);
                                    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_5, GPIO_PIN_SET); // Red Error
                                } else {
                                    ota_written_size = 0;
                                    ota_crc_current = 0xFFFFFFFF;
                                    SendAck(cmd);
                                }
                            }
                        } else if (cmd == CMD_OTA_DATA && ota_active) {
                            // Toggle Blue
                            HAL_GPIO_TogglePin(GPIOK, GPIO_PIN_3);

                            uint32_t offset;
                            memcpy(&offset, &buffer[3], 4);
                            uint8_t* data = &buffer[7];
                            uint16_t len = expected_len - 4; // Minus offset

                            // Write Flash
                            HAL_FLASH_Unlock();
                            for (int i=0; i<len; i+=4) {
                                uint32_t word = 0;
                                memcpy(&word, &data[i], (len-i < 4) ? (len-i) : 4);
                                if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, TARGET_APP_ADDR + offset + i, word) != HAL_OK) {
                                    HAL_FLASH_Lock();
                                    SendNack(cmd);
                                    HAL_GPIO_WritePin(GPIOD, GPIO_PIN_5, GPIO_PIN_SET); // Red Error
                                    return 0; // Error
                                }
                            }
                            HAL_FLASH_Lock();

                            ota_crc_current = crc32_update(ota_crc_current, data, len);
                            ota_written_size += len;
                            SendAck(cmd);

                        } else if (cmd == CMD_OTA_END && ota_active) {
                            uint32_t rx_crc32;
                            memcpy(&rx_crc32, &buffer[3], 4);

                            if (ota_crc_current == rx_crc32 && rx_crc32 == ota_crc_target) {
                                SendAck(cmd);
                                HAL_GPIO_WritePin(GPIOK, GPIO_PIN_3, GPIO_PIN_RESET);
                                HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_SET); // Green Success
                                HAL_Delay(100);
                                ToggleBank(); // Resets
                            } else {
                                SendNack(cmd);
                                HAL_GPIO_WritePin(GPIOD, GPIO_PIN_5, GPIO_PIN_SET); // Red Error
                            }
                        }
                    }
                    buf_idx = 0;
                }
            }
        }
    }
}

// --- Helpers ---

HAL_StatusTypeDef Flash_Copy_Bootloader(void) {
    HAL_FLASH_Unlock();
    // Inactive Bank is mapped to 0x0810xxxx.
    // Bootloader is 32KB (Sector 0: 16KB, Sector 1: 16KB).
    // Target Sectors are Sector 12 (16KB) and Sector 13 (16KB) physically?
    // If Active is Bank 1 (0x0800), Inactive is Bank 2 (0x0810). Sectors 12, 13.
    // If Active is Bank 2 (0x0800), Inactive is Bank 1 (0x0810). Sectors 0, 1.
    // HAL_FLASHEx_Erase uses SNB bits.
    // We need to determine which sectors to erase based on BFB2.

    FLASH_OBProgramInitTypeDef OBInit;
    HAL_FLASHEx_OBGetConfig(&OBInit);
    bool active_is_bank2 = (OBInit.USERConfig & FLASH_OPTCR_BFB2);

    uint32_t startSector, endSector;
    if (active_is_bank2) {
        // Active is Bank 2. Inactive is Bank 1 (Mapped to 0x0810).
        // Physical Sectors 0, 1.
        startSector = FLASH_SECTOR_0;
        endSector = FLASH_SECTOR_1;
    } else {
        // Active is Bank 1. Inactive is Bank 2 (Mapped to 0x0810).
        // Physical Sectors 12, 13.
        startSector = FLASH_SECTOR_12;
        endSector = FLASH_SECTOR_13;
    }

    FLASH_EraseInitTypeDef EraseInitStruct;
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    EraseInitStruct.NbSectors = 1;
    uint32_t SectorError = 0;

    for (uint32_t s = startSector; s <= endSector; s++) {
        HAL_IWDG_Refresh(&hiwdg);
        EraseInitStruct.Sector = s;
        if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
            HAL_FLASH_Lock();
            return HAL_ERROR;
        }
    }

    // Copy 32KB from 0x08000000 to TARGET_BANK_ADDR
    for (uint32_t i = 0; i < 0x8000; i+=4) { // 32KB
        uint32_t val = *(__IO uint32_t*)(0x08000000 + i);
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, TARGET_BANK_ADDR + i, val) != HAL_OK) {
            HAL_FLASH_Lock();
            return HAL_ERROR;
        }
    }

    HAL_FLASH_Lock();
    return HAL_OK;
}

HAL_StatusTypeDef Flash_Erase_Target_App(void) {
    // App uses Sectors 2-11 (Bank 1) or 14-23 (Bank 2).
    // Target starts at Offset 0x8000.
    // If Active Bank 2: Inactive is Bank 1. Sectors 2-11.
    // If Active Bank 1: Inactive is Bank 2. Sectors 14-23.

    FLASH_OBProgramInitTypeDef OBInit;
    HAL_FLASHEx_OBGetConfig(&OBInit);
    bool active_is_bank2 = (OBInit.USERConfig & FLASH_OPTCR_BFB2);

    uint32_t startSector, endSector;
    if (active_is_bank2) {
        startSector = FLASH_SECTOR_2;
        endSector = FLASH_SECTOR_11;
    } else {
        startSector = FLASH_SECTOR_14;
        endSector = FLASH_SECTOR_23;
    }

    HAL_FLASH_Unlock();
    FLASH_EraseInitTypeDef EraseInitStruct;
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    EraseInitStruct.NbSectors = 1;
    uint32_t SectorError = 0;

    for (uint32_t s = startSector; s <= endSector; s++) {
        HAL_IWDG_Refresh(&hiwdg);
        EraseInitStruct.Sector = s;
        if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
            HAL_FLASH_Lock();
            return HAL_ERROR;
        }
    }
    HAL_FLASH_Lock();
    return HAL_OK;
}

void ToggleBank() {
    HAL_FLASH_Unlock();
    HAL_FLASH_OB_Unlock();
    FLASH_OBProgramInitTypeDef OBInit;
    HAL_FLASHEx_OBGetConfig(&OBInit);
    OBInit.OptionType = OPTIONBYTE_USER;
    if ((OBInit.USERConfig & FLASH_OPTCR_BFB2) == FLASH_OPTCR_BFB2) {
         OBInit.USERConfig &= ~FLASH_OPTCR_BFB2;
    } else {
         OBInit.USERConfig |= FLASH_OPTCR_BFB2;
    }
    HAL_FLASHEx_OBProgram(&OBInit);
    HAL_FLASH_OB_Launch();
    HAL_FLASH_OB_Lock();
    HAL_FLASH_Lock();
}

void JumpTo(uint32_t address) {
    // Cleanup Peripherals
    HAL_UART_DeInit(&huart6);
    HAL_DeInit();

    // Disable Interrupts
    __disable_irq();

    // Set Vector Table
    SCB->VTOR = address;

    // Get Stack Pointer
    uint32_t stack_ptr = *(__IO uint32_t*)address;

    // Get Reset Handler
    JumpAddress = *(__IO uint32_t*) (address + 4);
    JumpToApplication = (pFunction) JumpAddress;

    // Set MSP
    __set_MSP(stack_ptr);

    // Jump
    JumpToApplication();
}

static void UART_Init(void) {
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

    // Explicitly disable interrupts for Polling Mode
    HAL_NVIC_DisableIRQ(USART6_IRQn);
}

static void IWDG_Init(void) {
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_256;
    hiwdg.Init.Reload = 4095; // Max timeout (~32s)
    HAL_IWDG_Init(&hiwdg);
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

static uint8_t calc_crc8(uint8_t *data, int len) {
    uint8_t crc = 0;
    for (int i = 0; i < len; i++) {
        uint8_t inbyte = data[i];
        for (uint8_t j = 0; j < 8; j++) {
            uint8_t mix = (crc ^ inbyte) & 0x01;
            crc >>= 1;
            if (mix) crc ^= 0x8C;
            inbyte >>= 1;
        }
    }
    return crc;
}

static uint32_t crc32_update(uint32_t crc, const uint8_t *data, size_t len) {
    for (size_t i = 0; i < len; i++) {
        uint8_t b = data[i];
        for (int j = 0; j < 8; j++) {
            uint32_t bit = ((b >> (7 - j)) & 1) ^ ((crc >> 31) & 1);
            crc <<= 1;
            if (bit) crc ^= 0x04C11DB7;
        }
    }
    return crc;
}

void SendAck(uint8_t cmd_id) {
    uint8_t packet[5];
    packet[0] = 0xAA;
    packet[1] = CMD_OTA_ACK;
    packet[2] = 1;
    packet[3] = cmd_id;
    packet[4] = calc_crc8(&packet[1], 3);
    HAL_UART_Transmit(&huart6, packet, 5, 100);
}

void SendNack(uint8_t cmd_id) {
    uint8_t packet[5];
    packet[0] = 0xAA;
    packet[1] = CMD_OTA_NACK;
    packet[2] = 1;
    packet[3] = cmd_id;
    packet[4] = calc_crc8(&packet[1], 3);
    HAL_UART_Transmit(&huart6, packet, 5, 100);
}

static void Error_Handler(void) { while(1); }
