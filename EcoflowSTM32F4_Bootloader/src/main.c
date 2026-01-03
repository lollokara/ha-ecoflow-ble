#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>

/**
 * @file main.c
 * @author Lollokara
 * @brief STM32F469 Dual Bank Bootloader for EcoFlow Clone
 * @details Handles UART OTA, Bank Swapping, and Boot Safety Checks.
 */

// --- Configuration ---
#define APP_OFFSET 0x8000
#define BANK1_ADDR 0x08000000
#define BANK2_ADDR 0x08100000
#define BOOT_FAIL_THRESHOLD 3

// --- Protocol Constants ---
#define START_BYTE 0xAA
#define CMD_OTA_START 0xA0
#define CMD_OTA_CHUNK 0xA1
#define CMD_OTA_END   0xA2
#define CMD_OTA_APPLY 0xA3
#define CMD_OTA_ACK   0x06
#define CMD_OTA_NACK  0x15

// --- Hardware Defs ---
#ifndef FLASH_OPTCR_BFB2
#define FLASH_OPTCR_BFB2 (1 << 4)
#endif
#ifndef FLASH_OPTCR_DB1M
#define FLASH_OPTCR_DB1M (1 << 30)
#endif

// --- Ring Buffer ---
#define RING_BUFFER_SIZE 2048
typedef struct {
    uint8_t buffer[RING_BUFFER_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
} RingBuffer;

RingBuffer rx_ring_buffer;
uint8_t rx_byte_isr;

UART_HandleTypeDef huart6;
UART_HandleTypeDef huart3; // Debug

typedef void (*pFunction)(void);
pFunction JumpToApplication;
uint32_t JumpAddress;

// --- Prototypes ---
void SystemClock_Config(void);
void UART_Init(void);
void USART3_Init(void);
void GPIO_Init(void);
void Bootloader_OTA_Loop(bool error_mode);
void Serial_Log(const char* fmt, ...);
void DeInit(void);
void LED_R_Toggle(void);
void LED_B_Toggle(void);

// --- Ring Buffer Ops ---
void rb_init(RingBuffer *rb) {
    rb->head = 0;
    rb->tail = 0;
}

void rb_push(RingBuffer *rb, uint8_t byte) {
    uint16_t next_head = (rb->head + 1) % RING_BUFFER_SIZE;
    if (next_head != rb->tail) {
        rb->buffer[rb->head] = byte;
        rb->head = next_head;
    }
}

int rb_pop(RingBuffer *rb, uint8_t *byte) {
    if (rb->head == rb->tail) return 0;
    *byte = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % RING_BUFFER_SIZE;
    return 1;
}

// --- Interrupts ---
void USART6_IRQHandler(void) {
    HAL_UART_IRQHandler(&huart6);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART6) {
        rb_push(&rx_ring_buffer, rx_byte_isr);
        HAL_UART_Receive_IT(&huart6, &rx_byte_isr, 1);
    }
}

// --- UART Helpers ---
HAL_StatusTypeDef UART_ReadByte(uint8_t* out, uint32_t timeout) {
    uint32_t start = HAL_GetTick();
    while (1) {
        if (rb_pop(&rx_ring_buffer, out)) return HAL_OK;
        if (HAL_GetTick() - start > timeout) return HAL_TIMEOUT;
    }
}

HAL_StatusTypeDef UART_ReadBuffer(uint8_t* out, uint16_t len, uint32_t timeout) {
    for (uint16_t i = 0; i < len; i++) {
        if (UART_ReadByte(&out[i], timeout) != HAL_OK) return HAL_TIMEOUT;
    }
    return HAL_OK;
}

// --- CRC32 ---
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

// --- Peripherals ---
void LED_G_On() { HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_RESET); }
void LED_G_Off() { HAL_GPIO_WritePin(GPIOG, GPIO_PIN_6, GPIO_PIN_SET); }
void LED_G_Toggle() { HAL_GPIO_TogglePin(GPIOG, GPIO_PIN_6); }

void LED_O_On() { HAL_GPIO_WritePin(GPIOD, GPIO_PIN_4, GPIO_PIN_RESET); }
void LED_O_Off() { HAL_GPIO_WritePin(GPIOD, GPIO_PIN_4, GPIO_PIN_SET); }
void LED_O_Toggle() { HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_4); }

void LED_R_On() { HAL_GPIO_WritePin(GPIOD, GPIO_PIN_5, GPIO_PIN_RESET); }
void LED_R_Off() { HAL_GPIO_WritePin(GPIOD, GPIO_PIN_5, GPIO_PIN_SET); }
void LED_R_Toggle() { HAL_GPIO_TogglePin(GPIOD, GPIO_PIN_5); }

