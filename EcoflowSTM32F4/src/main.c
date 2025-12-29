<<<<<<< HEAD
#include "main.h"
#include "FreeRTOS.h"
#include "task.h"

/* UART Handle */
UART_HandleTypeDef huart1;

/* Private function prototypes */
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART1_UART_Init(void);
static void Backlight_Init(void);

/* Task Handle */
// Task handles will be defined in app_tasks.c or here if simple

int main(void)
{
  /* MCU Configuration */
  HAL_Init();

  /* Configure the system clock */
  SystemClock_Config();

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  Backlight_Init(); /* Turn on backlight early */
  
  /* Initialize BSP LCD */
  if (BSP_LCD_Init() != LCD_OK)
  {
    Error_Handler();
  }
  
  /* Initialize LCD Layer 0 */
  BSP_LCD_LayerDefaultInit(0, LCD_FB_START_ADDRESS);
  BSP_LCD_SelectLayer(0);
  BSP_LCD_Clear(LCD_COLOR_BLACK);
  BSP_LCD_DisplayOn();
  
  /* Initialize BSP Touch Screen */
  if (BSP_TS_Init(BSP_LCD_GetXSize(), BSP_LCD_GetYSize()) != TS_OK)
  {
    /* Touch init failed - proceed anyway for display */
  }

  /* Initialize UART */
  MX_USART1_UART_Init();

  /* Create Tasks (Placeholder for next steps) */
  // xTaskCreate(...);

  /* Start scheduler */
  // vTaskStartScheduler();

  /* We need to hook into the app tasks in the next step. 
     For now, infinite loop to prevent crash if scheduler not started yet */
  while (1)
  {
  }
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 360;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  RCC_OscInitStruct.PLL.PLLR = 6;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Activate the Over-Drive mode
  */
  if (HAL_PWREx_EnableOverDrive() != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_USART1_UART_Init(void)
{
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 460800;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
}

static void MX_GPIO_Init(void)
{
  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();
  __HAL_RCC_GPIOH_CLK_ENABLE(); /* SDRAM/LCD pins often on H/I/J */
  // ... other clocks enabled by BSP Init
}

static void Backlight_Init(void)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
=======
#include "stm32f4xx_hal.h"
#include "FreeRTOS.h"
#include "task.h"
#include "display_task.h"
#include "uart_task.h"

extern UART_HandleTypeDef huart1;
>>>>>>> stm32-display-freertos-2546555926341226134

  /* Configure GPIO pin : PA3 - Backlight Control */
  GPIO_InitStruct.Pin = GPIO_PIN_3;
  GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
  HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

  /* Turn on Backlight */
  HAL_GPIO_WritePin(GPIOA, GPIO_PIN_3, GPIO_PIN_SET);
}

<<<<<<< HEAD
void Error_Handler(void)
{
  while(1)
  {
    /* Blink LED or stay here */
  }
}

/* Time base for FreeRTOS */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  if (htim->Instance == TIM6) {
    HAL_IncTick();
  }
=======
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
>>>>>>> stm32-display-freertos-2546555926341226134
}
