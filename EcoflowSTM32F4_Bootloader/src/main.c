#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>

// Define Application Address (Sector 2)
#define APP_ADDRESS 0x08008000

// UART Protocol
#define START_BYTE 0xAA
#define CMD_OTA_START 0xA0
#define CMD_OTA_CHUNK 0xA1
#define CMD_OTA_END   0xA2
#define CMD_OTA_APPLY 0xA3
#define CMD_OTA_ACK   0x06
#define CMD_OTA_NACK  0x15

// Register Definitions
#ifndef FLASH_OPTCR_BFB2
#define FLASH_OPTCR_BFB2 (1 << 4) // Note: On F469, BFB2 is bit 23, but HAL might map differently.
                                  // Wait, F469 Reference Manual says BFB2 is Bit 23 of FLASH_OPTCR.
                                  // HAL_FLASH_Ex.h defines FLASH_OPTCR_BFB2 as 0x00800000U (Bit 23).
                                  // The previous code had (1 << 4) which is WRONG.
#endif

// Correction for BFB2 based on STM32F469NI
#undef FLASH_OPTCR_BFB2
#define FLASH_OPTCR_BFB2 (1UL << 23)

// Ring Buffer Size
#define RING_BUFFER_SIZE 2048

UART_HandleTypeDef huart6;
UART_HandleTypeDef huart3; // Debug UART
IWDG_HandleTypeDef hiwdg;

typedef void (*pFunction)(void);
pFunction JumpToApplication;
uint32_t JumpAddress;

// Ring Buffer
typedef struct {
    uint8_t buffer[RING_BUFFER_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
} RingBuffer;

RingBuffer rx_ring;
uint8_t rx_byte_isr;

void SystemClock_Config(void);
void UART_Init(void);
void USART3_Init(void);
void GPIO_Init(void);
void IWDG_Init(void);
void Bootloader_OTA_Loop(void);
void Serial_Log(const char* fmt, ...);

// Helper to de-initialize peripherals and interrupts
void DeInit(void) {
    HAL_UART_DeInit(&huart6);
    HAL_UART_DeInit(&huart3);
    HAL_GPIO_DeInit(GPIOG, GPIO_PIN_6);
    HAL_GPIO_DeInit(GPIOD, GPIO_PIN_4 | GPIO_PIN_5);
    HAL_GPIO_DeInit(GPIOK, GPIO_PIN_3);
    SysTick->CTRL = 0;
    SysTick->LOAD = 0;
    SysTick->VAL  = 0;
    __disable_irq();
    for (int i = 0; i < 8; i++) {
        NVIC->ICER[i] = 0xFFFFFFFF;
        NVIC->ICPR[i] = 0xFFFFFFFF;
    }
    HAL_DeInit();
}

// LED Helpers
void LED_G_On() { HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_RESET); }
void LED_G_Off() { HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_SET); }
void LED_O_On() { HAL_GPIO_WritePin(GPIOD, GPIO_PIN_4, GPIO_PIN_RESET); }
void LED_O_Off() { HAL_GPIO_WritePin(GPIOD, GPIO_PIN_4, GPIO_PIN_SET); }
void LED_R_On() { HAL_GPIO_WritePin(GPIOD, GPIO_PIN_5, GPIO_PIN_RESET); }
void LED_R_Off() { HAL_GPIO_WritePin(GPIOD, GPIO_PIN_5, GPIO_PIN_SET); }
void LED_B_On() { HAL_GPIO_WritePin(GPIOK, GPIO_PIN_3, GPIO_PIN_RESET); }
void LED_B_Off() { HAL_GPIO_WritePin(GPIOK, GPIO_PIN_3, GPIO_PIN_SET); }
void LED_B_Toggle() { HAL_GPIO_TogglePin(GPIOK, GPIO_PIN_3); }

void All_LEDs_Off() {
    LED_G_Off(); LED_O_Off(); LED_R_Off(); LED_B_Off();
}

