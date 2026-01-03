#include <Arduino.h>
#include "hardware/watchdog.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>

// --- Config ---
#define FAN1_PWM_PIN 26
#define FAN2_PWM_PIN 15
#define FAN3_PWM_PIN 12
#define FAN4_PWM_PIN 13

#define FAN1_TACH_PIN 27
#define FAN2_TACH_PIN 14
#define FAN3_TACH_PIN 16
#define FAN4_TACH_PIN 17

#define TEMP_PIN 4  // Updated

// Using Serial1 on default pins for this core/board?
// The user specified "PA1" for STM32 RX (UART4).
// The user specified "PA2" for STM32 TX (USART2).
// RP2040 Pins:
// #define UART_TX_PIN 0  (RP2040 TX -> STM32 RX)
// #define UART_RX_PIN 1  (RP2040 RX <- STM32 TX)
#define UART_TX_PIN 0
#define UART_RX_PIN 1

#define EEPROM_SIZE 512
#define CONFIG_ADDR 0

// --- Protocol (Must match STM32) ---
#define FAN_UART_START_BYTE 0xBB
#define FAN_CMD_STATUS      0x01
#define FAN_CMD_SET_CONFIG  0x02
#define FAN_CMD_GET_CONFIG  0x03
#define FAN_CMD_CONFIG_RESP 0x04

typedef struct {
    uint16_t min_speed;
    uint16_t max_speed;
    uint8_t start_temp;
    uint8_t max_temp;
} FanGroupConfig;

typedef struct {
    FanGroupConfig group1;
    FanGroupConfig group2;
} FanConfig;

// --- State ---
FanConfig config;
float currentTemp = 0.0f;
volatile uint32_t fan_pulses[4] = {0};
uint16_t fan_rpm[4] = {0};
uint32_t last_tach_time = 0;
uint32_t last_ctrl_time = 0;

OneWire oneWire(TEMP_PIN);
DallasTemperature sensors(&oneWire);

// --- Tacho ISRs ---
void onPulseFan1() { fan_pulses[0]++; }
void onPulseFan2() { fan_pulses[1]++; }
void onPulseFan3() { fan_pulses[2]++; }
void onPulseFan4() { fan_pulses[3]++; }

// --- UART Helper ---
uint8_t calc_crc8(uint8_t *data, size_t len) {
    uint8_t crc = 0;
    for (size_t i = 0; i < len; i++) {
        crc ^= data[i];
        for (int j = 0; j < 8; j++) {
            if (crc & 0x80) crc = (crc << 1) ^ 0x07;
            else crc <<= 1;
        }
    }
    return crc;
}

void send_packet(uint8_t cmd, uint8_t *payload, uint8_t len) {
    uint8_t packet[32];
    packet[0] = FAN_UART_START_BYTE;
    packet[1] = cmd;
    packet[2] = len;
    if (len > 0) memcpy(&packet[3], payload, len);
    packet[3 + len] = calc_crc8(packet, 3 + len);

    Serial1.write(packet, 3 + len + 1);

    // Debug Log Packet Sent
    Serial.printf("TX CMD=%02X LEN=%d: ", cmd, len);
    for(int i=0; i<3+len+1; i++) Serial.printf("%02X ", packet[i]);
    Serial.println();
}

// --- Logic ---
void load_config() {
    EEPROM.begin(EEPROM_SIZE);
    EEPROM.get(CONFIG_ADDR, config);
    // Validate
    if (config.group1.max_speed > 10000 || config.group1.min_speed > 10000) {
        // Defaults
        config.group1 = {500, 3000, 30, 45};
        config.group2 = {500, 3000, 30, 45};
    }
    Serial.println("Config Loaded");
}

void save_config() {
    EEPROM.put(CONFIG_ADDR, config);
    EEPROM.commit();
    Serial.println("Config Saved");
}

int calc_pwm(FanGroupConfig *grp, float temp) {
    if (temp < grp->start_temp) return 0; // Off below start
    if (temp >= grp->max_temp) return 255; // Max speed

    // Linear interpolation
    float temp_range = grp->max_temp - grp->start_temp;
    float temp_delta = temp - grp->start_temp;
    float factor = temp_delta / temp_range;

    int target_rpm_min = grp->min_speed;
    int target_rpm_max = grp->max_speed;

    int target_rpm = target_rpm_min + (int)(factor * (target_rpm_max - target_rpm_min));

    // Map RPM to PWM (Approximate: 0-5000 RPM -> 0-255 PWM)
    int pwm = map(target_rpm, 0, 5000, 0, 255);

    // Ensure minimum start-up if target > 0
    if (pwm > 0 && pwm < 50) pwm = 50;

    return constrain(pwm, 0, 255);
}

