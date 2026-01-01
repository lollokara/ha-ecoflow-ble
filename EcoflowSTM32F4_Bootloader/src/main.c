#include "stm32f4xx_hal.h"
#include "boot_shared.h"
#include <string.h>

/*
 * Bootloader Logic:
 * 1. Initialize HAL and Flash.
 * 2. Check Config Sector (0x08004000).
 * 3. If Magic == UPDATE_PENDING:
 *    a. Validate Checksum of Bank 2 (0x08100000).
 *    b. Erase Bank 1 (App A).
 *    c. Copy Bank 2 -> Bank 1.
 *    d. Erase Config Sector (Clear Magic).
 * 4. Jump to App A (0x08008000).
 */

typedef void (*pFunction)(void);
pFunction JumpToApplication;
uint32_t JumpAddress;

void SystemClock_Config(void);
void Error_Handler(void);

// STM32 Hardware Compatible CRC32
// Matches App/ESP32 implementation
uint32_t CalculateCRC32(uint32_t *pData, uint32_t len) {
    uint32_t crc = 0xFFFFFFFF;
    // len is in bytes, loop over words
    uint32_t n_words = len / 4;

    for (uint32_t i = 0; i < n_words; i++) {
        uint32_t val = pData[i];
        // pData is word aligned in flash, so direct access is fine

        crc ^= val;
        for (int j = 0; j < 32; j++) {
            if (crc & 0x80000000) crc = (crc << 1) ^ 0x04C11DB7;
            else crc <<= 1;
        }
    }
    return crc;
}

void PerformUpdate(BootConfig *cfg) {
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError;

    HAL_FLASH_Unlock();

    // 1. Validate Source (Bank 2)
    uint32_t calcCrc = CalculateCRC32((uint32_t*)ADDR_APP_B, cfg->size);
    if (calcCrc != cfg->checksum) {
        // Validation Failed!
        // Do not update. Clear flag so we don't loop forever?
        // Or keep flag and do nothing, relying on WDT reset loop to eventually manual recovery?
        // Better: Clear flag to allow booting old app if valid.

        // Erase Config
        EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
        EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
        EraseInitStruct.Sector = FLASH_SECTOR_1;
        EraseInitStruct.NbSectors = 1;
        HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError);
        HAL_FLASH_Lock();
        return;
    }

    // 2. Erase Bank 1 (App A: Sectors 2-11)
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    EraseInitStruct.Sector = FLASH_SECTOR_2;
    EraseInitStruct.NbSectors = 10; // 2 to 11
    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
        Error_Handler();
    }

    // 3. Copy Bank 2 -> Bank 1
    uint32_t *pSrc = (uint32_t *)ADDR_APP_B;
    uint32_t destAddr = ADDR_APP_A;
    uint32_t lenWords = cfg->size / 4 + ((cfg->size % 4) ? 1 : 0);

    for (uint32_t i = 0; i < lenWords; i++) {
        if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, destAddr, *pSrc) != HAL_OK) {
            Error_Handler();
        }
        destAddr += 4;
        pSrc++;
    }

    // 4. Clear Config
    EraseInitStruct.Sector = FLASH_SECTOR_1;
    EraseInitStruct.NbSectors = 1;
    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
        Error_Handler();
    }

    HAL_FLASH_Lock();
}

static void LED_Init(void) {
    __HAL_RCC_GPIOG_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_6; // LED4 (Red)
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
}

int main(void) {
    HAL_Init();
    SystemClock_Config();
    LED_Init();

    BootConfig *cfg = (BootConfig *)ADDR_CONFIG;

    if (cfg->magic == BOOT_MAGIC_UPDATE_PENDING) {
        // Blink fast to indicate update start
        for(int i=0; i<5; i++) {
            HAL_GPIO_TogglePin(GPIOG, GPIO_PIN_6);
            HAL_Delay(100);
        }

        PerformUpdate(cfg);
        NVIC_SystemReset(); // Reset to clean state after update
    }

    // Check if valid app exists at ADDR_APP_A (Check Stack Pointer)
    // RAM is 0x20000000 - 0x20050000. Mask 0xFFF00000 covers 0x200xxxxx range correctly.
    if (((*(__IO uint32_t *)ADDR_APP_A) & 0xFFF00000) == 0x20000000) {
        // Jump to Application
        JumpAddress = *(__IO uint32_t *)(ADDR_APP_A + 4);
        JumpToApplication = (pFunction)JumpAddress;

        // DeInit
        HAL_RCC_DeInit();
        HAL_DeInit();
        SysTick->CTRL = 0;
        SysTick->LOAD = 0;
        SysTick->VAL = 0;

        // Set Vector Table (App should do this too, but good practice)
        SCB->VTOR = ADDR_APP_A;

        __set_MSP(*(__IO uint32_t *)ADDR_APP_A);
        JumpToApplication();
    }

    // No App found? Blink LED (Heartbeat)
    while (1) {
        HAL_GPIO_TogglePin(GPIOG, GPIO_PIN_6);
        HAL_Delay(500);
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
    RCC_OscInitStruct.PLL.PLLN = 360; // 180MHz
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    RCC_OscInitStruct.PLL.PLLR = 2; // For F469
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) Error_Handler();

    if (HAL_PWREx_EnableOverDrive() != HAL_OK) Error_Handler();

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) Error_Handler();
}

void Error_Handler(void) {
    while(1) {}
}
