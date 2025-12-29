#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "display_task.h"
#include "uart_task.h"

extern UART_HandleTypeDef huart1;

// System Clock Configuration
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
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        // Error_Handler();
    }

    if (HAL_PWREx_EnableOverDrive() != HAL_OK) {
        // Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        // Error_Handler();
    }
}

void USART1_IRQHandler(void) {
    HAL_UART_IRQHandler(&huart1);
}

// Map FreeRTOS interrupts
// SysTick_Handler is mapped in FreeRTOSConfig.h to xPortSysTickHandler
// We need to implement SysTick_Handler to call HAL_IncTick if we want HAL delay to work,
// but FreeRTOS xPortSysTickHandler does not call HAL_IncTick.
// The BOJIT library might handle this differently, but standard practice:
// Define xPortSysTickHandler in FreeRTOSConfig.h as SysTick_Handler
// and then implementing it to call both is tricky because xPortSysTickHandler is defined in port.c.
//
// Actually, usually we define configOVERRIDE_DEFAULT_TICK_CONFIGURATION and provide vPortSetupTimerInterrupt.
// Or simpler: Use a different timer for HAL.
//
// BUT, looking at BOJIT repo, it seems to wrap standard FreeRTOS.
// If I defined `#define xPortSysTickHandler SysTick_Handler` in FreeRTOSConfig.h,
// then the function `SysTick_Handler` will be provided by FreeRTOS port.c.
// So I cannot define `SysTick_Handler` here in main.c, otherwise: multiple definition.
//
// If I don't define it in main.c, HAL_IncTick won't be called. HAL_Delay will hang.
//
// Solution:
// 1. Don't use HAL_Delay in tasks (use vTaskDelay).
// 2. For initialization before scheduler, HAL_Delay is needed.
//    But before scheduler, SysTick is not hooked by FreeRTOS yet (vTaskStartScheduler configures it).
//    So HAL_Delay works fine during init.
//    Once scheduler starts, SysTick calls xPortSysTickHandler. HAL_Tick stops incrementing.
//    HAL functions with timeout called FROM tasks might fail if they rely on uwTick.
//    (e.g. HAL_UART_Transmit with timeout).
//
//    To fix this properly, we should use a hook or modify FreeRTOSConfig.h to NOT map SysTick_Handler,
//    and instead call xPortSysTickHandler FROM our own SysTick_Handler.
//
//    Let's modify FreeRTOSConfig.h in a separate step if needed.
//    For now, I'll rely on the fact that `xPortSysTickHandler` is likely `SysTick_Handler` in the library.
//    Wait, I added `#define xPortSysTickHandler SysTick_Handler` in my config.
//    So `port.c` will define `SysTick_Handler`.
//    I must NOT define `SysTick_Handler` here.
//
//    However, I need HAL_IncTick to be called.
//    FreeRTOS often provides `vApplicationTickHook`.
//    I can enable `configUSE_TICK_HOOK` and call `HAL_IncTick()` there.
//
//    Let's check my config: `#define configUSE_TICK_HOOK 0`.
//    I should change that to 1.
//
//    Let's update main.c to NOT include SysTick_Handler, and assume I will fix the hook.

int main(void) {
    HAL_Init();
    SystemClock_Config();

    // Create Tasks
    xTaskCreate(StartDisplayTask, "Display", 1024, NULL, 2, NULL);
    xTaskCreate(StartUARTTask, "UART", 256, NULL, 3, NULL);

    // Start Scheduler
    vTaskStartScheduler();

    // Should never reach here
    while (1) {}
}

// Tick Hook to keep HAL happy
void vApplicationTickHook(void) {
    HAL_IncTick();
}

// Stack Overflow Hook
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    while(1);
}

// Malloc Failed Hook
void vApplicationMallocFailedHook(void) {
    while(1);
}
