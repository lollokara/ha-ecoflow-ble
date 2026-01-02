#include <stdio.h>
#include <string.h>
#include "stm32f4xx_hal.h"
#include "flash_ops.h"

extern UART_HandleTypeDef huart3;

// Simple CLI Buffer
#define CLI_BUFFER_SIZE 64
static char cli_buffer[CLI_BUFFER_SIZE];
static uint8_t cli_index = 0;

void ProcessCLI(void) {
    uint8_t rx_char;
    // Non-blocking read? No, use HAL polling or interrupt logic from main?
    // For simplicity, we just poll here in the main loop if nothing else is happening.

    if (HAL_UART_Receive(&huart3, &rx_char, 1, 0) == HAL_OK) {
        // Echo
        HAL_UART_Transmit(&huart3, &rx_char, 1, 10);

        if (rx_char == '\r' || rx_char == '\n') {
            HAL_UART_Transmit(&huart3, (uint8_t*)"\r\n", 2, 10);
            cli_buffer[cli_index] = 0;

            if (strcmp(cli_buffer, "info") == 0) {
                printf("Bootloader v1.0\r\n");
                printf("Active Bank: %d\r\n", Flash_GetActiveBank());
                printf("OPTCR: 0x%08X\r\n", (unsigned int)FLASH->OPTCR);
            }
            else if (strcmp(cli_buffer, "toggle") == 0) {
                printf("Toggling Bank...\r\n");
                Flash_ToggleBank();
                HAL_NVIC_SystemReset();
            }
            else if (strcmp(cli_buffer, "erase1") == 0) {
                // Erase Bank 1 App (Sector 2-11)
                Flash_Unlock();
                printf("Erasing Bank 1 App...\r\n");
                for(int i=2; i<=11; i++) Flash_EraseSector(i);
                Flash_Lock();
                printf("Done.\r\n");
            }
            else if (strcmp(cli_buffer, "erase2") == 0) {
                // Erase Bank 2 App (Sector 14-23)
                Flash_Unlock();
                printf("Erasing Bank 2 App...\r\n");
                for(int i=14; i<=23; i++) Flash_EraseSector(i);
                Flash_Lock();
                printf("Done.\r\n");
            }
            else if (strcmp(cli_buffer, "help") == 0) {
                printf("Commands: info, toggle, erase1, erase2\r\n");
            }

            cli_index = 0;
        } else {
            if (cli_index < CLI_BUFFER_SIZE - 1) {
                cli_buffer[cli_index++] = rx_char;
            }
        }
    }
}
