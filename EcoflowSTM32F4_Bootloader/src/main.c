#include "stm32f4xx_hal.h"
#include <string.h>

/* --- Definitions --- */
#define APP_ADDRESS         0x08008000
#define OTA_MAGIC_NUMBER    0xDEADBEEF
#define OTA_TIMEOUT_MS      30000

/* LEDs: PG6 (Green), PD4 (Orange), PD5 (Red), PK3 (Blue) */
#define LED_GREEN_PIN       GPIO_PIN_6
#define LED_GREEN_PORT      GPIOG
#define LED_ORANGE_PIN      GPIO_PIN_4
#define LED_ORANGE_PORT     GPIOD
#define LED_RED_PIN         GPIO_PIN_5
#define LED_RED_PORT        GPIOD
#define LED_BLUE_PIN        GPIO_PIN_3
#define LED_BLUE_PORT       GPIOK

/* UART: USART6 PG14(TX), PG9(RX) */
UART_HandleTypeDef huart6;

/* Protocol Commands */
#define CMD_OTA_START       0xF0
#define CMD_OTA_CHUNK       0xF1
#define CMD_OTA_END         0xF2
#define CMD_OTA_ACK         0xAA
#define CMD_OTA_NACK        0x55

/* Buffer for one chunk (Flash Write Size) */
#define CHUNK_SIZE          256
#define PACKET_BUFFER_SIZE  (CHUNK_SIZE + 10)

/* --- Function Prototypes --- */
void SystemClock_Config(void);
void GPIO_Init(void);
void UART_Init(void);
void LED_Set(uint8_t green, uint8_t orange, uint8_t red, uint8_t blue);
void LED_BlinkAll(void);
void JumpToApplication(void);
void OTA_Process(void);
uint8_t CRC8(const uint8_t *data, uint16_t len);

/* --- Main --- */
int main(void) {
    HAL_Init();
    SystemClock_Config();
    GPIO_Init();

    // Startup Signal: Blink Rainbow
    LED_BlinkAll();

    UART_Init();

    // Enable PWR and BKP for Magic Number check
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    // __HAL_RCC_RTC_ENABLE(); // Removed to prevent RTC domain reset risk

    // Check Backup Register 0 directly
    uint32_t bkp0 = RTC->BKP0R;

    if (bkp0 == OTA_MAGIC_NUMBER) {
        // Magic Found: Blue LED
        LED_Set(0, 0, 0, 1);

        // Clear magic
        RTC->BKP0R = 0;

        // Enter OTA Mode
        OTA_Process();
    } else {
        // No Magic: Green LED
        LED_Set(1, 0, 0, 0);
        HAL_Delay(100);
        JumpToApplication();
    }

    // If Jump returns (invalid app), turn on Red and halt
    LED_Set(0, 0, 1, 0);
    while (1);
}

/* --- Hardware Init --- */
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
    HAL_RCC_OscConfig(&RCC_OscInitStruct);

    HAL_PWREx_EnableOverDrive();

    RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                                |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
    RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
    RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
    RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
    RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
    HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);
}

