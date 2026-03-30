void Error_Handler(void) { while(1); }
/**
 * @file main.c
 * @author Lollokara
 * @brief Main entry point for the EcoflowSTM32F4 application.
 *
 * This file handles the low-level hardware initialization of the STM32F469I-Discovery board,
 * including:
 * - System Clock Configuration (HSE, PLL, LTDC/DSI Clocks).
 * - ESP32 Reset Control (PG10).
 * - Debug UART (USART3) Initialization.
 * - Backlight PWM (TIM2 CH4) Initialization.
 * - FreeRTOS Task Creation (Display Task, UART Task).
 * - FreeRTOS Scheduler Start.
 */

// Ensure HSE_VALUE is defined correctly before HAL inclusion
#ifndef HSE_VALUE
#define HSE_VALUE 8000000
#endif

#include "stm32h7xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "display_task.h"
#include "uart_task.h"
#include "fan_task.h"
#include "log_manager.h"
#include "ff.h"
#include "sd_diskio.h"
#include <stdio.h>

// External Handles
extern UART_HandleTypeDef huart6;

// Local Handles
UART_HandleTypeDef huart3;
TIM_HandleTypeDef htim2;
IWDG_HandleTypeDef hiwdg;
SD_HandleTypeDef hsd;
FATFS SDFatFs;
char SDPath[4] = {0};

QueueHandle_t displayQueue; // Global queue for UI events

// Forward Declarations
void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 5;
    RCC_OscInitStruct.PLL.PLLN = 160;
    RCC_OscInitStruct.PLL.PLLP = 2;
    RCC_OscInitStruct.PLL.PLLQ = 4;
    RCC_OscInitStruct.PLL.PLLR = 2;
    RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
    RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
    RCC_OscInitStruct.PLL.PLLFRACN = 0;
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
    {
      Error_Handler();
    }
    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                                |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
    RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV2;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV2;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV2;
    RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV2;
    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
    {
      Error_Handler();
    }

    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
        Error_Handler();
    }
}

/**
 * @brief SDIO Initialization.
 */
static void MX_SDIO_SD_Init(void) {
    // Initialize CD Pin (PG2)
    __HAL_RCC_GPIOG_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    // Check Card Detect
    // Assuming Active Low (Grounded when inserted)
    if (HAL_GPIO_ReadPin(GPIOG, GPIO_PIN_2) == GPIO_PIN_RESET) {
        printf("SD Card Detected\n");
    } else {
        printf("SD Card Not Detected (PG2 High)\n");
    }

    hsd.Instance = SDMMC1;
    hsd.Init.ClockEdge = SDMMC_CLOCK_EDGE_RISING;
    // hsd.Init.ClockBypass = SDIO_CLOCK_BYPASS_DISABLE;
    hsd.Init.ClockPowerSave = SDMMC_CLOCK_POWER_SAVE_DISABLE;
    hsd.Init.BusWide = SDMMC_BUS_WIDE_1B;
    hsd.Init.HardwareFlowControl = SDMMC_HARDWARE_FLOW_CONTROL_DISABLE;
    hsd.Init.ClockDiv = 4; // Slow down clock (48MHz / (4+2) = 8MHz)

    if (HAL_SD_Init(&hsd) != HAL_OK) {
        printf("SD Init Failed: %lu\n", hsd.ErrorCode);
    }

    // Force 1-bit mode to verify basic connectivity and avoid D2/D3 issues
    if (HAL_SD_ConfigWideBusOperation(&hsd, SDMMC_BUS_WIDE_1B) != HAL_OK) {
         printf("SD Bus Config Failed: %lu\n", hsd.ErrorCode);
    }
}

/**
 * @brief UART MSP Initialization.
 * Configures PB10 (TX) and PB11 (RX) for USART3.
 * Configures PA1 (RX) for UART4 and PA2 (TX) for USART2 (Split Mode).
 */
void HAL_UART_MspInit(UART_HandleTypeDef* uartHandle) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if(uartHandle->Instance==USART3) {
        __HAL_RCC_USART3_CLK_ENABLE();
        __HAL_RCC_GPIOB_CLK_ENABLE();

        // PB10 -> TX, PB11 -> RX
        GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
        HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);
    }
    else if(uartHandle->Instance==UART4) {
        __HAL_RCC_UART4_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        // PA0 -> TX, PA1 -> RX
        GPIO_InitStruct.Pin = GPIO_PIN_0|GPIO_PIN_1;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF8_UART4;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

        HAL_NVIC_SetPriority(UART4_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(UART4_IRQn);
    }
}

/**
 * @brief TIM PWM MSP Initialization.
 * Configures PA3 for TIM2 Channel 4.
 */
void HAL_TIM_PWM_MspInit(TIM_HandleTypeDef* tim_pwmHandle) {
    if(tim_pwmHandle->Instance==TIM2) {
        __HAL_RCC_TIM2_CLK_ENABLE();
        __HAL_RCC_GPIOA_CLK_ENABLE();

        // PA3 -> TIM2_CH4 (Backlight)
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin = GPIO_PIN_3;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        GPIO_InitStruct.Alternate = GPIO_AF1_TIM2;
        HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);
    }
}

