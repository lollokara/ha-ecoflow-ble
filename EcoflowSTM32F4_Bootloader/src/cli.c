#include "main.h"
#include <stdio.h>
#include <string.h>

// Externs
extern UART_HandleTypeDef huart3;
extern void Flash_ToggleBank(void);
extern uint32_t Flash_GetActiveBank(void);
extern void Flash_Unlock(void);
extern void Flash_Lock(void);
extern uint8_t Flash_EraseSector(uint32_t sector);
extern uint32_t GetPhysicalSector(uint32_t Address);

// Helper to read line
void CLI_ReadLine(char *buf, int max_len) {
    int idx = 0;
    uint8_t c;
    while(idx < max_len - 1) {
        if(HAL_UART_Receive(&huart3, &c, 1, 100) == HAL_OK) {
            if(c == '\r' || c == '\n') {
                buf[idx] = 0;
                printf("\r\n");
                return;
            }
            buf[idx++] = c;
            HAL_UART_Transmit(&huart3, &c, 1, 10); // Echo
        }
    }
    buf[max_len-1] = 0;
}

void CLI_Loop(void) {
    char cmd[64];

    printf("\r\n=== Bootloader CLI ===\r\n");
    printf("Commands: info, swap, erase <1|2>, boot, reboot\r\n");

    while(1) {
        printf("> ");
        CLI_ReadLine(cmd, 64);

        if(strcmp(cmd, "info") == 0) {
            uint32_t bank = Flash_GetActiveBank();
            printf("Active Bank: %d\r\n", (int)bank);
            uint32_t optcr = FLASH->OPTCR;
            printf("OPTCR: 0x%08X (BFB2=%d)\r\n", (unsigned int)optcr, (optcr >> 4) & 1);

        } else if(strcmp(cmd, "swap") == 0) {
            printf("Toggling Bank... System will reset.\r\n");
            Flash_ToggleBank();

        } else if(strncmp(cmd, "erase", 5) == 0) {
            // "erase 1" or "erase 2"
            int target = 0;
            if (strlen(cmd) > 6) target = cmd[6] - '0';

            if (target != 1 && target != 2) {
                 printf("Usage: erase <1|2>\r\n");
                 continue;
            }

            uint32_t active = Flash_GetActiveBank();
            if (target == active) {
                printf("Cannot erase Active Bank %d! Swap first.\r\n", target);
                continue;
            }

            printf("Erasing Bank %d (Inactive)...\r\n", target);
            Flash_Unlock();
            // Erase all sectors of inactive bank
            // If Active=1 (Bank 1), Inactive=2 (Phys 12-23)
            // If Active=2 (Bank 2), Inactive=1 (Phys 0-11)

            // Wait, GetPhysicalSector logic handles mapping via addresses.
            // Bank 2 starts at 0x08100000 (Logical Inactive Window) if active=1.
            // Bank 1 starts at 0x08100000 (Logical Inactive Window) if active=2.

            // So we erase range 0x08100000 to End
            uint32_t base = 0x08100000;
            // Erase 12 sectors
            for (int i=0; i<12; i++) {
                // Approximate addresses to hit each sector
                // This is lazy but works with GetPhysicalSector
                // S12/0: +0
                // S13/1: +4000
                // S14/2: +8000
                // ...
                // Better to iterate sector numbers directly?
                // Yes, but which ones?

                uint32_t sector_to_erase = 0;
                if (active == 1) sector_to_erase = FLASH_SECTOR_12 + i;
                else sector_to_erase = FLASH_SECTOR_0 + i;

                printf("Erasing Sector %d...\r\n", (int)sector_to_erase);
                Flash_EraseSector(sector_to_erase);
            }
            Flash_Lock();
            printf("Done.\r\n");

        } else if(strcmp(cmd, "boot") == 0) {
            printf("Exiting CLI, attempting boot...\r\n");
            return;

        } else if(strcmp(cmd, "reboot") == 0) {
            HAL_NVIC_SystemReset();

        } else if (strlen(cmd) > 0) {
            printf("Unknown command.\r\n");
        }
    }
}