void GPIO_Init(void) {
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOK_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // LEDs Output
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;

    GPIO_InitStruct.Pin = LED_GREEN_PIN;
    HAL_GPIO_Init(LED_GREEN_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LED_ORANGE_PIN | LED_RED_PIN;
    HAL_GPIO_Init(LED_ORANGE_PORT, &GPIO_InitStruct);

    GPIO_InitStruct.Pin = LED_BLUE_PIN;
    HAL_GPIO_Init(LED_BLUE_PORT, &GPIO_InitStruct);
}

void UART_Init(void) {
    __HAL_RCC_USART6_CLK_ENABLE();
    __HAL_RCC_GPIOG_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_9 | GPIO_PIN_14; // RX, TX
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

void LED_Set(uint8_t green, uint8_t orange, uint8_t red, uint8_t blue) {
    // Active High
    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, green ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_ORANGE_PORT, LED_ORANGE_PIN, orange ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, red ? GPIO_PIN_SET : GPIO_PIN_RESET);
    HAL_GPIO_WritePin(LED_BLUE_PORT, LED_BLUE_PIN, blue ? GPIO_PIN_SET : GPIO_PIN_RESET);
}

void LED_BlinkAll(void) {
    // Sequence: Green -> Orange -> Red -> Blue
    LED_Set(1, 0, 0, 0); HAL_Delay(200);
    LED_Set(0, 1, 0, 0); HAL_Delay(200);
    LED_Set(0, 0, 1, 0); HAL_Delay(200);
    LED_Set(0, 0, 0, 1); HAL_Delay(200);
    LED_Set(0, 0, 0, 0); HAL_Delay(200);
}

/* --- App Jump --- */
typedef void (*pFunction)(void);

void JumpToApplication(void) {
    uint32_t jumpAddress;
    pFunction JumpToApp;

    // Check if Stack Pointer is valid (in RAM)
    // 0x20000000 to 0x20050000 (320KB RAM)
    uint32_t stack_ptr = *(__IO uint32_t*)APP_ADDRESS;
    if ((stack_ptr & 0x2FF00000) == 0x20000000) {
        // DeInit Peripherals
        HAL_UART_DeInit(&huart6);
        HAL_RCC_DeInit();
        HAL_DeInit();
        SysTick->CTRL = 0;
        SysTick->LOAD = 0;
        SysTick->VAL = 0;

        // Jump
        jumpAddress = *(__IO uint32_t*) (APP_ADDRESS + 4);
        JumpToApp = (pFunction) jumpAddress;

        __set_MSP(stack_ptr);
        SCB->VTOR = APP_ADDRESS;
        JumpToApp();
    }
}

/* --- OTA Logic --- */
// Simple Checksum: CRC8 Poly 0x31 (X8 + X5 + X4 + 1)
uint8_t CRC8(const uint8_t *data, uint16_t len) {
    uint8_t crc = 0x00;
    while (len--) {
        crc ^= *data++;
        for (uint8_t i = 0; i < 8; i++) {
            if (crc & 0x80) crc = (crc << 1) ^ 0x31;
            else crc <<= 1;
        }
    }
    return crc;
}

void UART_SendByte(uint8_t b) {
    HAL_UART_Transmit(&huart6, &b, 1, 100);
}

void UART_SendAck(void) {
    UART_SendByte(CMD_OTA_ACK);
}

void OTA_Process(void) {
    uint8_t rx_buf[PACKET_BUFFER_SIZE];
    uint32_t current_addr = APP_ADDRESS;
    uint32_t total_received = 0;
    uint32_t total_expected = 0;

    LED_Set(0, 1, 0, 0); // Orange = OTA Wait

    // Unlock Flash
    HAL_FLASH_Unlock();

    // Handshake Loop
    // Flush RX buffer first to prevent stale data
    __HAL_UART_FLUSH_DRREGISTER(&huart6);

    while (1) {
        // Send READY every 500ms? No, wait for ESP32 to initiate
        // Use timeout receive
        if (HAL_UART_Receive(&huart6, rx_buf, 5, 200) == HAL_OK) {
            if (rx_buf[0] == CMD_OTA_START) {
                // rx_buf[1..4] = Size
                total_expected = (rx_buf[1] << 24) | (rx_buf[2] << 16) | (rx_buf[3] << 8) | rx_buf[4];
                UART_SendAck();
                break;
            }
        }
        // Toggle Blue to indicate waiting life
        HAL_GPIO_TogglePin(LED_BLUE_PORT, LED_BLUE_PIN);
    }

    LED_Set(1, 1, 0, 0); // Green+Orange = Erasing

    // Erase Sectors needed
    // 374KB -> Sectors 2 to 7.
    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError;
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    EraseInitStruct.Sector = FLASH_SECTOR_2;
    EraseInitStruct.NbSectors = 6; // Erase up to Sector 7

    // Clear flags before erase
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
        LED_Set(0, 0, 1, 0); // Red = Error
        while(1);
    }

    UART_SendAck(); // Ready for data
    LED_Set(0, 1, 0, 1); // Blue+Orange = Receiving

    // Data Loop
    while (total_received < total_expected) {
        // Expect: [CMD_OTA_CHUNK] [LEN_H] [LEN_L] [DATA...] [CRC]
        // Header
        if (HAL_UART_Receive(&huart6, rx_buf, 3, 5000) != HAL_OK) {
            // Timeout
             LED_Set(0, 0, 1, 1); // Red+Blue Error
             continue;
        }

        if (rx_buf[0] != CMD_OTA_CHUNK) {
            // Unexpected
            continue;
        }

        uint16_t len = (rx_buf[1] << 8) | rx_buf[2];
        if (len > CHUNK_SIZE) {
            // Error
            while(1);
        }

        // Payload + CRC
        if (HAL_UART_Receive(&huart6, &rx_buf[3], len + 1, 1000) != HAL_OK) {
            // Timeout
            while(1);
        }

        // Verify CRC
        uint8_t calc_crc = CRC8(&rx_buf[3], len);
        if (calc_crc != rx_buf[3 + len]) {
            // NACK
            UART_SendByte(CMD_OTA_NACK);
            continue;
        }

        // Write to Flash
        for (int i = 0; i < len; i++) {
            if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, current_addr, rx_buf[3+i]) != HAL_OK) {
                LED_Set(0, 0, 1, 0); // Red
                while(1);
            }
            current_addr++;
        }

        total_received += len;
        UART_SendAck();
        LED_Set(0, 1, 0, (total_received / 1024) % 2); // Toggle Blue
    }

    HAL_FLASH_Lock();

    // Verify End
    if (HAL_UART_Receive(&huart6, rx_buf, 1, 5000) == HAL_OK) {
        if (rx_buf[0] == CMD_OTA_END) {
             UART_SendAck();
        }
    }

    // Success
    LED_Set(1, 1, 1, 1); // All On
    HAL_Delay(1000);
    HAL_NVIC_SystemReset();
}
