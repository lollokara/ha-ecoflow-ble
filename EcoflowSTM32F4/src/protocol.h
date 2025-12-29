#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <stdint.h>
#include <stddef.h>

#define START_BYTE 0xAA
#define MIN_PACKET_LEN 5 // START + CMD + LEN + CRC8
#define MAX_PAYLOAD_LEN 255

// Message Commands
#define CMD_BATTERY_STATUS 0x01
#define CMD_TEMPERATURE    0x02
#define CMD_CONNECTION     0x03
#define CMD_REQUEST_STATUS 0x10

typedef struct {
    uint8_t soc;
    int16_t power_w;
    uint16_t voltage_v; // mV * 10 (Wait, user said mV * 10? No, usually 100mV or similar. Let's stick to spec: "mV * 10")
    // Wait, user said: "voltage_V: uint16_t, // mV * 10".
    // If it's 12V -> 12000mV -> 120000. uint16_t max is 65535.
    // Maybe it means "100mV units"? or "Volts * 10" (12.5V -> 125)?
    // User wrote: "voltage_V: uint16_t, // mV * 10" which is ambiguous.
    // If it is 120V, 120000 fits in uint32_t.
    // Assuming standard "Volts * 100" or similar for now, but following the struct exactly.
    // Actually, "mV * 10" might mean the unit is 0.1mV? No, that's smaller.
    // Let's assume the payload simply contains bytes and I decode them.
    uint8_t connected;
    // device_name follows
} BatteryStatus;

uint8_t calculate_crc8(const uint8_t *data, size_t len);

#endif // PROTOCOL_H
