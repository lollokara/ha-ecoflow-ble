#include "stm32f4xx_hal.h"

// Application Start Address (Sector 1 in Bank 1 or Offset 0x8000 in remapped Bank 2)
// Since we use hardware remapping (BFB2), the Active Bank is always mapped to 0x08000000.
// So the App is ALWAYS at 0x08008000 relative to the boot address.
#define APP_ADDRESS 0x08008000

typedef void (*pFunction)(void);
pFunction JumpToApplication;
uint32_t JumpAddress;

void SystemClock_Config(void);

int main(void) {
    HAL_Init();
    SystemClock_Config();

    // Check if Application exists at 0x08008000 (Check Stack Pointer validity)
    // The first word of the vector table is the Stack Pointer value (usually 0x20xxxxxx)
    uint32_t app_sp = *(__IO uint32_t*)APP_ADDRESS;
    if (app_sp >= 0x20000000 && app_sp <= 0x20050000) {

        // De-initialize Peripherals and Clocks to clean state
        HAL_RCC_DeInit();
        HAL_DeInit();
        SysTick->CTRL = 0;
        SysTick->LOAD = 0;
        SysTick->VAL = 0;

        // Disable all interrupts
        __disable_irq();

        // Jump to User Application
        JumpAddress = *(__IO uint32_t*) (APP_ADDRESS + 4);
        JumpToApplication = (pFunction) JumpAddress;

        // Set Vector Table Offset Register
        SCB->VTOR = APP_ADDRESS;

        // Set Main Stack Pointer
        __set_MSP(*(__IO uint32_t*) APP_ADDRESS);

        // Jump
        JumpToApplication();
    }

    // If we are here, there is no valid application.
    // Blink LED or stay in loop.
    // Initialize GPIO for LED (PG6 is Red LED on Discovery)
    __HAL_RCC_GPIOG_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    while (1) {
        HAL_GPIO_TogglePin(GPIOG, GPIO_PIN_6);
        HAL_Delay(500);
    }
}

// Minimal Clock Config for Bootloader (can be slower/simpler than App)
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
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    HAL_PWREx_EnableOverDrive();

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);
}

void SysTick_Handler(void) {
    HAL_IncTick();
}
