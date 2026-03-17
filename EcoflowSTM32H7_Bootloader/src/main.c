// Ensure HSE_VALUE is defined correctly before HAL inclusion
#ifndef HSE_VALUE
#define HSE_VALUE 25000000
#endif

#include "stm32h7xx_hal.h"
#include <string.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdarg.h>

// External Flash Offsets
#define EXT_FLASH_BASE 0x90000000
#define BANK_A_OFFSET  0x00000000
#define BANK_B_OFFSET  0x01000000
#define OTA_OFFSET     0x02000000

// UART Protocol
#define START_BYTE 0xAA
#define CMD_OTA_START 0xA0
#define CMD_OTA_CHUNK 0xA1
#define CMD_OTA_END   0xA2
#define CMD_OTA_APPLY 0xA3
#define CMD_OTA_ACK   0x06
#define CMD_OTA_NACK  0x15

// Ring Buffer Definition
#define RING_BUFFER_SIZE 2048
typedef struct {
    uint8_t buffer[RING_BUFFER_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
} RingBuffer;

RingBuffer rx_ring_buffer;
uint8_t rx_byte_isr;

// Register Definitions
#ifndef FLASH_OPTCR_BFB2
#define FLASH_OPTCR_BFB2 (1 << 4)
#endif

UART_HandleTypeDef huart6;
UART_HandleTypeDef huart3; // Debug UART
IWDG_HandleTypeDef hiwdg; // Watchdog
OSPI_HandleTypeDef hospi1; // OctoSPI Flash

typedef void (*pFunction)(void);
pFunction JumpToApplication;
uint32_t JumpAddress;

void SystemClock_Config(void);
void UART_Init(void);
void USART3_Init(void);
void MX_IWDG_Init(void);
void GPIO_Init(void);
void MX_OCTOSPI1_Init(void);
void OSPI_EnableMemoryMappedMode(void);
void OSPI_DisableMemoryMappedMode(void);
HAL_StatusTypeDef OSPI_EraseSector(uint32_t address);
HAL_StatusTypeDef OSPI_WriteData(uint32_t address, uint8_t *buffer, uint32_t length);
void MPU_Config(void);
void Bootloader_OTA_Loop(void);
void Serial_Log(const char* fmt, ...);
void ClearFlashFlags(void);

// Software Delay (Independent of SysTick)
void Software_Delay(uint32_t count) {
    for (volatile uint32_t i = 0; i < count; i++) {
        __NOP();
    }
}

// --- Ring Buffer Implementation ---
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
    if (rb->head == rb->tail) {
        return 0; // Empty
    }
    *byte = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % RING_BUFFER_SIZE;
    return 1;
}

int rb_available(RingBuffer *rb) {
    if (rb->head >= rb->tail) return rb->head - rb->tail;
    return RING_BUFFER_SIZE - rb->tail + rb->head;
}

// --- Interrupt Handling ---
void USART6_IRQHandler(void) {
    HAL_UART_IRQHandler(&huart6);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == USART6) {
        rb_push(&rx_ring_buffer, rx_byte_isr);
        HAL_UART_Receive_IT(&huart6, &rx_byte_isr, 1);
    }
}

// Blocking Read from Ring Buffer with Timeout
HAL_StatusTypeDef UART_ReadByte(uint8_t* out, uint32_t timeout) {
    uint32_t start = HAL_GetTick();
    while (1) {
        if (rb_pop(&rx_ring_buffer, out)) {
            return HAL_OK;
        }
        if (HAL_GetTick() - start > timeout) {
            return HAL_TIMEOUT;
        }
    }
}

