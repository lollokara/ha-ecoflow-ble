#include "stm32f4xx_hal.h"
#include <string.h>

/* --- Definitions --- */
#define APP_ADDRESS         0x08008000
#define OTA_MAGIC_NUMBER    0xDEADBEEF
#define OTA_TIMEOUT_MS      30000

/* LEDs: PG6 (Green), PD4 (Orange), PD5 (Red), PK3 (Blue) */
/* Logic: Active LOW (0 = ON, 1 = OFF) */
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
#define START_BYTE          0xAA
#define CMD_OTA_START       0xF0
#define CMD_OTA_CHUNK       0xF1
#define CMD_OTA_END         0xF2
#define CMD_OTA_ACK         0xAA
#define CMD_OTA_NACK        0x55

/* Buffer */
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

    // Clear LEDs (Active Low -> Write 1)
    LED_Set(0, 0, 0, 0);

    // Startup: Blink Green 3 times
    for(int i=0; i<3; i++) {
        LED_Set(1, 0, 0, 0); HAL_Delay(100);
        LED_Set(0, 0, 0, 0); HAL_Delay(100);
    }

    UART_Init();

    // Enable PWR and BKP for Magic Number check
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    // Check Backup Register 0
    uint32_t bkp0 = RTC->BKP0R;

    if (bkp0 == OTA_MAGIC_NUMBER) {
        // Magic Found: Blue Solid
        LED_Set(0, 0, 0, 1);
        RTC->BKP0R = 0; // Clear
        OTA_Process();
    } else {
        // No Magic: Jump
        JumpToApplication();
    }

    // Jump Failed: Red Blink
    while (1) {
        LED_Set(0, 0, 1, 0); HAL_Delay(200);
        LED_Set(0, 0, 0, 0); HAL_Delay(200);
    }
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

    if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
        // HSE Fail: Red Solid
        __HAL_RCC_GPIOD_CLK_ENABLE();
        GPIO_InitTypeDef GPIO_InitStruct = {0};
        GPIO_InitStruct.Pin = LED_RED_PIN;
        GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(LED_RED_PORT, &GPIO_InitStruct);
        HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, GPIO_PIN_RESET); // ON
        while(1);
    }

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
    // Active Low: 0=ON, 1=OFF
    HAL_GPIO_WritePin(LED_GREEN_PORT, LED_GREEN_PIN, green ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_ORANGE_PORT, LED_ORANGE_PIN, orange ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_RED_PORT, LED_RED_PIN, red ? GPIO_PIN_RESET : GPIO_PIN_SET);
    HAL_GPIO_WritePin(LED_BLUE_PORT, LED_BLUE_PIN, blue ? GPIO_PIN_RESET : GPIO_PIN_SET);
}

/* --- App Jump --- */
typedef void (*pFunction)(void);

void JumpToApplication(void) {
    uint32_t jumpAddress;
    pFunction JumpToApp;

    uint32_t stack_ptr = *(__IO uint32_t*)APP_ADDRESS;
    if ((stack_ptr & 0x2FF00000) == 0x20000000) {
        HAL_UART_DeInit(&huart6);
        HAL_RCC_DeInit();
        HAL_DeInit();
        SysTick->CTRL = 0;
        SysTick->LOAD = 0;
        SysTick->VAL = 0;

        jumpAddress = *(__IO uint32_t*) (APP_ADDRESS + 4);
        JumpToApp = (pFunction) jumpAddress;

        __set_MSP(stack_ptr);
        SCB->VTOR = APP_ADDRESS;
        JumpToApp();
    }
}

/* --- OTA Logic --- */
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

