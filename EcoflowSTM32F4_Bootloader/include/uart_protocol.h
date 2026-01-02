#ifndef UART_PROTOCOL_H
#define UART_PROTOCOL_H

#include <stdint.h>

#define OTA_CMD_START 0xF0
#define OTA_CMD_DATA  0xF1
#define OTA_CMD_END   0xF2
#define OTA_ACK       0xAA
#define OTA_NACK      0xFF

#define START_BYTE    0xAA

// Structure:
// [START][CMD][LEN][PAYLOAD...][CRC]
// START: 1B
// CMD:   1B
// LEN:   1B (Length of payload)
// PAYLOAD: N Bytes
// CRC:   1B (CRC8 of CMD+LEN+PAYLOAD)

#endif