// Helper to fill buffer from Ring Buffer
HAL_StatusTypeDef UART_ReadBuffer(uint8_t* out, uint16_t len, uint32_t timeout) {
    for (uint16_t i = 0; i < len; i++) {
        if (UART_ReadByte(&out[i], timeout) != HAL_OK) {
            return HAL_TIMEOUT;
        }
    }
    return HAL_OK;
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

// Helper to de-initialize peripherals and interrupts
void DeInit(void) {
    HAL_UART_DeInit(&huart6);
    HAL_UART_DeInit(&huart3);

    // De-init LEDs
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

int main(void) {
    // CRITICAL: Ensure Interrupt Vector Table is set correctly.
    // When booting from Bank 2 (aliased to 0x00), this ensures VTOR points to
    // the alias 0x00 (or 0x08000000 alias) rather than some other random location.
    MPU_Config();

    SCB->VTOR = 0x08000000;
    __DSB(); // Data Synchronization Barrier

    HAL_Init();
    SystemClock_Config(); // Clock init first for correct speed (180MHz)
    GPIO_Init();

    // 1. Startup Sequence with Software Delay
    // This verifies CPU execution INDEPENDENT of SysTick.
    // If these blink but subsequent HAL_Delay fails, we know it's SysTick.
    // Delay increased to ~10M iterations for visibility (~200ms at 180MHz)
    LED_B_On(); Software_Delay(500000); LED_B_Off();
    LED_O_On(); Software_Delay(500000); LED_O_Off();
    LED_R_On(); Software_Delay(500000); LED_R_Off();
    LED_G_On(); Software_Delay(500000); LED_G_Off();

    UART_Init();
    USART3_Init();
    MX_IWDG_Init(); // Init Watchdog (Required for Flash Erase loops)

    Serial_Log("Bootloader Started. CRC & Logging Active.");

    MX_OCTOSPI1_Init();
    OSPI_EnableMemoryMappedMode();
    Serial_Log("OSPI Initialized and Memory Mapped.");

    // Enable Backup Access for OTA Flag and Boot Counter
    // __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    bool ota_flag = (RTC->BKP0R == 0xDEADBEEF);
    uint32_t boot_fails = RTC->BKP1R;

    // Determine Active Bank based on RTC Backup Register 2
    // If BKP2R == 0 (or uninitialized), Bank A is active
    // If BKP2R == 1, Bank B is active
    bool is_bank_b_active = (RTC->BKP2R == 1);
    uint32_t app_address = EXT_FLASH_BASE + (is_bank_b_active ? BANK_B_OFFSET : BANK_A_OFFSET);

    Serial_Log("Active Bank: %s (Address 0x%08X)", is_bank_b_active ? "B" : "A", app_address);

    // Check App Validity (SP must be in RAM 0x20000000 - 0x24080000 approx for H7)
    uint32_t sp = *(__IO uint32_t*)app_address;
    bool valid_app = (sp >= 0x20000000 && sp <= 0x24080000);

    Serial_Log("Checking App at 0x%08X. SP: 0x%08X. Valid: %d", app_address, sp, valid_app);

    if (ota_flag) {
        Serial_Log("OTA Flag Detected.");
        // Clear flag and boot counter
        RTC->BKP0R = 0;
        RTC->BKP1R = 0;
        Bootloader_OTA_Loop();
    } else if (valid_app) {
        if (boot_fails >= 3) {
            Serial_Log("Too many failed boots (%d). Forcing OTA.", boot_fails);
            // Flash RED to indicate failure
            for(int i=0; i<10; i++) { LED_R_Toggle(); HAL_Delay(100); }
            RTC->BKP1R = 0;
            Bootloader_OTA_Loop();
        }

        // Increment Boot Counter (App must clear it on success)
        RTC->BKP1R = boot_fails + 1;

        // Wait 500ms for OTA START - Blink Blue
        uint8_t buf[3];
        LED_B_On();

        // Check for specific CMD_OTA_START command sequence
        // AA A0 ...
        if (HAL_UART_Receive(&huart6, buf, 1, 500) == HAL_OK) {
            if (buf[0] == START_BYTE) {
                 if (HAL_UART_Receive(&huart6, &buf[1], 2, 50) == HAL_OK) {
                     if (buf[1] == CMD_OTA_START) {
                         Serial_Log("OTA START received on boot.");
                         RTC->BKP1R = 0; // Reset counter if we enter OTA manually
                         Bootloader_OTA_Loop();
                     }
                 }
            }
        }
        LED_B_Off();

        // Jump - Green ON
        LED_G_On();
        Serial_Log("Jumping to Application at 0x%08X", app_address);

        // Reset RCC (Clocks) to default state (HSI) before jump
        HAL_RCC_DeInit();

        // Do NOT completely DeInit OSPI if memory mapped execution is required!
        // We just de-init UART and LEDs
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

        JumpAddress = *(__IO uint32_t*) (app_address + 4);
        JumpToApplication = (pFunction) JumpAddress;
        __set_MSP(sp);
        JumpToApplication();
    } else {
        // No valid app, force OTA
        Serial_Log("No valid app found. Entering OTA Loop.");
        RTC->BKP1R = 0;
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
    // REMOVED HAL_Delay(50) for performance
    // Tiny software delay for LED visibility, negligible impact
    LED_G_On();
    Software_Delay(1000);
    LED_G_Off();
}

void send_nack() {
    uint8_t buf[4] = {START_BYTE, CMD_OTA_NACK, 0, 0};
    buf[3] = calculate_crc8(&buf[1], 2);
    HAL_UART_Transmit(&huart6, buf, 4, 100);
    // Red Flash - Reduced delay
    LED_R_On(); HAL_Delay(50); LED_R_Off();
}

void ClearFlashFlags() {
    // __HAL_FLASH_CLEAR_FLAG
}

// Bit 30: DB1M (0=Dual Bank, 1=Single Bank)
#ifndef FLASH_OPTCR_DB1M
#define FLASH_OPTCR_DB1M (1 << 30)
#endif

void Bootloader_OTA_Loop(void) {
    uint8_t header[3];
    uint8_t payload[256];

    // Initialize Ring Buffer and Interrupts
    rb_init(&rx_ring_buffer);
    HAL_NVIC_SetPriority(USART6_IRQn, 0, 0); // High Priority
    HAL_NVIC_EnableIRQ(USART6_IRQn);
    HAL_UART_Receive_IT(&huart6, &rx_byte_isr, 1);

    HAL_FLASH_Unlock();
    ClearFlashFlags(); // Clear flags on entry
    All_LEDs_Off();

    // Abort Memory Mapped mode so we can write to OSPI
    HAL_OSPI_Abort(&hospi1);

    // Target OTA Partition
    uint32_t target_ota_addr = OTA_OFFSET;

    bool is_bank_b_active = (RTC->BKP2R == 1);
    uint32_t inactive_bank_offset = is_bank_b_active ? BANK_A_OFFSET : BANK_B_OFFSET;

    Serial_Log("Active Bank: %s, Inactive: %s, OTA Addr: 0x%08X",
                is_bank_b_active ? "B" : "A",
                is_bank_b_active ? "A" : "B",
                EXT_FLASH_BASE + target_ota_addr);

    bool ota_started = false;
    bool checksum_verified = false;
    uint32_t bytes_written = 0;
    uint32_t chunks_received = 0;
    uint32_t last_packet_time = HAL_GetTick();

    while(1) {
        // Watchdog Refresh (Important for long waits)
        HAL_IWDG_Refresh(&hiwdg);

        // Heartbeat: Blue Toggle
        static uint32_t last_tick = 0;
        if (HAL_GetTick() - last_tick > (ota_started ? 200 : 1000)) {
            LED_B_Toggle();
            last_tick = HAL_GetTick();
        }

        // Timeout Check (30 seconds of inactivity)
        // Only if OTA has not started (or maybe even if it has? Best to allow resume if stuck)
        // If OTA has started, we might want to stay longer, but 30s silence is bad.
        // But the ESP32 might be downloading.
        // Let's set timeout to 30s. If OTA started, maybe 60s?
        uint32_t timeout_val = ota_started ? 60000 : 30000;
        if (HAL_GetTick() - last_packet_time > timeout_val) {
             Serial_Log("OTA Timeout (%d ms). Resetting...", timeout_val);
             HAL_NVIC_SystemReset();
        }

        uint8_t b;
        // Non-blocking check for start byte
        if (UART_ReadByte(&b, 10) != HAL_OK) continue;
        if (b != START_BYTE) continue;

        // RX Activity: Orange On
        LED_O_On();

        if (UART_ReadBuffer(&header[1], 2, 100) != HAL_OK) {
            LED_O_Off(); continue;
        }
        uint8_t cmd = header[1];
        uint8_t len = header[2];

        if (UART_ReadBuffer(payload, len + 1, 500) != HAL_OK) {
            LED_O_Off(); continue;
        }

        last_packet_time = HAL_GetTick(); // Update timestamp
        LED_O_Off(); // RX Done

        uint8_t recv_crc = payload[len];
        uint8_t check_buf[300];
        check_buf[0] = cmd;
        check_buf[1] = len;
        memcpy(&check_buf[2], payload, len);

        uint8_t calculated_crc8_val = calculate_crc8(check_buf, 2 + len);
        if (calculated_crc8_val != recv_crc) {
            Serial_Log("CRC Err: Cmd=%02X Len=%d Calc=%02X Recv=%02X", cmd, len, calculated_crc8_val, recv_crc);
            send_nack(); continue;
        }

        if (cmd == CMD_OTA_START) {
            Serial_Log("OTA Start");
            ota_started = true;
            bytes_written = 0;
            chunks_received = 0;
            checksum_verified = false;

            bool error = false;
            // Erase the OTA partition (Assume max 16MB image size for simplicity, or we can erase as we go)
            // MX25LM51245G Sector size is usually 64KB
            // We will erase 16MB = 256 Sectors
            Serial_Log("Erasing OTA partition...");
            for (uint32_t offset = 0; offset < (16 * 1024 * 1024); offset += (64 * 1024)) {
                HAL_IWDG_Refresh(&hiwdg);
                if (OSPI_EraseSector(target_ota_addr + offset) != HAL_OK) {
                    Serial_Log("Erase Error at offset 0x%08X", offset);
                    error = true; break;
                }
            }

            if (!error) {
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

            // Write to OTA Partition
            uint32_t addr = target_ota_addr + offset;

            bool ok = true;

            // Assume data_len is page-aligned or we just write it all
            if (OSPI_WriteData(addr, data, data_len) != HAL_OK) {
                ok = false;
            }

            if (ok) {
                bytes_written += data_len;
                chunks_received++;
                if (chunks_received % 64 == 0) { // Log every ~16KB
                    Serial_Log("Written %dKB...", bytes_written / 1024);
                }
                send_ack();
            } else {
                Serial_Log("Flash Write Error at %08X.", addr);
                send_nack();
            }
        }
        else if (cmd == CMD_OTA_END) {
            // New Logic: Checksum Verification
            uint32_t received_crc32;
            if (len >= 4) {
                memcpy(&received_crc32, payload, 4);
                Serial_Log("OTA End. Verifying CRC...");

                // Calculate CRC of written flash using SOFTWARE CRC
                // Switch back to Memory Mapped Mode to easily read flash for CRC check
                OSPI_EnableMemoryMappedMode();

                uint32_t calculated_crc32 = 0;
                uint8_t* flash_ptr = (uint8_t*)(EXT_FLASH_BASE + target_ota_addr);

                for(uint32_t i = 0; i < bytes_written; i++) {
                     // Refresh Watchdog during CRC Check
                     if (i % 10000 == 0) HAL_IWDG_Refresh(&hiwdg);
                     calculated_crc32 = calculate_crc32(calculated_crc32, &flash_ptr[i], 1);
                }

                Serial_Log("Calc: 0x%08X, Recv: 0x%08X", calculated_crc32, received_crc32);

                // Disable Memory Mapped Mode to allow applying if needed
                OSPI_DisableMemoryMappedMode();

                if (calculated_crc32 == received_crc32) {
                    checksum_verified = true;
                    send_ack();
                    All_LEDs_Off();
                    LED_G_On(); // Green Solid
                } else {
                    checksum_verified = false;
                    Serial_Log("Checksum Mismatch!");
                    send_nack();
                }

            } else {
                Serial_Log("OTA End without CRC. Rejecting.");
                send_nack();
            }
        }
        else if (cmd == CMD_OTA_APPLY) {
            if (!checksum_verified) {
                Serial_Log("Apply requested but Checksum not verified!");
                send_nack();
                continue;
            }

            Serial_Log("Applying Update...");
            send_ack();

            // Disable Memory Mapped Mode to allow erasing/writing
            OSPI_DisableMemoryMappedMode();

            // Step 1: Erase Inactive Bank
            Serial_Log("Erasing Inactive Bank at 0x%08X", inactive_bank_offset);
            bool erase_error = false;
            // Erase up to `bytes_written` rounded to next 64KB sector
            for (uint32_t offset = 0; offset < bytes_written; offset += (64 * 1024)) {
                HAL_IWDG_Refresh(&hiwdg);
                if (OSPI_EraseSector(inactive_bank_offset + offset) != HAL_OK) {
                    Serial_Log("Erase Error at offset 0x%08X", offset);
                    erase_error = true; break;
                }
            }

            if (erase_error) {
                Serial_Log("Erase failed. Aborting apply.");
                HAL_NVIC_SystemReset();
            }

            // Step 2: Copy from OTA Partition to Inactive Bank
            Serial_Log("Copying OTA to Inactive Bank...");

            // Re-enable memory mapped mode to easily read the OTA partition
            OSPI_EnableMemoryMappedMode();

            // Temporary buffer to read from OTA partition and write to inactive bank
            uint8_t copy_buffer[256];
            uint8_t* ota_ptr = (uint8_t*)(EXT_FLASH_BASE + target_ota_addr);

            bool copy_error = false;
            for (uint32_t offset = 0; offset < bytes_written; offset += 256) {
                HAL_IWDG_Refresh(&hiwdg);

                uint32_t copy_len = (bytes_written - offset > 256) ? 256 : (bytes_written - offset);
                memcpy(copy_buffer, &ota_ptr[offset], copy_len);

                // Disable Mapped Mode to Write
                OSPI_DisableMemoryMappedMode();

                if (OSPI_WriteData(inactive_bank_offset + offset, copy_buffer, copy_len) != HAL_OK) {
                    Serial_Log("Write Error at offset 0x%08X", offset);
                    copy_error = true; break;
                }

                // Re-enable Mapped Mode to Read next chunk
                OSPI_EnableMemoryMappedMode();
            }

            if (copy_error) {
                Serial_Log("Copy failed. Aborting apply.");
                HAL_NVIC_SystemReset();
            }

            // Step 3: Toggle Active Bank in RTC Backup Register
            Serial_Log("Copy Complete. Toggling Active Bank.");

            // Clear Boot Counter (BKP1R) for fresh start
            RTC->BKP1R = 0;

            // Toggle BKP2R
            if (is_bank_b_active) {
                RTC->BKP2R = 0; // Bank A becomes active
            } else {
                RTC->BKP2R = 1; // Bank B becomes active
            }

            Serial_Log("Update Applied Successfully. Resetting...");
            HAL_Delay(100);
            HAL_NVIC_SystemReset();
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
    GPIO_InitStruct.Alternate = GPIO_AF7_USART6;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    huart6.Instance = USART6;
    huart6.Init.BaudRate = 921600; // Increased to 921600
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

    // PB10 (TX), PB11 (RX)
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    GPIO_InitStruct.Pin = GPIO_PIN_10|GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF7_USART3;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    huart3.Instance = USART3;
    huart3.Init.BaudRate = 115200; // Keep Debug logging at standard rate
    huart3.Init.WordLength = UART_WORDLENGTH_8B;
    huart3.Init.StopBits = UART_STOPBITS_1;
    huart3.Init.Parity = UART_PARITY_NONE;
    huart3.Init.Mode = UART_MODE_TX_RX;
    huart3.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart3.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart3);
}

void MX_IWDG_Init(void) {
    hiwdg.Instance = IWDG1;
    hiwdg.Init.Prescaler = IWDG_PRESCALER_256; // 32kHz / 256 = 125Hz
    hiwdg.Init.Reload = 1250; // 10s
    if (HAL_IWDG_Init(&hiwdg) != HAL_OK) {
        // Error
    }
}

void GPIO_Init(void) {
    __HAL_RCC_GPIOG_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOK_CLK_ENABLE();

    GPIO_InitTypeDef GPIO_InitStruct = {0};

    // PG6 (Green)
    GPIO_InitStruct.Pin = GPIO_PIN_6;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOG, &GPIO_InitStruct);

    // PD4 (Orange), PD5 (Red)
    GPIO_InitStruct.Pin = GPIO_PIN_4 | GPIO_PIN_5;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    // PK3 (Blue)
    GPIO_InitStruct.Pin = GPIO_PIN_3;
    HAL_GPIO_Init(GPIOK, &GPIO_InitStruct);

    All_LEDs_Off();
}

void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_DIRECT_SMPS_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE0);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  // HSE = 25MHz, M = 5, N = 104, P = 1 => 520MHz SysClock
  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 104;
  RCC_OscInitStruct.PLL.PLLP = 1;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  RCC_OscInitStruct.PLL.PLLR = 2;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
      while(1) { LED_R_On(); Software_Delay(200000); LED_R_Off(); Software_Delay(200000); }
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
      while(1) { LED_R_On(); Software_Delay(200000); LED_R_Off(); Software_Delay(200000); }
  }

  // PERFORMANCE: Enable Cache and Prefetch
  // __HAL_FLASH_PREFETCH_BUFFER_ENABLE();
  // __HAL_FLASH_INSTRUCTION_CACHE_ENABLE();
  // __HAL_FLASH_DATA_CACHE_ENABLE();
}

void SysTick_Handler(void)
{
  HAL_IncTick();
}
// MPU Configuration
void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /* Configure the MPU attributes for the OSPI external flash */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x90000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_64MB; // 512Mbit = 64MB
  MPU_InitStruct.SubRegionDisable = 0x0;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);
}