// Receive a full packet: [START] [CMD] [LEN] [PAYLOAD...] [CRC]
// Returns CMD if success, 0 if fail/timeout
uint8_t ReceivePacket(uint8_t *payload_out, uint16_t *len_out, uint32_t timeout) {
    uint8_t header[3]; // START, CMD, LEN

    // Wait for START byte
    uint32_t tick = HAL_GetTick();
    uint8_t b = 0;
    while ((HAL_GetTick() - tick) < timeout) {
        if (HAL_UART_Receive(&huart6, &b, 1, 10) == HAL_OK) {
            if (b == START_BYTE) break;
        }
    }
    if (b != START_BYTE) return 0;

    // Read CMD and LEN
    if (HAL_UART_Receive(&huart6, header, 2, 100) != HAL_OK) return 0;

    uint8_t cmd = header[0];
    uint8_t len = header[1];

    // Read Payload + CRC
    uint8_t buf[256 + 1];
    if (HAL_UART_Receive(&huart6, buf, len + 1, 500) != HAL_OK) return 0;

    // Verify CRC
    uint8_t calc_crc = CRC8(header, 2); // CRC includes CMD and LEN?
    // Protocol says: [START][CMD][LEN][PAYLOAD][CRC]
    // Standard logic usually calculates CRC over Payload?
    // Let's check Stm32Serial logic.
    // Stm32Serial: calculate_crc8(&rx_buf[1], 2 + expected_len);
    // rx_buf[1] is CMD. rx_buf[2] is LEN.
    // So CRC is over CMD + LEN + PAYLOAD.

    calc_crc = 0;
    // Manual CRC over header
    uint8_t h_calc[2] = {cmd, len};
    // CRC8 function implementation restarts? No, it's stateful if we want.
    // But our CRC8 function resets to 0x00.
    // We need a CRC function that takes a seed or we concat.

    // Concat buffer for verification
    uint8_t verify_buf[300];
    verify_buf[0] = cmd;
    verify_buf[1] = len;
    memcpy(&verify_buf[2], buf, len); // Payload

    if (CRC8(verify_buf, len + 2) != buf[len]) {
        return 0; // CRC Fail
    }

    memcpy(payload_out, buf, len);
    *len_out = len;
    return cmd;
}

void OTA_Process(void) {
    uint8_t payload[256];
    uint16_t len;
    uint32_t current_addr = APP_ADDRESS;
    uint32_t total_received = 0;
    uint32_t total_expected = 0;

    // Orange Wait
    LED_Set(0, 1, 0, 0);

    HAL_FLASH_Unlock();
    __HAL_UART_FLUSH_DRREGISTER(&huart6);

    // Handshake
    while (1) {
        uint8_t cmd = ReceivePacket(payload, &len, 200);
        if (cmd == CMD_OTA_START && len == 4) {
            total_expected = (payload[0] << 24) | (payload[1] << 16) | (payload[2] << 8) | payload[3];
            UART_SendAck();
            break;
        }
        // Blink Blue waiting
        HAL_GPIO_TogglePin(LED_BLUE_PORT, LED_BLUE_PIN);
    }

    // Erase: Orange + Green
    LED_Set(1, 1, 0, 0);

    FLASH_EraseInitTypeDef EraseInitStruct;
    uint32_t SectorError;
    EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
    EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
    EraseInitStruct.Sector = FLASH_SECTOR_2;
    EraseInitStruct.NbSectors = 6;

    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR);

    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
        LED_Set(0, 0, 1, 0); while(1); // Red Error
    }

    UART_SendAck();

    // Receive: Green Flash
    LED_Set(1, 0, 0, 0);

    while (total_received < total_expected) {
        uint8_t cmd = ReceivePacket(payload, &len, 5000);
        if (cmd == CMD_OTA_CHUNK) {
            for (int i = 0; i < len; i++) {
                if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_BYTE, current_addr++, payload[i]) != HAL_OK) {
                    LED_Set(0, 0, 1, 0); while(1);
                }
            }
            total_received += len;
            UART_SendAck();
            HAL_GPIO_TogglePin(LED_GREEN_PORT, LED_GREEN_PIN);
        } else {
            // Timeout or Error
            // Optional: Send NACK
        }
    }

    HAL_FLASH_Lock();

    // End
    uint8_t cmd = ReceivePacket(payload, &len, 5000);
    if (cmd == CMD_OTA_END) {
        UART_SendAck();
    }

    // Success Rainbow
    LED_Set(1, 0, 0, 0); HAL_Delay(100);
    LED_Set(0, 1, 0, 0); HAL_Delay(100);
    LED_Set(0, 0, 1, 0); HAL_Delay(100);
    LED_Set(0, 0, 0, 1); HAL_Delay(100);
    HAL_NVIC_SystemReset();
}
