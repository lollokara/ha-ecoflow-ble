#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdio.h>

// Handle Definitions
UART_HandleTypeDef huart3; // Debug
UART_HandleTypeDef huart6; // ESP32
IWDG_HandleTypeDef hiwdg;  // Watchdog

// LED Pins
#define LED_GREEN_PIN  GPIO_PIN_6
#define LED_GREEN_PORT GPIOG
#define LED_ORANGE_PIN GPIO_PIN_4
#define LED_ORANGE_PORT GPIOD
#define LED_RED_PIN    GPIO_PIN_5
#define LED_RED_PORT   GPIOD
#define LED_BLUE_PIN   GPIO_PIN_3
#define LED_BLUE_PORT  GPIOK

// Application Address
#define APP_ADDRESS 0x08008000

// Function Prototypes
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_USART6_UART_Init(void);
static void MX_IWDG_Init(void);
static void JumpToApplication(void);
static int CheckUpdateTrigger(void);
void ProcessCLI(void);
void ProcessOTAMessage(void);
void LED_SetStatus(int status); // 0=OK, 1=OTA, 2=Error, 3=Activity

int main(void) {
    HAL_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART3_UART_Init();
    MX_IWDG_Init(); // Init Watchdog (10s)

    // Early Debug Message
    char msg[] = "Bootloader Started\r\n";
    HAL_UART_Transmit(&huart3, (uint8_t*)msg, strlen(msg), 100);

    // Refresh WDT
    HAL_IWDG_Refresh(&hiwdg);

    // Check Trigger
    uint32_t app_sp = *(__IO uint32_t*)APP_ADDRESS;
    int app_valid = (app_sp >= 0x20000000 && app_sp <= 0x20050000);
    int triggered = CheckUpdateTrigger();

    if (triggered || !app_valid) {
        char *reason = app_valid ? "Triggered" : "No Valid App";
        printf("Entering OTA Mode (%s)\r\n", reason);
        MX_USART6_UART_Init();
        // Call ProcessOTAMessage loop instead of Echo Loop
        ProcessOTAMessage();
    } else {
        printf("Jumping to App at 0x%08X (Press key for CLI...)\r\n", APP_ADDRESS);

        // Give 1 second for CLI entry
        uint32_t start = HAL_GetTick();
        int cli_mode = 0;
        while(HAL_GetTick() - start < 1000) {
            uint8_t c;
            if(HAL_UART_Receive(&huart3, &c, 1, 0) == HAL_OK) {
                cli_mode = 1;
                break;
            }
        }

        if (cli_mode) {
             printf("CLI Mode Active.\r\n");
             while(1) {
                 ProcessCLI();
                 HAL_IWDG_Refresh(&hiwdg);
             }
        }

        JumpToApplication();
    }
}

// Check Backup Register DR0 for Magic Value 0xDEADBEEF
static int CheckUpdateTrigger(void) {
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    __HAL_RCC_RTC_ENABLE();

    // Read RTC Backup Register 0
    // Note: F469 uses RTC_BKP_DR0. HAL_RTCEx_BKUPRead required an RTC handle usually,
    // but we can access register directly: RTC->BKP0R
    // Wait, on F4, it's RTC_BKP0R

    if (RTC->BKP0R == 0xDEADBEEF) {
        // Clear it so we don't loop forever if OTA fails/reboots
        RTC->BKP0R = 0x00000000;
        return 1;
    }
    return 0;
}

static void JumpToApplication(void) {
    uint32_t JumpAddress = *(__IO uint32_t*) (APP_ADDRESS + 4);
    void (*pJumpToApplication)(void) = (void (*)(void)) JumpAddress;

    // DeInit Logic
    HAL_UART_DeInit(&huart3);
    HAL_RCC_DeInit();
    HAL_DeInit();

    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;

    __disable_irq();

    // Set Stack Pointer
    __set_MSP(*(__IO uint32_t*) APP_ADDRESS);

    // Relocate Vector Table
    SCB->VTOR = APP_ADDRESS;

    pJumpToApplication();
}

// EnterOTAMode removed (logic moved to ProcessOTAMessage in ota.c)

void LED_SetStatus(int status) {
    // 0 = OK (Green Solid)
    // 1 = OTA Active (Blue Solid)
    // 2 = Error (Red Solid)
    // 3 = Activity (Orange Toggle)

    if (status == 0) {
        HAL_GPIO_WritePin(GPIOG, LED_GREEN_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOK, LED_BLUE_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOD, LED_RED_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOD, LED_ORANGE_PIN, GPIO_PIN_RESET);
    } else if (status == 1) {
        HAL_GPIO_WritePin(GPIOG, LED_GREEN_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOK, LED_BLUE_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOD, LED_RED_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOD, LED_ORANGE_PIN, GPIO_PIN_RESET);
    } else if (status == 2) {
        HAL_GPIO_WritePin(GPIOG, LED_GREEN_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOK, LED_BLUE_PIN, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(GPIOD, LED_RED_PIN, GPIO_PIN_SET);
        HAL_GPIO_WritePin(GPIOD, LED_ORANGE_PIN, GPIO_PIN_RESET);
    } else if (status == 3) {
        HAL_GPIO_TogglePin(GPIOD, LED_ORANGE_PIN);
    }
}

// BOILERPLATE INIT CODE
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

static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOK_CLK_ENABLE();

    // LEDs
    HAL_GPIO_WritePin(GPIOG, LED_GREEN_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOD, LED_ORANGE_PIN|LED_RED_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOK, LED_BLUE_PIN, GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = LED_GREEN_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LED_ORANGE_PIN|LED_RED_PIN;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LED_BLUE_PIN;
    HAL_GPIO_Init(GPIOK, &GPIO_InitStruct);
}

static void MX_USART3_UART_Init(void) {
    __HAL_RCC_USART3_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    huart3.Instance = USART3;
    huart3.Init.BaudRate = 115200;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart3);
}

static void MX_USART6_UART_Init(void) {
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

static void MX_IWDG_Init(void) {
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_256;
    hiwdg.Init.Reload = 1250; // ~10s
    if (HAL_IWDG_Init(&hiwdg) != HAL_OK) {
        // Error
    }
}

// Retarget printf
int _write(int file, char *ptr, int len) {
    HAL_UART_Transmit(&huart3, (uint8_t*)ptr, len, 1000);
    return len;
}