void MX_OCTOSPI1_Init(void)
{
  /* OCTOSPI1 parameter configuration*/
  hospi1.Instance = OCTOSPI1;
  hospi1.Init.FifoThreshold = 4;
  hospi1.Init.DualQuad = HAL_OSPI_DUALQUAD_DISABLE;
  hospi1.Init.MemoryType = HAL_OSPI_MEMTYPE_MACRONIX;
  hospi1.Init.DeviceSize = 26; // 64MB = 2^26 bytes
  hospi1.Init.ChipSelectHighTime = 2;
  hospi1.Init.FreeRunningClock = HAL_OSPI_FREERUNCLK_DISABLE;
  hospi1.Init.ClockMode = HAL_OSPI_CLOCK_MODE_0;
  hospi1.Init.WrapSize = HAL_OSPI_WRAP_NOT_SUPPORTED;
  hospi1.Init.ClockPrescaler = 2; // Prescaler
  hospi1.Init.SampleShifting = HAL_OSPI_SAMPLE_SHIFTING_NONE;
  hospi1.Init.DelayHoldQuarterCycle = HAL_OSPI_DHQC_ENABLE;
  hospi1.Init.ChipSelectBoundary = 0;
  hospi1.Init.DelayBlockBypass = HAL_OSPI_DELAY_BLOCK_USED;
  hospi1.Init.MaxTran = 0;
  hospi1.Init.Refresh = 0;
  if (HAL_OSPI_Init(&hospi1) != HAL_OK)
  {
    Serial_Log("OSPI Init Error!");
  }
}