void LED_B_On() { HAL_GPIO_WritePin(GPIOK, GPIO_PIN_3, GPIO_PIN_RESET); }
void LED_B_Off() { HAL_GPIO_WritePin(GPIOK, GPIO_PIN_3, GPIO_PIN_SET); }
void LED_B_Toggle() { HAL_GPIO_TogglePin(GPIOK, GPIO_PIN_3); }

void All_LEDs_Off() {
    LED_G_Off(); LED_O_Off(); LED_R_Off(); LED_B_Off();
}

void Serial_Log(const char* fmt, ...) {
    char buf[128];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    HAL_UART_Transmit(&huart3, (uint8_t*)buf, strlen(buf), 100);
    HAL_UART_Transmit(&huart3, (uint8_t*)"\r\n", 2, 10);
}

void send_ack() {
    uint8_t buf[4] = {START_BYTE, CMD_OTA_ACK, 0, 0};
    buf[3] = calculate_crc8(&buf[1], 2);
    HAL_UART_Transmit(&huart6, buf, 4, 100);
    LED_G_On(); HAL_Delay(50); LED_G_Off();
}

void send_nack() {
    uint8_t buf[4] = {START_BYTE, CMD_OTA_NACK, 0, 0};
    buf[3] = calculate_crc8(&buf[1], 2);
    HAL_UART_Transmit(&huart6, buf, 4, 100);
    LED_R_On(); HAL_Delay(500); LED_R_Off();
}

void ClearFlashFlags() {
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP | FLASH_FLAG_OPERR | FLASH_FLAG_WRPERR |
                           FLASH_FLAG_PGAERR | FLASH_FLAG_PGPERR | FLASH_FLAG_PGSERR | FLASH_FLAG_RDERR);
}

// --- Main ---
int main(void) {
    HAL_Init();
    GPIO_Init();

    // Quick Startup pattern
    LED_B_On(); HAL_Delay(100); LED_B_Off();
    LED_O_On(); HAL_Delay(100); LED_O_Off();

    SystemClock_Config();
    UART_Init();
    USART3_Init();

    Serial_Log("Bootloader Started. CRC & Logging Active.");

    // Enable Backup Access
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    // Verify DB1M (Dual Bank) - CRITICAL for F469
    FLASH_OBProgramInitTypeDef OBInit;
    HAL_FLASH_Unlock();
    HAL_FLASH_OB_Unlock();
    HAL_FLASHEx_OBGetConfig(&OBInit);
    HAL_FLASH_OB_Lock();
    HAL_FLASH_Lock();

    bool db1m_enabled = (OBInit.USERConfig & FLASH_OPTCR_DB1M);
    if (!db1m_enabled) {
        Serial_Log("CRITICAL: DB1M (Dual Bank) NOT ENABLED! BFB2 will fail.");
    } else {
        Serial_Log("DB1M Enabled. Dual Bank Active.");
    }

    // Determine Active Bank and Jump Address
    // Note: BFB2 controls which physical bank is mapped to 0x00000000 (and aliased to 0x08000000).
    // The Active Bank is ALWAYS accessible at 0x08000000.
    // The Inactive Bank is ALWAYS accessible at 0x08100000.

    bool bfb2_active = ((OBInit.USERConfig & FLASH_OPTCR_BFB2) == FLASH_OPTCR_BFB2);
    uint32_t app_addr_final = BANK1_ADDR + APP_OFFSET; // Always 0x08008000 (Active Bank Alias)

    if (bfb2_active) {
        Serial_Log("Active: Bank 2 (BFB2=1). Mapped to 0x0800xxxx");
    } else {
        Serial_Log("Active: Bank 1 (BFB2=0). Mapped to 0x0800xxxx");
    }

    bool ota_flag = (RTC->BKP0R == 0xDEADBEEF);
    uint32_t boot_fails = RTC->BKP1R;

    // Check Validity at Calculated Address
    uint32_t sp = *(__IO uint32_t*)app_addr_final;
    bool valid_app = (sp >= 0x20000000 && sp <= 0x20060000);

    if (ota_flag) {
        Serial_Log("OTA Flag Detected.");
        RTC->BKP0R = 0;
        RTC->BKP1R = 0;
        Bootloader_OTA_Loop(false);
    }
    else if (valid_app) {
        if (boot_fails >= BOOT_FAIL_THRESHOLD) {
            Serial_Log("Too many failed boots (%d). Forcing OTA.", boot_fails);
            RTC->BKP1R = 0;
            Bootloader_OTA_Loop(true);
        }

        RTC->BKP1R = boot_fails + 1;

        LED_B_On();
        uint8_t buf[1];
        if (HAL_UART_Receive(&huart6, buf, 1, 500) == HAL_OK) {
            if (buf[0] == START_BYTE) {
                 Serial_Log("OTA Byte received on boot.");
                 RTC->BKP1R = 0;
                 Bootloader_OTA_Loop(false);
            }
        }
        LED_B_Off();

        Serial_Log("Jumping to Application...");
        DeInit();

        JumpAddress = *(__IO uint32_t*) (app_addr_final + 4);
        JumpToApplication = (pFunction) JumpAddress;
        __set_MSP(sp);
        JumpToApplication();
    }
    else {
        Serial_Log("No valid app found at 0x%08X. OTA Loop.", app_addr_final);
        RTC->BKP1R = 0;
        Bootloader_OTA_Loop(true);
    }

    while (1) {}
}