// CRC32 Table (Standard Ethernet 0x04C11DB7)
static const uint32_t crc32_table[] = {
    0x00000000, 0x04c11db7, 0x09823b6e, 0x0d4326d9, 0x130476dc, 0x17c56b6b, 0x1a864db2, 0x1e475005,
    0x2608edb8, 0x22c9f00f, 0x2f8ad6d6, 0x2b4bcb61, 0x350c9b64, 0x31cd86d3, 0x3c8ea00a, 0x384fbdbd,
    0x4c11db70, 0x48d0c6c7, 0x4593e01e, 0x4152fda9, 0x5f15adac, 0x5bd4b01b, 0x569796c2, 0x52568b75,
    0x6a1936c8, 0x6ed82b7f, 0x639b0da6, 0x675a1011, 0x791d4014, 0x7ddc5da3, 0x709f7b7a, 0x745e66cd,
    0x9823b6e0, 0x9ce2ab57, 0x91a18d8e, 0x95609039, 0x8b27c03c, 0x8fe6dd8b, 0x82a5fb52, 0x8664e6e5,
    0xbe2b5b58, 0xbaea46ef, 0xb7a96036, 0xb3687d81, 0xad2f2d84, 0xa9ee3033, 0xa4ad16ea, 0xa06c0b5d,
    0xd4326d90, 0xd0f37027, 0xddb056fe, 0xd9714b49, 0xc7361b4c, 0xc3f706fb, 0xceb42022, 0xca753d95,
    0xf23a8028, 0xf6fb9d9f, 0xfbb8bb46, 0xff79a6f1, 0xe13ef6f4, 0xe5ffeb43, 0xe8bccd9a, 0xec7dd02d,
    0x34867077, 0x30476dc0, 0x3d044b19, 0x39c556ae, 0x278206ab, 0x23431b1c, 0x2e003dc5, 0x2ac12072,
    0x128e9dcf, 0x164f8078, 0x1b0ca6a1, 0x1fcdbb16, 0x018aeb13, 0x054bf6a4, 0x0808d07d, 0x0cc9cdca,
    0x7897ab07, 0x7c56b6b0, 0x71159069, 0x75d48dde, 0x6b93dddb, 0x6f52c06c, 0x6211e6b5, 0x66d0fb02,
    0x5e9f46bf, 0x5a5e5b08, 0x571d7dd1, 0x53dc6066, 0x4d9b3063, 0x495a2dd4, 0x44190b0d, 0x40d816ba,
    0xaca5c697, 0xa864db20, 0xa527fdf9, 0xa1e6e04e, 0xbfa1b04b, 0xbb60adfc, 0xb6238b25, 0xb2e29692,
    0x8aad2b2f, 0x8e6c3698, 0x832f1041, 0x87ee0df6, 0x99a95df3, 0x9d684044, 0x902b669d, 0x94ea7b2a,
    0xe0b41de7, 0xe4750050, 0xe9362689, 0xedf73b3e, 0xf3b06b3b, 0xf771768c, 0xfa325055, 0xfef34de2,
    0xc6bcf05f, 0xc27dede8, 0xcf3ecb31, 0xcbffd686, 0xd5b88683, 0xd1799b34, 0xdc3abded, 0xd8fba05a,
    0x690ce0ee, 0x6dcdfd59, 0x608edb80, 0x644fc637, 0x7a089632, 0x7ec98b85, 0x738aad5c, 0x774bb0eb,
    0x4f040d56, 0x4bc510e1, 0x46863638, 0x42472b8f, 0x5c007b8a, 0x58c1663d, 0x558240e4, 0x51435d53,
    0x251d3b9e, 0x21dc2629, 0x2c9f00f0, 0x285e1d47, 0x36194d42, 0x32d850f5, 0x3f9b762c, 0x3b5a6b9b,
    0x0315d626, 0x07d4cb91, 0x0a97ed48, 0x0e56f0ff, 0x1011a0fa, 0x14d0bd4d, 0x19939b94, 0x1d528623,
    0xf12f560e, 0xf5ee4bb9, 0xf8ad6d60, 0xfc6c70d7, 0xe22b20d2, 0xe6ea3d65, 0xeba91bbc, 0xef68060b,
    0xd727bbb6, 0xd3e6a601, 0xdea580d8, 0xda649d6f, 0xc423cd6a, 0xc0e2d0dd, 0xcda1f604, 0xc960ebb3,
    0xbd3e8d7e, 0xb9ff90c9, 0xb4bcb610, 0xb07daba7, 0xae3afba2, 0xaafbe615, 0xa7b8c0cc, 0xa379dd7b,
    0x9b3660c6, 0x9ff77d71, 0x92b45ba8, 0x9675461f, 0x8832161a, 0x8cf30bad, 0x81b02d74, 0x857130c3,
    0x5d8a9099, 0x594b8d2e, 0x5408abf7, 0x50c9b640, 0x4e8ee645, 0x4a4ffbf2, 0x470cdd2b, 0x43cdc09c,
    0x7b827d21, 0x7f436096, 0x7200464f, 0x76c15bf8, 0x68860bfd, 0x6c47164a, 0x61043093, 0x65c52d24,
    0x119b4be9, 0x155a565e, 0x18197087, 0x1cd86d30, 0x029f3d35, 0x065e2082, 0x0b1d065b, 0x0fdc1bec,
    0x3793a651, 0x3352bbe6, 0x3e119d3f, 0x3ad08088, 0x2497d08d, 0x2056cd3a, 0x2d15ebe3, 0x29d4f654,
    0xc5a92679, 0xc1683bce, 0xcc2b1d17, 0xc8ea00a0, 0xd6ad50a5, 0xd26c4d12, 0xdf2f6bcb, 0xdbee767c,
    0xe3a1cbc1, 0xe760d676, 0xea23f0af, 0xeee2ed18, 0xf0a5bd1d, 0xf464a0aa, 0xf9278673, 0xfde69bc4,
    0x89b8fd09, 0x8d79e0be, 0x803ac667, 0x84fbdbd0, 0x9abc8bd5, 0x9e7d9662, 0x933eb0bb, 0x97ffad0c,
    0xafb010b1, 0xab710d06, 0xa6322bdf, 0xa2f33668, 0xbcb4666d, 0xb8757bda, 0xb5365d03, 0xb1f740b4
};

