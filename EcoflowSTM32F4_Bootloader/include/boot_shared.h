#ifndef BOOTLOADER_SHARED_H
#define BOOTLOADER_SHARED_H

#include <stdint.h>

// Flash Sectors for STM32F469NI (2MB)
// Sector 0: 0x08000000 - 0x08003FFF (16KB) - Bootloader A
// Sector 1: 0x08004000 - 0x08007FFF (16KB) - Config A
// Sector 2: 0x08008000 - 0x0800BFFF (16KB) - App A Start
// ...
// Bank 2 (1MB Offset)
// Sector 12: 0x08100000 - 0x08103FFF (16KB) - Bootloader B (Copy)
// Sector 13: 0x08104000 - 0x08107FFF (16KB) - Config B
// Sector 14: 0x08108000 - 0x0810BFFF (16KB) - App B Start

// Address Definitions
#define BOOTLOADER_A_ADDR   0x08000000
#define CONFIG_A_ADDR       0x08004000
#define APP_A_ADDR          0x08008000

#define BOOTLOADER_B_ADDR   0x08100000
#define CONFIG_B_ADDR       0x08104000
#define APP_B_ADDR          0x08108000

// Magic Value to identify valid config
#define CONFIG_MAGIC        0xDEADBEEF

typedef struct {
    uint32_t magic;
    uint32_t active_bank; // 0 = Bank 1 (App A), 1 = Bank 2 (App B)
    uint32_t update_pending; // 0 = No, 1 = Yes
    uint32_t target_bank; // Bank to update
    uint32_t crc32; // CRC of the config struct itself (excluding this field)
    uint32_t image_size;
    uint32_t image_crc;
} BootConfig;

#endif