/**
 * @brief SDIO MSP Initialization.
 */
void HAL_SD_MspInit(SD_HandleTypeDef *hsd) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    if(hsd->Instance==SDMMC1) {
        __HAL_RCC_SDMMC1_CLK_ENABLE();
        __HAL_RCC_GPIOC_CLK_ENABLE();
        __HAL_RCC_GPIOD_CLK_ENABLE();

        // PC8..PC12: D0..D3, CK
        GPIO_InitStruct.Pin = GPIO_PIN_8|GPIO_PIN_9|GPIO_PIN_10|GPIO_PIN_11|GPIO_PIN_12;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF12_SDIO1;
        HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

        // PD2: CMD
        GPIO_InitStruct.Pin = GPIO_PIN_2;
        GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
        GPIO_InitStruct.Pull = GPIO_PULLUP;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
        GPIO_InitStruct.Alternate = GPIO_AF12_SDIO1;
        HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

        // Enable SDIO Interrupt (Required for some SD operations and correct HAL behavior)
        HAL_NVIC_SetPriority(SDMMC1_IRQn, 5, 0);
        HAL_NVIC_EnableIRQ(SDMMC1_IRQn);
    }
}

/**
 * @brief Sets the display backlight brightness.
 * @param percent Brightness 0-100.
 */
void SetBacklight(uint8_t percent) {
    if (percent > 100) percent = 100;
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, percent);
}

/**
 * @brief Main Application Entry Point.
 */
int main(void) {
    // Relocate Vector Table to Application Address
    // Note: When booting from Bank 2 (BFB2 set), 0x08000000 is the alias for Bank 2.
    // However, for Dual Bank boot stability, using the aliased 0x00000000 base
    // plus offset ensures we point to the running bank correctly.
    // Changed to 0x00008000 as requested for stability.
    SCB->VTOR = 0x00008000;
    __DSB();

    // Enable Interrupts (Bootloader disables them)
    __enable_irq();

    HAL_Init();

    SystemClock_Config();

    // MX_USART3_UART_Init(); // Init Debug UART Early

    // Reset Boot Counter after successful boot
    // __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    RTC->BKP1R = 0;

    // ESP32_Reset_Init();

    // MX_TIM2_Init();
    SetBacklight(75); // Ensure screen is visible at boot

    // Initialize SD and Filesystem
    MX_SDIO_SD_Init();
    if (FATFS_LinkDriver(&SD_Driver, SDPath) != 0) {
        printf("FatFs Link Driver Failed\n");
    }

    // LogManager_Init() moved to StartUARTTask to ensure Scheduler/Mutex availability
    // LogManager_Write(3, "SYS", "Main: Boot Complete"); // Defer logging until init

    // MX_IWDG_Init(); // Watchdog Enabled

    // Create Display Event Queue
    displayQueue = xQueueCreate(10, sizeof(DisplayEvent));
    if (displayQueue == NULL) {
        while(1);
    }

    // Create FreeRTOS Tasks
    xTaskCreate(StartDisplayTask, "Display", 16384, NULL, 2, NULL);
    xTaskCreate(StartUARTTask, "UART", 8192, NULL, 3, NULL);
    xTaskCreate(StartFanTask, "Fan", 4096, NULL, 2, NULL);

    // Start Scheduler
    vTaskStartScheduler();

    // Should never reach here
    while (1) {}
}

/**
 * @brief FreeRTOS Tick Hook.
 * Called every tick to increment the HAL tick counter.
 */
void vApplicationTickHook(void) {
    // HAL_IncTick(); // Removed to avoid double increment as it is called in SysTick_Handler
}

/* FreeRTOS Interrupt Handlers */
extern void xPortSysTickHandler(void);
extern void vPortSVCHandler(void);
extern void xPortPendSVHandler(void);

/**
  * @brief  This function handles SVCall exception.
  * @param  None
  * @retval None
  */
void SVC_Handler(void)
{
  vPortSVCHandler();
}

/**
  * @brief  This function handles PendSVC exception.
  * @param  None
  * @retval None
  */
void PendSV_Handler(void)
{
  xPortPendSVHandler();
}

/**
  * @brief  This function handles SysTick Handler.
  * @param  None
  * @retval None
  */
void SysTick_Handler(void)
{
  HAL_IncTick();
  if (xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED)
  {
    xPortSysTickHandler();
  }
}

/**
 * @brief FreeRTOS Stack Overflow Hook.
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName) {
    printf("Stack Overflow in task: %s\n", pcTaskName);
    while(1);
}

/**
 * @brief FreeRTOS Malloc Failed Hook.
 */
void vApplicationMallocFailedHook(void) {
    printf("Malloc Failed!\n");
    while(1);
}

void WWDG_IRQHandler(void) {
    printf("WWDG_IRQHandler (IRQ 0) Triggered!\n");
    while(1);
}

/**
 * @brief SDIO Interrupt Handler.
 */
void SDIO_IRQHandler(void) {
    HAL_SD_IRQHandler(&hsd);
}