void setup() {
    // Debug Serial
    Serial.begin(115200);
    // Note: Not waiting for Serial to allow headless operation
    delay(1000);
    Serial.println("RP2040 Fan Controller Starting...");

    // Hardware UART to STM32
    Serial1.setTX(UART_TX_PIN);
    Serial1.setRX(UART_RX_PIN);
    Serial1.begin(115200);

    analogWriteFreq(25000); // 25kHz standard for fans
    analogWriteRange(255);

    pinMode(FAN1_TACH_PIN, INPUT_PULLUP);
    pinMode(FAN2_TACH_PIN, INPUT_PULLUP);
    pinMode(FAN3_TACH_PIN, INPUT_PULLUP);
    pinMode(FAN4_TACH_PIN, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(FAN1_TACH_PIN), onPulseFan1, FALLING);
    attachInterrupt(digitalPinToInterrupt(FAN2_TACH_PIN), onPulseFan2, FALLING);
    attachInterrupt(digitalPinToInterrupt(FAN3_TACH_PIN), onPulseFan3, FALLING);
    attachInterrupt(digitalPinToInterrupt(FAN4_TACH_PIN), onPulseFan4, FALLING);

    sensors.begin();
    load_config();

    // Enable Watchdog (8s timeout, pause on debug)
    watchdog_enable(8000, 1);
}

void loop() {
    watchdog_update();
    uint32_t now = millis();

    // 1. Read Temp & Control Fans (Every 200ms)
    if (now - last_ctrl_time > 200) {
        last_ctrl_time = now;
        sensors.requestTemperatures();
        currentTemp = sensors.getTempCByIndex(0);
        if (currentTemp == DEVICE_DISCONNECTED_C) currentTemp = 0.0f;

        // Group 1: Fans 1 & 2
        int pwm1 = calc_pwm(&config.group1, currentTemp);
        analogWrite(FAN1_PWM_PIN, pwm1);
        analogWrite(FAN2_PWM_PIN, pwm1);

        // Group 2: Fans 3 & 4
        int pwm2 = calc_pwm(&config.group2, currentTemp);
        analogWrite(FAN3_PWM_PIN, pwm2);
        analogWrite(FAN4_PWM_PIN, pwm2);

        // Log Debug
        // Reduce log spam, maybe every 1s? Or just rely on print
        // Serial.printf("Temp: %.2f C | PWM1: %d | PWM2: %d\n", currentTemp, pwm1, pwm2);
    }

    // 2. Read Tach & Send Status (Every 1000ms)
    if (now - last_tach_time > 1000) {
        last_tach_time = now;

        for(int i=0; i<4; i++) {
            noInterrupts();
            uint32_t pulses = fan_pulses[i];
            fan_pulses[i] = 0;
            interrupts();

            fan_rpm[i] = (pulses * 60) / 2;
        }

        Serial.printf("Temp: %.2f C | RPM: %d %d %d %d\n", currentTemp, fan_rpm[0], fan_rpm[1], fan_rpm[2], fan_rpm[3]);

        // Send Status
        uint8_t payload[12];
        memcpy(&payload[0], &currentTemp, 4);
        memcpy(&payload[4], &fan_rpm[0], 2);
        memcpy(&payload[6], &fan_rpm[1], 2);
        memcpy(&payload[8], &fan_rpm[2], 2);
        memcpy(&payload[10], &fan_rpm[3], 2);

        send_packet(FAN_CMD_STATUS, payload, 12);
    }

    // 3. Handle UART Reception
    if (Serial1.available()) {
        static uint8_t rxBuf[64];
        static int rxIdx = 0;

        while (Serial1.available()) {
            uint8_t b = Serial1.read();
            if (rxIdx == 0 && b != FAN_UART_START_BYTE) {
                 // Serial.printf("Skip %02X\n", b);
                 continue; // Sync
            }

            rxBuf[rxIdx++] = b;

            // Check Header
            if (rxIdx >= 3) {
                uint8_t len = rxBuf[2];
                if (rxIdx == 3 + len + 1) {
                    // Full Packet
                    uint8_t crc = calc_crc8(rxBuf, 3 + len);
                    if (crc == rxBuf[3 + len]) {
                        // Valid
                        uint8_t cmd = rxBuf[1];
                        Serial.printf("RX CMD=%02X LEN=%d\n", cmd, len);

                        if (cmd == FAN_CMD_SET_CONFIG) {
                            if (len == sizeof(FanConfig)) {
                                memcpy(&config, &rxBuf[3], sizeof(FanConfig));
                                save_config();
                            }
                        } else if (cmd == FAN_CMD_GET_CONFIG) {
                            send_packet(FAN_CMD_CONFIG_RESP, (uint8_t*)&config, sizeof(FanConfig));
                        }
                    } else {
                        Serial.printf("CRC Fail: Calc=%02X Recv=%02X\n", crc, rxBuf[3+len]);
                    }
                    rxIdx = 0;
                }
            }
            if (rxIdx >= 64) rxIdx = 0; // Overflow safety
        }
    }
}
