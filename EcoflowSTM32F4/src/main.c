#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "display_task.h"
#include "uart_task.h"
#include <stdio.h>

extern UART_HandleTypeDef huart6;
UART_HandleTypeDef huart3;
TIM_HandleTypeDef htim2;
QueueHandle_t displayQueue;

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

    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};
    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_LTDC;
    PeriphClkInitStruct.PLLSAI.PLLSAIN = 384;
    PeriphClkInitStruct.PLLSAI.PLLSAIP = RCC_PLLSAIP_DIV2;
    PeriphClkInitStruct.PLLSAI.PLLSAIR = 7;
    PeriphClkInitStruct.PLLSAIDivR = RCC_PLLSAIDIVR_2;
    if (HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK) {
       // Error_Handler();
    }
}

static void ESP32_Reset_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // Enable GPIOG Clock
    __HAL_RCC_GPIOG_CLK_ENABLE();

    // Set PG6 High (LED OFF) and PG10 Low (ESP32 Reset Active)
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_SET);
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_10, GPIO_PIN_RESET);

    // Configure PG6 and PG10
    GPIO_InitStruct.Pin = GPIO_PIN_6 | GPIO_PIN_10;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    // Wait 1s
    HAL_Delay(1000);

    // Release ESP32 Reset (Set PG10 High)
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_10, GPIO_PIN_SET);
}

static void MX_USART3_UART_Init(void) {
    huart3.Instance = USART3;
    huart3.Init.BaudRate = 115200;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX; // Disable RX to prevent noise interrupts
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart3);

    // Explicitly disable interrupts for debug UART as we only use polling
    HAL_NVIC_DisableIRQ(USART3_IRQn);
}

static void MX_TIM2_Init(void) {
    TIM_MasterConfigTypeDef sMasterConfig = {0};
    TIM_OC_InitTypeDef sConfigOC = {0};

    htim2.Instance = TIM2;
    // Prescaler to get around 1kHz frequency
    // Clock is APB1 * 2 = 90MHz (approx, based on PLLN=360/M=8*P=2 => SYS=180, APB1=45 => TIM2=90MHz)
    // 90,000,000 / 1000 = 90000.
    // Period = 100 (for % logic) -> Prescaler = 900.
    htim2.Init.Prescaler = 900 - 1;
    htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
    htim2.Init.Period = 100 - 1; // 0-99
    htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
    htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
    if (HAL_TIM_PWM_Init(&htim2) != HAL_OK) {
        // Error
    }

    sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
    sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
    if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK) {
        // Error
    }

    sConfigOC.OCMode = TIM_OCMODE_PWM1;
    sConfigOC.Pulse = 50; // Start at 50% to verify PWM
    sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
    sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
    if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_4) != HAL_OK) {
        // Error
    }

    HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_4);
}

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
}

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

// Exposed to UI Task
void SetBacklight(uint8_t percent) {
    if (percent > 100) percent = 100;
    __HAL_TIM_SET_COMPARE(&htim2, TIM_CHANNEL_4, percent);
}

int main(void) {
    HAL_Init();
    SystemClock_Config();
    ESP32_Reset_Init();

    MX_USART3_UART_Init();
    MX_TIM2_Init();

    // Create Tasks
    displayQueue = xQueueCreate(10, sizeof(DisplayEvent));
    if (displayQueue == NULL) {
        printf("Display Queue Creation Failed!\n");
        while(1);
    }

    xTaskCreate(StartDisplayTask, "Display", 8192, NULL, 2, NULL);
    xTaskCreate(StartUARTTask, "UART", 4096, NULL, 3, NULL);

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
    printf("Stack Overflow in task: %s\n", pcTaskName);
    while(1);
}

// Malloc Failed Hook
void vApplicationMallocFailedHook(void) {
    printf("Malloc Failed!\n");
    while(1);
}

void WWDG_IRQHandler(void) {
    printf("WWDG_IRQHandler (IRQ 0) Triggered!\n");
    while(1);
}