void Bootloader_OTA_Loop(bool error_mode) {
    uint8_t header[3];
    uint8_t payload[256];

    rb_init(&rx_ring_buffer);
    HAL_NVIC_SetPriority(USART6_IRQn, 0, 0);
    HAL_NVIC_EnableIRQ(USART6_IRQn);
    HAL_UART_Receive_IT(&huart6, &rx_byte_isr, 1);

    HAL_FLASH_Unlock();
    ClearFlashFlags();
    All_LEDs_Off();

    FLASH_OBProgramInitTypeDef OBInit;
    HAL_FLASH_OB_Unlock();
    HAL_FLASHEx_OBGetConfig(&OBInit);
    HAL_FLASH_OB_Lock();

    bool bfb2_active = ((OBInit.USERConfig & FLASH_OPTCR_BFB2) == FLASH_OPTCR_BFB2);

    // Target Bank Logic
    // On STM32F469 with Dual Bank:
    // If BFB2=0 (Bank 1 Active): Bank 2 is at 0x08100000. Erase Sectors 12-23.
    // If BFB2=1 (Bank 2 Active): Bank 1 is ALIASED to 0x08100000. Erase Sectors 0-11.
    // Conclusion: The INACTIVE bank is ALWAYS at 0x08100000.

    uint32_t target_bank_addr = BANK2_ADDR; // Always 0x08100000 for Inactive Bank
    uint32_t start_sector, end_sector;

    if (bfb2_active) {
        // Active: Bank 2. Target: Bank 1 (Mapped to 0x08100000).
        // Sectors 0-11 correspond to Bank 1 physically.
        start_sector = FLASH_SECTOR_0;
        end_sector = FLASH_SECTOR_11;
        Serial_Log("Active: Bank 2. Target: Bank 1 (Sec 0-11) @ 0x%08X", target_bank_addr);
    } else {
        // Active: Bank 1. Target: Bank 2 (Mapped to 0x08100000).
        // Sectors 12-23 correspond to Bank 2 physically.
        start_sector = FLASH_SECTOR_12;
        end_sector = FLASH_SECTOR_23;
        Serial_Log("Active: Bank 1. Target: Bank 2 (Sec 12-23) @ 0x%08X", target_bank_addr);
    }

    bool ota_started = false;
    bool checksum_verified = false;
    uint32_t bytes_written = 0;

    while(1) {
        static uint32_t last_tick = 0;
        uint32_t now = HAL_GetTick();

        if (now - last_tick > (ota_started ? 200 : 500)) {
            if (error_mode && !ota_started) {
                LED_R_Toggle();
            } else {
                LED_B_Toggle();
            }
            last_tick = now;
        }

        uint8_t b;
        if (UART_ReadByte(&b, 10) != HAL_OK) continue;
        if (b != START_BYTE) continue;

        LED_O_On();
        if (UART_ReadBuffer(&header[1], 2, 100) != HAL_OK) { LED_O_Off(); continue; }
        uint8_t cmd = header[1];
        uint8_t len = header[2];

        if (UART_ReadBuffer(payload, len + 1, 500) != HAL_OK) { LED_O_Off(); continue; }
        LED_O_Off();

        uint8_t recv_crc = payload[len];
        uint8_t check_buf[300];
        check_buf[0] = cmd;
        check_buf[1] = len;
        memcpy(&check_buf[2], payload, len);

        if (calculate_crc8(check_buf, 2 + len) != recv_crc) {
            Serial_Log("CRC Fail: %02X", cmd);
            send_nack(); continue;
        }

        if (cmd == CMD_OTA_START) {
            Serial_Log("OTA Start");
            ota_started = true;
            bytes_written = 0;
            checksum_verified = false;

            FLASH_EraseInitTypeDef EraseInitStruct;
            uint32_t SectorError;
            EraseInitStruct.TypeErase = FLASH_TYPEERASE_SECTORS;
            EraseInitStruct.VoltageRange = FLASH_VOLTAGE_RANGE_3;
            EraseInitStruct.NbSectors = 1;

            ClearFlashFlags();

            bool error = false;
            for (uint32_t sec = start_sector; sec <= end_sector; sec++) {
                EraseInitStruct.Sector = sec;
                if (HAL_FLASHEx_Erase(&EraseInitStruct, &SectorError) != HAL_OK) {
                    Serial_Log("Erase Fail Sec %d", sec);
                    error = true; break;
                }
            }

            if (!error) {
                ClearFlashFlags();
                Serial_Log("Erase Complete");
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
            uint32_t addr = target_bank_addr + offset;

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
                bytes_written += data_len;
                if ((bytes_written / 240) % 64 == 0) Serial_Log("Written %dKB...", bytes_written / 1024);
                send_ack();
            } else {
                Serial_Log("Write Fail %08X", addr);
                send_nack();
            }
        }
        else if (cmd == CMD_OTA_END) {
            uint32_t received_crc32;
            if (len >= 4) {
                memcpy(&received_crc32, payload, 4);
                Serial_Log("OTA End. Verify CRC...");

                uint32_t calculated_crc32 = 0;
                uint8_t* flash_ptr = (uint8_t*)target_bank_addr;
                for(uint32_t i = 0; i < bytes_written; i++) {
                     calculated_crc32 = calculate_crc32(calculated_crc32, &flash_ptr[i], 1);
                }
                Serial_Log("Calc: 0x%08X, Recv: 0x%08X", calculated_crc32, received_crc32);

                if (calculated_crc32 == received_crc32) {
                    checksum_verified = true;
                    send_ack();
                    All_LEDs_Off();
                    LED_G_On();
                } else {
                    checksum_verified = false;
                    Serial_Log("Checksum Mismatch!");
                    send_nack();
                }
            } else {
                send_nack();
            }
        }
        else if (cmd == CMD_OTA_APPLY) {
            if (!checksum_verified) {
                Serial_Log("Err: No Verified Checksum");
                send_nack(); continue;
            }

            Serial_Log("Applying Update...");
            send_ack();
            HAL_Delay(50);

            RTC->BKP1R = 0;

            HAL_FLASH_Unlock();
            HAL_FLASH_OB_Unlock();

            HAL_FLASHEx_OBGetConfig(&OBInit);
            OBInit.OptionType = OPTIONBYTE_USER;

            if (bfb2_active) {
                OBInit.USERConfig &= ~FLASH_OPTCR_BFB2; // Disable BFB2
            } else {
                OBInit.USERConfig |= FLASH_OPTCR_BFB2; // Enable BFB2
            }

            Serial_Log("Programming OB...");
            if (HAL_FLASHEx_OBProgram(&OBInit) == HAL_OK) {
                Serial_Log("OB Launch (Reset)...");
                // Wait for any pending UART transmission
                HAL_Delay(100);

                // Critical Section for Reset
                __disable_irq();
                __DSB();
                __ISB();

                HAL_FLASH_OB_Launch();

                // Should not reach here
                while(1);
            } else {
                Serial_Log("OB Program FAILED!");
                HAL_NVIC_SystemReset();
            }
        }
    }
}