static uint32_t calculate_crc32(uint32_t crc, const uint8_t *buf, size_t len) {
    crc = ~crc;
    while (len--) {
        crc = (crc << 8) ^ crc32_table[((crc >> 24) ^ *buf++) & 0xFF];
    }
    return ~crc;
}

// Log to USART3
void Serial_Log(const char* fmt, ...) {
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    HAL_UART_Transmit(&huart3, (uint8_t*)buf, strlen(buf), 100);
    HAL_UART_Transmit(&huart3, (uint8_t*)"\r\n", 2, 10);
}

void rb_init(RingBuffer *rb) {
    rb->head = 0; rb->tail = 0;
}

void rb_push(RingBuffer *rb, uint8_t byte) {
    uint16_t next = (rb->head + 1) % RING_BUFFER_SIZE;
    if (next != rb->tail) {
        rb->buffer[rb->head] = byte;
        rb->head = next;
    }
}

int rb_pop(RingBuffer *rb, uint8_t *byte) {
    if (rb->head == rb->tail) return 0;
    *byte = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % RING_BUFFER_SIZE;
    return 1;
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART6) {
        rb_push(&rx_ring, rx_byte_isr);
        HAL_UART_Receive_IT(&huart6, &rx_byte_isr, 1);
    }
}

int main(void) {
    HAL_Init();
    GPIO_Init();

    // 1. Startup Sequence
    LED_B_On(); HAL_Delay(100); LED_B_Off();
    LED_O_On(); HAL_Delay(100); LED_O_Off();
    LED_R_On(); HAL_Delay(100); LED_R_Off();
    LED_G_On(); HAL_Delay(100); LED_G_Off();

    SystemClock_Config();
    UART_Init();
    USART3_Init();
    IWDG_Init(); // 10s timeout

    Serial_Log("Bootloader Started v2.0 (DualBank Safe).");

    // Check Backup Register for OTA Flag
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
    bool ota_flag = (RTC->BKP0R == 0xDEADBEEF);

    // Check App Validity
    uint32_t sp = *(__IO uint32_t*)APP_ADDRESS;
    bool valid_app = ((sp & 0x2FFE0000) == 0x20000000);

    if (ota_flag) {
        Serial_Log("OTA Flag Detected. Entering OTA Mode.");
        RTC->BKP0R = 0;
        Bootloader_OTA_Loop();
    } else if (valid_app) {
        // Wait 500ms for OTA START (Scenario 1)
        // Check Ring Buffer for START_BYTE
        uint32_t start = HAL_GetTick();
        bool enter_ota = false;
        LED_B_On();
        while ((HAL_GetTick() - start) < 500) {
            uint8_t b;
            if (rb_pop(&rx_ring, &b)) {
                if (b == START_BYTE) {
                    enter_ota = true;
                    break;
                }
            }
        }
        LED_B_Off();

        if (enter_ota) {
             Serial_Log("OTA Byte received on boot.");
             Bootloader_OTA_Loop();
        }

        // Jump to App
        LED_G_On();
        Serial_Log("Jumping to Application at 0x%08X", APP_ADDRESS);
        DeInit();
        JumpAddress = *(__IO uint32_t*) (APP_ADDRESS + 4);
        JumpToApplication = (pFunction) JumpAddress;
        __set_MSP(sp);
        JumpToApplication();
    } else {
        Serial_Log("No valid app found. Entering OTA Loop.");
        Bootloader_OTA_Loop();
    }

    while (1) {}
}

