#include "stm32f4xx_hal.h"
#include "boot_shared.h"

// Global Variables
typedef void (*pFunction)(void);
pFunction JumpToApplication;
uint32_t JumpAddress;

// Function Prototypes
void SystemClock_Config(void);
static void Error_Handler(void);
void JumpTo(uint32_t address);
void WriteConfig(BootConfig* cfg);
void ReadConfig(BootConfig* cfg);
uint32_t CalculateCRC(uint32_t address, uint32_t size);
void ToggleBank();

int main(void) {
    HAL_Init();
    SystemClock_Config();

    // Enable GPIO/Flash Clocks
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_FLASH_CLK_ENABLE();

    // LED Init (PG6)
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    // Read Config
    BootConfig cfg;
    ReadConfig(&cfg);

    // Check Update Pending
    if (cfg.magic == CONFIG_MAGIC && cfg.update_pending) {
        // We verify the Inactive Bank (APP_B_ADDR = 0x08108000)
        // APP_B_ADDR includes the 0x8000 offset.
        if (CalculateCRC(APP_B_ADDR, cfg.image_size) == cfg.image_crc) {
             // Valid!
             cfg.update_pending = 0;
             cfg.active_bank = (cfg.active_bank == 0) ? 1 : 0;
             WriteConfig(&cfg);
             ToggleBank();
        } else {
             // Invalid
             cfg.update_pending = 0;
             WriteConfig(&cfg);
        }
    }

    // Default Boot: Jump to 0x08008000 (Active Bank + Offset)
    if (((*(__IO uint32_t*)APP_A_ADDR) & 0x2FFE0000) == 0x20000000) {
        JumpTo(APP_A_ADDR);
    } else {
        while(1) {
            HAL_GPIO_TogglePin(GPIOG, GPIO_PIN_6);
            HAL_Delay(100);
        }
    }
}

uint32_t CalculateCRC(uint32_t address, uint32_t size) {
    uint32_t crc = 0xFFFFFFFF;
    for (uint32_t i = 0; i < size; i++) {
        uint8_t b = *(__IO uint8_t*)(address + i);
        for (int j = 0; j < 8; j++) {
            uint32_t bit = ((b >> (7-j)) & 1) ^ ((crc >> 31) & 1);
            crc <<= 1;
            if (bit) crc ^= 0x04C11DB7;
        }
    }
    return crc;
}

void ToggleBank() {
    HAL_FLASH_Unlock();
    HAL_FLASH_OB_Unlock();
    FLASH_OBProgramInitTypeDef OBInit;
    HAL_FLASHEx_OBGetConfig(&OBInit);
    OBInit.OptionType = OPTIONBYTE_USER;
    if ((OBInit.USERConfig & FLASH_OPTCR_BFB2) == FLASH_OPTCR_BFB2) {
         OBInit.USERConfig &= ~FLASH_OPTCR_BFB2; // Disable
    } else {
         OBInit.USERConfig |= FLASH_OPTCR_BFB2; // Enable
    }
    HAL_FLASHEx_OBProgram(&OBInit);
    HAL_FLASH_OB_Launch();
    HAL_FLASH_OB_Lock();
    HAL_FLASH_Lock();
    HAL_NVIC_SystemReset();
}

void ReadConfig(BootConfig* cfg) {
    // Always read from 0x08104000 (Inactive Bank Alias)
    uint32_t* p = (uint32_t*)CONFIG_B_ADDR;
    cfg->magic = p[0];
    cfg->active_bank = p[1];
    cfg->update_pending = p[2];
    cfg->target_bank = p[3];
    cfg->crc32 = p[4];
    cfg->image_size = p[5];
    cfg->image_crc = p[6];
}

void WriteConfig(BootConfig* cfg) {
    HAL_FLASH_Unlock();

    // Determine sector based on BFB2
    FLASH_OBProgramInitTypeDef OBInit;
    HAL_FLASHEx_OBGetConfig(&OBInit);
    uint32_t sector;

    // If BFB2 is Set, Active is Bank 2 (mapped to 0x0800). Inactive is Bank 1 (mapped to 0x0810).
    // Bank 1 is Sectors 0-11. Config A is Sector 1.
    // If BFB2 is Reset, Active is Bank 1. Inactive is Bank 2 (Sectors 12-23). Config B is Sector 13.
    if ((OBInit.USERConfig & FLASH_OPTCR_BFB2) == FLASH_OPTCR_BFB2) {
         sector = FLASH_SECTOR_1;
    } else {
         sector = FLASH_SECTOR_13;
    }

    FLASH_EraseInitTypeDef EraseInitStruct;
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    EraseInitStruct.Sector = sector;
    EraseInitStruct.NbSectors = 1;
    uint32_t SectorError = 0;

    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) == HAL_OK) {
        // Write to CONFIG_B_ADDR (0x08104000)
        uint32_t addr = CONFIG_B_ADDR;
        uint32_t* p = (uint32_t*)cfg;
        for (int i = 0; i < sizeof(BootConfig)/4; i++) {
            HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i*4, p[i]);
        }
    }
    HAL_FLASH_Lock();
}

void JumpTo(uint32_t address) {
    // Disable all interrupts
    __disable_irq();

    // Set Vector Table
    SCB->VTOR = address;

    // Get Stack Pointer (First 32-bit word)
    uint32_t stack_ptr = *(__IO uint32_t*)address;

    // Get Reset Handler (Second 32-bit word)
    JumpAddress = *(__IO uint32_t*) (address + 4);
    JumpToApplication = (pFunction) JumpAddress;

    // Set MSP
    __set_MSP(stack_ptr);

    // Jump
    JumpToApplication();
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

static void Error_Handler(void) { while(1); }
