#ifndef BOOT_SHARED_H
#define BOOT_SHARED_H

#include <stdint.h>

// Flash Sector Definitions for STM32F469NI (2MB)
// Bank 1
#define SECTOR_BOOTLOADER   FLASH_SECTOR_0  // 0x0800 0000 - 16KB
#define SECTOR_CONFIG       FLASH_SECTOR_1  // 0x0800 4000 - 16KB
#define SECTOR_APP_A_START  FLASH_SECTOR_2  // 0x0800 8000
#define SECTOR_APP_A_END    FLASH_SECTOR_11 // 0x080F FFFF

// Bank 2 (Starts at 0x0810 0000)
#define SECTOR_APP_B_START  FLASH_SECTOR_12 // 0x0810 0000 (We use this as start of Staging)
#define SECTOR_APP_B_END    FLASH_SECTOR_23 // 0x081F FFFF

// Addresses
#define ADDR_BOOTLOADER     0x08000000
#define ADDR_CONFIG         0x08004000
#define ADDR_APP_A          0x08008000
#define ADDR_APP_B          0x08100000

// Config Sector Data Structure
#define BOOT_MAGIC_UPDATE_PENDING  0xDEADBEEF
#define BOOT_MAGIC_NORMAL          0xFFFFFFFF

typedef struct {
    uint32_t magic;         // 0xDEADBEEF if update pending
    uint32_t size;          // Size of new firmware
    uint32_t checksum;      // CRC32 of new firmware
    uint32_t reserved[29];  // Padding
} BootConfig;

#endif // BOOT_SHARED_H