// --- Init Functions ---
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
    huart6.Init.BaudRate = 921600;
    huart6.Init.WordLength = UART_WORDLENGTH_8B;
    huart6.Init.StopBits = UART_STOPBITS_1;
    huart6.Init.Parity = UART_PARITY_NONE;
    huart6.Init.Mode = UART_MODE_TX_RX;
    huart6.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart6.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart6);
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
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct); // Green

    GPIO_InitStruct.Pin = GPIO_PIN_4 | GPIO_PIN_5;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct); // Orange, Red

    GPIO_InitStruct.Pin = GPIO_PIN_3;
    HAL_GPIO_Init(GPIOK, &GPIO_InitStruct); // Blue

    All_LEDs_Off();
}

void DeInit(void) {
    HAL_UART_DeInit(&huart6);
    HAL_UART_DeInit(&huart3);
    HAL_GPIO_DeInit(GPIOG, GPIO_PIN_6);
    HAL_GPIO_DeInit(GPIOD, GPIO_PIN_4 | GPIO_PIN_5);
    HAL_GPIO_DeInit(GPIOK, GPIO_PIN_3);
    SysTick->CTRL = 0;
    __disable_irq();
    HAL_DeInit();
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
  RCC_OscInitStruct.PLL.PLLQ = 4;
  RCC_OscInitStruct.PLL.PLLR = 2;
  if(HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
      while(1) {}
  }

  HAL_PWREx_EnableOverDrive();
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;
  HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5);
}

void SysTick_Handler(void) { HAL_IncTick(); }