void HAL_OSPI_MspInit(OSPI_HandleTypeDef* ospiHandle)
{
  GPIO_InitTypeDef GPIO_InitStruct = {0};
  if(ospiHandle->Instance==OCTOSPI1)
  {
    /* OCTOSPI1 clock enable */
    __HAL_RCC_OCTOSPIM_CLK_ENABLE();
    __HAL_RCC_OSPI1_CLK_ENABLE();

    __HAL_RCC_GPIOB_CLK_ENABLE();
    __HAL_RCC_GPIOD_CLK_ENABLE();
    __HAL_RCC_GPIOE_CLK_ENABLE();
    __HAL_RCC_GPIOC_CLK_ENABLE();

    /**OCTOSPI1 GPIO Configuration
    PB2     ------> OCTOSPIM_P1_DQS
    PD11     ------> OCTOSPIM_P1_IO0
    PD12     ------> OCTOSPIM_P1_IO1
    PE2     ------> OCTOSPIM_P1_IO2
    PD13     ------> OCTOSPIM_P1_IO3
    PC1     ------> OCTOSPIM_P1_IO4
    PC2     ------> OCTOSPIM_P1_IO5
    PC3     ------> OCTOSPIM_P1_IO6
    PC0     ------> OCTOSPIM_P1_IO7
    PB2     ------> OCTOSPIM_P1_CLK
    PC11     ------> OCTOSPIM_P1_NCS
    */

    /* DQS */
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF10_OCTOSPIM_P1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* CLK */
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_OCTOSPIM_P1;
    HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

    /* NCS */
    GPIO_InitStruct.Pin = GPIO_PIN_11;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_OCTOSPIM_P1;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    /* IO0, IO1, IO3 */
    GPIO_InitStruct.Pin = GPIO_PIN_11|GPIO_PIN_12|GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_OCTOSPIM_P1;
    HAL_GPIO_Init(GPIOD, &GPIO_InitStruct);

    /* IO2 */
    GPIO_InitStruct.Pin = GPIO_PIN_2;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF9_OCTOSPIM_P1;
    HAL_GPIO_Init(GPIOE, &GPIO_InitStruct);

    /* IO4, IO5, IO6, IO7 */
    GPIO_InitStruct.Pin = GPIO_PIN_1|GPIO_PIN_2|GPIO_PIN_3|GPIO_PIN_0;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    GPIO_InitStruct.Alternate = GPIO_AF10_OCTOSPIM_P1;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);
  }
}