uint8_t calculate_crc8(const uint8_t *data, uint8_t len) {
    uint8_t crc = 0;
    for (uint8_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (uint8_t j = 0; j < 8; j++) {
            if (crc & 0x80) crc = (crc << 1) ^ 0x31; else crc <<= 1;
        }
    }
    return crc;
}

void send_ack() {
    uint8_t buf[4] = {START_BYTE, CMD_OTA_ACK, 0, 0};
    buf[3] = calculate_crc8(&buf[1], 2);
    HAL_UART_Transmit(&huart6, buf, 4, 100);
    LED_G_On(); HAL_Delay(20); LED_G_Off();
}

void send_nack() {
    uint8_t buf[4] = {START_BYTE, CMD_OTA_NACK, 0, 0};
    buf[3] = calculate_crc8(&buf[1], 2);
    HAL_UART_Transmit(&huart6, buf, 4, 100);
    LED_R_On(); HAL_Delay(50); LED_R_Off();
}

void ClearFlashFlags() {
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR | FLASH_FLAG_RDERR);
}

// Packet Parser State Machine
typedef enum {
    PARSE_IDLE,
    PARSE_CMD,
    PARSE_LEN,
    PARSE_PAYLOAD,
    PARSE_CRC
} ParseState;

void Bootloader_OTA_Loop(void) {
    ParseState state = PARSE_IDLE;
    uint8_t cmd = 0;
    uint8_t len = 0;
    uint8_t payload[256];
    uint16_t idx = 0;

    // OTA State
    bool ota_active = false;
    uint32_t total_size = 0;
    uint32_t bytes_received = 0;
    bool checksum_verified = false;

    HAL_FLASH_Unlock();
    ClearFlashFlags();
    All_LEDs_Off();

    // Determine Active Bank and Target
    FLASH_OBProgramInitTypeDef OBInit;
    HAL_FLASHEx_OBGetConfig(&OBInit);
    bool bfb2_active = ((OBInit.USERConfig & FLASH_OPTCR_BFB2) == FLASH_OPTCR_BFB2);

    // Physical Sector Mapping
    // If BFB2=0 (Bank 1 Active): Target Bank 2 -> Sectors 12-23
    // If BFB2=1 (Bank 2 Active): Target Bank 1 -> Sectors 0-11
    uint32_t start_sector, end_sector;
    if (!bfb2_active) {
        start_sector = FLASH_SECTOR_12;
        end_sector = FLASH_SECTOR_23;
        Serial_Log("Status: Bank 1 Active. Target: Bank 2 (Sectors 12-23).");
    } else {
        start_sector = FLASH_SECTOR_0;
        end_sector = FLASH_SECTOR_11;
        Serial_Log("Status: Bank 2 Active. Target: Bank 1 (Sectors 0-11).");
    }

    // Target Address is ALWAYS 0x08100000 for the inactive bank in Dual Bank Mode (Aliased)
    uint32_t target_addr_base = 0x08100000;

    while(1) {
        HAL_IWDG_Refresh(&hiwdg);

        // Heartbeat
        static uint32_t last_tick = 0;
        if (HAL_GetTick() - last_tick > (ota_active ? 100 : 1000)) {
            LED_B_Toggle();
            last_tick = HAL_GetTick();
        }

        uint8_t b;
        while (rb_pop(&rx_ring, &b)) {
            switch(state) {
                case PARSE_IDLE:
                    if (b == START_BYTE) {
                        state = PARSE_CMD;
                    }
                    break;
                case PARSE_CMD:
                    if (b == START_BYTE) {
                        // Resync: Found START where CMD expected
                        state = PARSE_CMD;
                    } else {
                        cmd = b;
                        state = PARSE_LEN;
                    }
                    break;
                case PARSE_LEN:
                    len = b;
                    idx = 0;
                    if (len == 0) state = PARSE_CRC;
                    else state = PARSE_PAYLOAD;
                    break;
                case PARSE_PAYLOAD:
                    payload[idx++] = b;
                    if (idx == len) state = PARSE_CRC;
                    break;
                case PARSE_CRC:
                    {
                        uint8_t recv_crc = b;
                        uint8_t check_buf[260];
                        check_buf[0] = cmd;
                        check_buf[1] = len;
                        memcpy(&check_buf[2], payload, len);
                        uint8_t calc_crc = calculate_crc8(check_buf, 2 + len);

                        if (calc_crc == recv_crc) {
                            // Valid Packet
                            LED_O_On();

                            if (cmd == CMD_OTA_START) {
                                memcpy(&total_size, payload, 4);
                                Serial_Log("OTA Start. Size: %d bytes", total_size);
                                ota_active = true;
                                bytes_received = 0;
                                checksum_verified = false;

                                FLASH_EraseInitTypeDef EraseInitStruct;
                                uint32_t SectorError;
                                EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
                                EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
                                EraseInitStruct.NbSectors = 1;

                                bool error = false;
                                for (uint32_t sec = start_sector; sec <= end_sector; sec++) {
                                    HAL_IWDG_Refresh(&hiwdg);
                                    EraseInitStruct.Sector = sec;
                                    if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
                                        Serial_Log("Erase Fail: Sector %d", sec);
                                        error = true; break;
                                    }
                                }

                                if (!error) {
                                    Serial_Log("Erase Complete.");
                                    send_ack();
                                } else {
                                    send_nack();
                                }
                            }
                            else if (cmd == CMD_OTA_CHUNK) {
                                uint32_t offset;
                                memcpy(&offset, payload, 4);
                                uint8_t *data = &payload[4];
                                uint32_t data_len = len - 4;

                                uint32_t addr = target_addr_base + offset;
                                bool ok = true;
                                for (uint32_t i=0; i<data_len; i+=4) {
                                    uint32_t word = 0xFFFFFFFF;
                                    uint8_t copy_len = (data_len - i < 4) ? (data_len - i) : 4;
                                    memcpy(&word, &data[i], copy_len);

                                    if (HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, addr + i, word) != HAL_OK) {
                                        ok = false; break;
                                    }
                                }

                                if (ok) {
                                    bytes_received += data_len;
                                    send_ack();
                                } else {
                                    Serial_Log("Write Fail at 0x%08X", addr);
                                    send_nack();
                                }
                            }
                            else if (cmd == CMD_OTA_END) {
                                uint32_t remote_crc;
                                memcpy(&remote_crc, payload, 4);
                                Serial_Log("OTA End. Verifying...");

                                // Calculate CRC32 of Flash Content
                                uint32_t calc_crc32 = 0;
                                uint8_t *flash_ptr = (uint8_t*)target_addr_base;

                                for(uint32_t i=0; i<total_size; i++) {
                                    calc_crc32 = calculate_crc32(calc_crc32, &flash_ptr[i], 1);
                                    // Refresh IWDG every 16KB to prevent timeout during large CRC
                                    if (i % 0x4000 == 0) HAL_IWDG_Refresh(&hiwdg);
                                }

                                Serial_Log("CRC: Calc=0x%08X, Remote=0x%08X", calc_crc32, remote_crc);

                                if (calc_crc32 == remote_crc) {
                                    checksum_verified = true;
                                    send_ack();
                                    All_LEDs_Off();
                                    LED_G_On();
                                } else {
                                    Serial_Log("CRC Mismatch!");
                                    send_nack();
                                }
                            }
                            else if (cmd == CMD_OTA_APPLY) {
                                if (checksum_verified) {
                                    Serial_Log("Applying Update (Swapping Banks)...");
                                    send_ack();
                                    HAL_Delay(50);

                                    HAL_FLASH_Unlock();
                                    HAL_FLASH_OB_Unlock();
                                    FLASH_OBProgramInitTypeDef OBInit;
                                    HAL_FLASHEx_OBGetConfig(&OBInit);

                                    OBInit.OptionType = OPTIONBYTE_USER;
                                    if (bfb2_active) {
                                        OBInit.USERConfig &= ~FLASH_OPTCR_BFB2; // Clear BFB2 -> Boot Bank 1
                                    } else {
                                        OBInit.USERConfig |= FLASH_OPTCR_BFB2;  // Set BFB2 -> Boot Bank 2
                                    }

                                    HAL_FLASHEx_OBProgram(&OBInit);
                                    HAL_FLASH_OB_Launch(); // Resets
                                } else {
                                    Serial_Log("Cannot Apply: Checksum Not Verified.");
                                    send_nack();
                                }
                            }

                            LED_O_Off();
                        } else {
                            Serial_Log("CRC Err: Cmd=%02X Len=%d Recv=%02X Calc=%02X", cmd, len, recv_crc, calc_crc);
                            send_nack();
                        }
                        state = PARSE_IDLE;
                    }
                    break;
            }
        }
    }
}

void UART_Init(void) {
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

    // Enable Interrupts
    HAL_NVIC_SetPriority(USART6_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART6_IRQn);

    rb_init(&rx_ring);
    HAL_UART_Receive_IT(&huart6, &rx_byte_isr, 1);
}

void USART3_Init(void) {
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

void GPIO_Init(void) {
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOK_CLK_ENABLE();
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = GPIO_PIN_4 | GPIO_PIN_5;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);
    GPIO_InitStruct.Pin = GPIO_PIN_3;
    HAL_GPIO_Init(GPIOK, &GPIO_InitStruct);
    All_LEDs_Off();
}

void IWDG_Init(void) {
    hiwdg.Instance = IWDG;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_256;
    hiwdg.Init.Reload = 1250;
    HAL_IWDG_Init(&hiwdg);
}

void SystemClock_Config(void)
{
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
  RCC_OscInitStruct.PLL.PLLQ = 4;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if(HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
      while(1) { LED_R_On(); HAL_Delay(50); LED_R_Off(); HAL_Delay(50); }
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

void SysTick_Handler(void)
{
  HAL_IncTick();
}
