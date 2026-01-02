#include "stm32f4xx_hal.h"
#include "ota_core.h"
#include <string.h>

/*
 * Bootloader Logic (Polling Mode):
 * 1. Init System.
 * 2. Init UART6 (for ESP32).
 * 3. Wait 2 seconds for OTA Command.
 * 4. If OTA: Enter OTA Loop (Receive -> Flash).
 * 5. If Timeout: Jump to App (0x08020000).
 */

#define APP_ADDR 0x08020000

typedef void (*pFunction)(void);
pFunction JumpToApplication;
uint32_t JumpAddress;

UART_HandleTypeDef huart6;
IWDG_HandleTypeDef hiwdg;

void SystemClock_Config(void);
void Error_Handler(void);
static void MX_IWDG_Init(void);

// Simple busy-wait delay for early init (before SysTick/HAL)
static void SafeDelay(volatile uint32_t count) {
    while(count--) { __asm("nop"); }
}

// Early LED Initialization using Direct Register Access
// Blinks PG6 (Green) to confirm CPU is alive before HAL/Clock Init
static void Early_LED_Init_And_Blink(void) {
    // Enable GPIOG Clock (RCC_AHB1ENR_GPIOGEN = Bit 6)
    RCC->AHB1ENR |= (1 << 6);

    // Dummy read to ensure clock propagation
    volatile uint32_t tmpreg = RCC->AHB1ENR;
    (void)tmpreg;

    // Configure PG6 as Output (MODER bits 12-13 = 01)
    GPIOG->MODER &= ~(3U << 12); // Clear
    GPIOG->MODER |= (1U << 12);  // Set to Output

    // Reset OTYPER (Push-Pull), OSPEEDR (Low), PUPDR (No Pull) for PG6
    GPIOG->OTYPER &= ~(1U << 6);
    GPIOG->OSPEEDR &= ~(3U << 12);
    GPIOG->PUPDR &= ~(3U << 12);

    // Rapid Blink Sequence (5 times)
    // HSI is 16MHz. Loop is ~4 cycles. 500,000 loops ~= 0.125s
    for(int i=0; i<5; i++) {
        GPIOG->ODR |= (1 << 6); // ON
        SafeDelay(500000);
        GPIOG->ODR &= ~(1 << 6); // OFF
        SafeDelay(500000);
    }
}

// SOS Blink Pattern for Critical Errors (Clock Failure)
static void Error_Blink(void) {
    // Ensure LED is Configured
    RCC->AHB1ENR |= (1 << 6);
    GPIOG->MODER &= ~(3U << 12);
    GPIOG->MODER |= (1U << 12);

    while(1) {
        // S (...)
        for(int i=0; i<3; i++) {
            GPIOG->ODR |= (1 << 6); SafeDelay(500000);
            GPIOG->ODR &= ~(1 << 6); SafeDelay(500000);
        }
        SafeDelay(2000000);

        // O (---)
        for(int i=0; i<3; i++) {
            GPIOG->ODR |= (1 << 6); SafeDelay(1500000);
            GPIOG->ODR &= ~(1 << 6); SafeDelay(500000);
        }
        SafeDelay(2000000);

        // S (...)
        for(int i=0; i<3; i++) {
            GPIOG->ODR |= (1 << 6); SafeDelay(500000);
            GPIOG->ODR &= ~(1 << 6); SafeDelay(500000);
        }
        SafeDelay(4000000);
    }
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
}

// Redundant Init (handled by Early_Init but kept for HAL state)
static void LED_Init(void) {
    __HAL_RCC_GPIOG_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_6; // LED4 (Red/Green?)
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
}