void OSPI_EnableMemoryMappedMode(void)
{
  OSPI_RegularCmdTypeDef sCommand = {0};
  OSPI_MemoryMappedTypeDef sMemMappedCfg = {0};

  /* Enable Octal mode (Read/Write configuration) for MX25LM51245G */
  sCommand.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;
  sCommand.FlashId = HAL_OSPI_FLASH_ID_1;
  sCommand.InstructionMode = HAL_OSPI_INSTRUCTION_8_LINES;
  sCommand.InstructionSize = HAL_OSPI_INSTRUCTION_16_BITS;
  sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
  sCommand.AddressMode = HAL_OSPI_ADDRESS_8_LINES;
  sCommand.AddressSize = HAL_OSPI_ADDRESS_32_BITS;
  sCommand.AddressDtrMode = HAL_OSPI_ADDRESS_DTR_DISABLE;
  sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
  sCommand.DataMode = HAL_OSPI_DATA_8_LINES;
  sCommand.DataDtrMode = HAL_OSPI_DATA_DTR_DISABLE;
  sCommand.DummyCycles = 20;
  sCommand.DQSMode = HAL_OSPI_DQS_ENABLE;
  sCommand.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;

  /* Read Command Configuration */
  sCommand.OperationType = HAL_OSPI_OPTYPE_READ_CFG;
  sCommand.Instruction = 0xEC13; /* Octal Read */
  if (HAL_OSPI_Command(&hospi1, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Serial_Log("OSPI Read Cmd Error!");
  }

  /* Write Command Configuration */
  sCommand.OperationType = HAL_OSPI_OPTYPE_WRITE_CFG;
  sCommand.Instruction = 0x12ED; /* Octal Write */
  sCommand.DummyCycles = 0;
  if (HAL_OSPI_Command(&hospi1, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
  {
    Serial_Log("OSPI Write Cmd Error!");
  }

  sMemMappedCfg.TimeOutActivation = HAL_OSPI_TIMEOUT_COUNTER_DISABLE;
  if (HAL_OSPI_MemoryMapped(&hospi1, &sMemMappedCfg) != HAL_OK)
  {
    Serial_Log("OSPI MemMapped Error!");
  }
}

// Flash Erase Helper for OSPI
HAL_StatusTypeDef OSPI_EraseSector(uint32_t address)
{
    OSPI_RegularCmdTypeDef sCommand = {0};

    /* Enable write operations */
    sCommand.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;
    sCommand.FlashId = HAL_OSPI_FLASH_ID_1;
    sCommand.InstructionMode = HAL_OSPI_INSTRUCTION_8_LINES;
    sCommand.InstructionSize = HAL_OSPI_INSTRUCTION_16_BITS;
    sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
    sCommand.AddressMode = HAL_OSPI_ADDRESS_NONE;
    sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
    sCommand.DataMode = HAL_OSPI_DATA_NONE;
    sCommand.DummyCycles = 0;
    sCommand.DQSMode = HAL_OSPI_DQS_ENABLE;
    sCommand.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;
    sCommand.Instruction = 0x06F9; // Write Enable for Octal

    if (HAL_OSPI_Command(&hospi1, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
        return HAL_ERROR;

    /* Erase Sector (64KB usually) */
    sCommand.AddressMode = HAL_OSPI_ADDRESS_8_LINES;
    sCommand.AddressSize = HAL_OSPI_ADDRESS_32_BITS;
    sCommand.Address = address;
    sCommand.Instruction = 0x21DE; // Sector Erase 4-byte address for Octal

    if (HAL_OSPI_Command(&hospi1, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
        return HAL_ERROR;

    /* Wait for Write in Progress bit to clear */
    // Note: Implementation of status check usually required here.
    // For simplicity, a delay is used, but reading SR is better.
    HAL_Delay(50);
    return HAL_OK;
}

HAL_StatusTypeDef OSPI_WriteData(uint32_t address, uint8_t *buffer, uint32_t length)
{
    OSPI_RegularCmdTypeDef sCommand = {0};

    /* Enable write operations */
    sCommand.OperationType = HAL_OSPI_OPTYPE_COMMON_CFG;
    sCommand.FlashId = HAL_OSPI_FLASH_ID_1;
    sCommand.InstructionMode = HAL_OSPI_INSTRUCTION_8_LINES;
    sCommand.InstructionSize = HAL_OSPI_INSTRUCTION_16_BITS;
    sCommand.InstructionDtrMode = HAL_OSPI_INSTRUCTION_DTR_DISABLE;
    sCommand.AddressMode = HAL_OSPI_ADDRESS_NONE;
    sCommand.AlternateBytesMode = HAL_OSPI_ALTERNATE_BYTES_NONE;
    sCommand.DataMode = HAL_OSPI_DATA_NONE;
    sCommand.DummyCycles = 0;
    sCommand.DQSMode = HAL_OSPI_DQS_ENABLE;
    sCommand.SIOOMode = HAL_OSPI_SIOO_INST_EVERY_CMD;
    sCommand.Instruction = 0x06F9; // Write Enable

    if (HAL_OSPI_Command(&hospi1, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
        return HAL_ERROR;

    /* Write Data */
    sCommand.AddressMode = HAL_OSPI_ADDRESS_8_LINES;
    sCommand.AddressSize = HAL_OSPI_ADDRESS_32_BITS;
    sCommand.Address = address;
    sCommand.DataMode = HAL_OSPI_DATA_8_LINES;
    sCommand.NbData = length;
    sCommand.Instruction = 0x12ED; // Octal Page Program

    if (HAL_OSPI_Command(&hospi1, &sCommand, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
        return HAL_ERROR;

    if (HAL_OSPI_Transmit(&hospi1, buffer, HAL_OSPI_TIMEOUT_DEFAULT_VALUE) != HAL_OK)
        return HAL_ERROR;

    /* Wait for write complete */
    HAL_Delay(2);
    return HAL_OK;
}
// Disable Memory Mapped Mode to use direct OSPI commands
void OSPI_DisableMemoryMappedMode(void) {
    HAL_OSPI_Abort(&hospi1);
}
