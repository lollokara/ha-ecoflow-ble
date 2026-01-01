#include "ota_core.h"
#include "flash_writer.h"
#include "uart_task.h" // For sending responses
#include <stdio.h>
#include <string.h>

static bool ota_active = false;
static uint32_t expected_size = 0;
static uint32_t received_size = 0;

void OTA_Init(void) {
    FlashWriter_Init();
    ota_active = false;
}

bool OTA_IsActive(void) {
    return ota_active;
}

void OTA_ProcessCommand(uint8_t cmd, uint8_t* payload, uint16_t len) {
    uint8_t resp = OTA_RESP_ERROR;

    switch (cmd) {
        case OTA_CMD_START:
            if (len >= 4) {
                // Payload: [Total Size (4 bytes)]
                expected_size = (payload[0] << 24) | (payload[1] << 16) | (payload[2] << 8) | payload[3];
                received_size = 0;
                printf("OTA Start. Size: %lu\n", expected_size);

                if (FlashWriter_PrepareBank(expected_size)) {
                    ota_active = true;
                    resp = OTA_RESP_OK;
                }
            }
            break;

        case OTA_CMD_DATA:
            if (ota_active) {
                // Payload: [Data...]
                if (FlashWriter_WriteChunk(received_size, payload, len)) {
                    received_size += len;
                    resp = OTA_RESP_OK;
                    // Optional: Toggle LED or update screen progress
                } else {
                    printf("OTA Write Failed at %lu\n", received_size);
                }
            }
            break;

        case OTA_CMD_END:
            if (ota_active) {
                // Payload: [CRC32 (4 bytes)]
                if (len >= 4) {
                    uint32_t received_crc = (payload[0] << 24) | (payload[1] << 16) | (payload[2] << 8) | payload[3];
                    printf("OTA End. Rx Size: %lu, CRC: 0x%08lX\n", received_size, received_crc);

                    if (received_size == expected_size) {
                        if (FlashWriter_Finalize(expected_size, received_crc)) {
                             resp = OTA_RESP_OK;
                             // System will reset in Finalize
                        } else {
                            printf("OTA Finalize Failed\n");
                        }
                    } else {
                        printf("Size mismatch! Exp: %lu, Act: %lu\n", expected_size, received_size);
                    }
                }
                ota_active = false;
            }
            break;
    }

    // Silence compiler warning about unused variable if debug is disabled
    (void)resp;
}
