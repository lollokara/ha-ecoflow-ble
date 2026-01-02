#include "main.h"
#include <stdio.h>
#include <string.h>

UART_HandleTypeDef huart3; // Debug
UART_HandleTypeDef huart6; // ESP32

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_USART3_UART_Init(void);
static void MX_USART6_UART_Init(void);
static void Early_LED_Init(void);
void ProcessOTA(void);
uint8_t CheckOTATrigger(void);
void CLI_Loop(void);

// Basic stdout for printf (retarget to USART3)
int _write(int file, char *ptr, int len) {
    HAL_UART_Transmit(&huart3, (uint8_t*)ptr, len, 100);
    return len;
}

int main(void) {
    // 1. Hardware Init
    HAL_Init();
    Early_LED_Init();
    SystemClock_Config();
    MX_GPIO_Init();
    MX_USART3_UART_Init();

    printf("\n=== EcoFlow Bootloader v1.1 ===\n");
    printf("Build Date: %s %s\n", __DATE__, __TIME__);

    // 2. Check for CLI Entry (Key Press during startup)
    // Non-blocking check for 200ms
    uint8_t rx_byte;
    if (HAL_UART_Receive(&huart3, &rx_byte, 1, 200) == HAL_OK) {
        printf("Key Pressed. Entering CLI.\n");
        CLI_Loop();
    }

    // 3. Check for OTA Trigger (Backup Register)
    if (CheckOTATrigger()) {
        printf("[BL] OTA Triggered! Starting OTA Process...\n");
        HAL_GPIO_WritePin(LED_BLUE_PORT, LED_BLUE_PIN, GPIO_PIN_SET);
        MX_USART6_UART_Init();
        ProcessOTA();
        // Fallback if OTA returns
        printf("[BL] OTA Failed or Aborted.\n");
        // Loop blink red
        while(1) {
             HAL_GPIO_TogglePin(LED_RED_PORT, LED_RED_PIN);
             HAL_Delay(200);
        }
    }

    // 4. Normal Boot
    printf("[BL] Normal Boot. Checking App at 0x%08X...\n", APP_ADDRESS);

    // Check Stack Pointer (First word of App Vector Table)
    // APP_ADDRESS is 0x08008000 (Relative to Active Bank)
    uint32_t appStack = *(__IO uint32_t*)APP_ADDRESS;

    // RAM is 0x20000000 to 0x20050000
    // Allow stack to be anywhere in RAM
    if ((appStack & 0xFFF00000) == 0x20000000) {
        printf("[BL] App Valid (SP=%08X). Jumping...\n", (unsigned int)appStack);
        HAL_Delay(20); // Flush UART
        JumpToApplication();
    } else {
        printf("[BL] Invalid App Stack Pointer: 0x%08X\n", (unsigned int)appStack);
        HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_SET);

        printf("[BL] Entering Emergency CLI.\n");
        CLI_Loop();
    }

    while (1);
}

void JumpToApplication(void) {
    // 1. Disable Interrupts
    __disable_irq();

    // 2. DeInit Peripherals (Important!)
    HAL_UART_DeInit(&huart3);
    HAL_UART_DeInit(&huart6);
    HAL_RCC_DeInit();

    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL = 0;

    // 3. Set Vector Table
    SCB->VTOR = APP_ADDRESS;

    // 4. Set Stack Pointer
    __set_MSP(*(__IO uint32_t*)APP_ADDRESS);

    // 5. Jump
    uint32_t jumpAddress = *(__IO uint32_t*)(APP_ADDRESS + 4);
    void (*pJumpToApplication)(void) = (void (*)(void))jumpAddress;
    pJumpToApplication();
}

uint8_t CheckOTATrigger(void) {
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    uint32_t bkp0 = RTC->BKP0R;
    if (bkp0 == RTC_BKP_OTA_FLAG) {
        // Clear the flag
        RTC->BKP0R = 0;
        return 1;
    }
    return 0;
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
    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Handler();
    }

    if (HAL_PWREx_EnableOverDrive() != HAL_OK) {
        Error_Handler();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_GPIO_Init(void) {
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOK_CLK_ENABLE();
    __HAL_RCC_GPIOA_CLK_ENABLE();
    __HAL_RCC_GPIOB_CLK_ENABLE();

    HAL_GPIO_WritePin(GPIOG, LED_GREEN_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOD, LED_ORANGE_PIN, GPIO_PIN_RESET);
    HAL_GPIO_WritePin(GPIOD, LED_RED_PIN, GPIO_PIN_RESET);
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

static void Early_LED_Init(void) {
    RCC->AHB1ENR |= (1 << 6);
    GPIOG->MODER &= ~(3 << (6 * 2));
    GPIOG->MODER |= (1 << (6 * 2));
    GPIOG->BSRR = (1 << 6); // On
    for(volatile int i=0; i<500000; i++);
    GPIOG->BSRR = (1 << (6 + 16)); // Off
}

static void MX_USART3_UART_Init(void) {
    huart3.Instance = USART3;
    huart3.Init.BaudRate = 115200;
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart3) != HAL_OK) {
        Error_Handler();
    }
}

static void MX_USART6_UART_Init(void) {
    huart6.Instance = USART6;
    huart6.Init.BaudRate = 115200;
    huart6.Init.WordLength = UART_WORDLENGTH_8B;
    huart6.Init.StopBits = UART_STOPBITS_1;
    huart6.Init.Parity = UART_PARITY_NONE;
    huart6.Init.Mode = UART_MODE_TX_RX;
    huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart6.Init.OverSampling = UART_OVERSAMPLING_16;
    if (HAL_UART_Init(&huart6) != HAL_OK) {
        Error_Handler();
    }
}

void Error_Handler(void) {
    __disable_irq();
    while (1) {
        HAL_GPIO_TogglePin(LED_RED_PORT, LED_RED_PIN);
        HAL_Delay(100);
    }
}