static void JumpToApp(void) {
    // Check Stack Pointer (First word of App Vector Table)
    // Must be in RAM (0x20000000 - 0x20050000)
    uint32_t sp = *(__IO uint32_t *)APP_ADDR;
    if ((sp & 0xFFF00000) == 0x20000000) {
        JumpAddress = *(__IO uint32_t *)(APP_ADDR + 4);
        JumpToApplication = (pFunction)JumpAddress;

        // Cleanup before Jump
        HAL_UART_DeInit(&huart6);
        HAL_RCC_DeInit();
        HAL_DeInit();
        SysTick->CTRL = 0;
        SysTick->LOAD = 0;
        SysTick->VAL = 0;

        // Disable all interrupts
        __disable_irq();

        // Clear all interrupt enable bits and pending bits
        for (int i = 0; i < 8; i++) {
            NVIC->ICER[i] = 0xFFFFFFFF;
            NVIC->ICPR[i] = 0xFFFFFFFF;
        }

        SCB->VTOR = APP_ADDR;
        __set_MSP(sp);
        JumpToApplication();
    }
}

int main(void) {
    // 1. Immediate Debug Blink (Bypasses HAL/Clock)
    // If LED blinks 5 times here, CPU is alive.
    Early_LED_Init_And_Blink();

    // 2. System Init
    HAL_Init();

    // 3. Clock Init (Blinks SOS on failure)
    SystemClock_Config();

    LED_Init(); // Re-init via HAL
    UART_Init();
    MX_IWDG_Init(); // Start Watchdog

    // Check for OTA Command window (2000ms)
    uint32_t startTick = HAL_GetTick();
    uint32_t ledTick = HAL_GetTick();

    // Bootloader Active Indication (Slower Blink)
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_SET);
    HAL_Delay(200);
    HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_RESET);

    // Packet State Machine
    uint8_t b;
    int state = 0; // 0=Start, 1=Cmd, 2=Len, 3=Payload, 4=CRC
    uint8_t cmd, len, idx = 0;
    uint8_t payload[256];

    OtaCore_Init();

    while (1) {
        HAL_IWDG_Refresh(&hiwdg);

        // Heartbeat Blink (every 200ms)
        if (HAL_GetTick() - ledTick > 200) {
            HAL_GPIO_TogglePin(GPIOG, GPIO_PIN_6);
            ledTick = HAL_GetTick();
        }

        // UART Polling (Non-blocking)
        if (HAL_UART_Receive(&huart6, &b, 1, 0) == HAL_OK) {
            startTick = HAL_GetTick(); // Reset jump timeout

            if (state == 0) {
                if (b == 0xAA) state = 1;
            } else if (state == 1) {
                cmd = b; state = 2;
            } else if (state == 2) {
                len = b; idx = 0;
                if (len == 0) state = 4; else state = 3;
            } else if (state == 3) {
                payload[idx++] = b;
                if (idx == len) state = 4;
            } else if (state == 4) {
                // Process Command
                OtaCore_HandleCmd(cmd, payload, len);
                state = 0;
                // OTA Active: Disable Jump
                startTick = HAL_GetTick() + 9999999;
            }
        }

        // Jump Timeout
        if (startTick < 9999999 && (HAL_GetTick() - startTick > 2000)) {
            JumpToApp();
            // If return, reset loop
            startTick = HAL_GetTick();
        }
    }
}

void SystemClock_Config(void) {
    RCC_OscInitTypeDef RCC_OscInitStruct = {0};
    RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    __HAL_RCC_PWR_CLK_ENABLE();
    __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

    // Configure HSE (8MHz) and PLL
    RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
    RCC_OscInitStruct.HSEState = RCC_HSE_ON;
    RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
    RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
    RCC_OscInitStruct.PLL.PLLM = 8;
    RCC_OscInitStruct.PLL.PLLN = 360; // 180MHz
    RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
    RCC_OscInitStruct.PLL.PLLQ = 7;
    RCC_OscInitStruct.PLL.PLLR = 2;

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        Error_Blink(); // Infinite SOS loop
    }

    if (HAL_PWREx_EnableOverDrive() != HAL_OK) {
        Error_Blink();
    }

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK | RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

    if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
        Error_Blink();
    }
}

static void MX_IWDG_Init(void) {
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_256;
    hiwdg.Init.Reload = 1250; // ~10s
    if (HAL_IWDG_Init(&hiwdg) != HAL_OK) {
        Error_Handler();
    }
}

void Error_Handler(void) {
    Error_Blink();
}
