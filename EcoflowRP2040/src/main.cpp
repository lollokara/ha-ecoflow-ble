#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>

// --- Config ---
#define FAN1_PWM_PIN 10
#define FAN2_PWM_PIN 11
#define FAN3_PWM_PIN 12
#define FAN4_PWM_PIN 13

#define FAN1_TACH_PIN 14
#define FAN2_TACH_PIN 15
#define FAN3_TACH_PIN 16
#define FAN4_TACH_PIN 17

#define TEMP_PIN 18

// Using Serial1 on default pins for this core/board?
// The user specified pin list for STM32, not RP2040. RP2040 pins 8/9 are UART1 default.
// The user specified "PA1" for STM32. We need to match.
#define UART_TX_PIN 8
#define UART_RX_PIN 9

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
}

void save_config() {
    EEPROM.put(CONFIG_ADDR, config);
    EEPROM.commit();
}

int calc_pwm(FanGroupConfig *grp, float temp) {
    if (temp < grp->start_temp) return 0; // Off below start
    if (temp >= grp->max_temp) return 255; // Max speed

    // Linear interpolation of temperature factor (0.0 to 1.0)
    float temp_range = grp->max_temp - grp->start_temp;
    float temp_delta = temp - grp->start_temp;
    float factor = temp_delta / temp_range;

    // Map Configured RPM to PWM Duty Cycle (0-255)
    // Heuristic:
    // Min Config Speed (e.g. 500 RPM) -> ~20% PWM (50)
    // Max Config Speed (e.g. 3000 RPM) -> 100% PWM (255) if it matches fan max,
    // but here we map the *control range*.
    // User wants "Max Speed" at "Max Temp".
    // We assume 100% PWM achieves the fan's physical max.
    // If the user sets Max Speed to 3000, but the fan can do 5000, we should limit PWM?
    // Without knowing the fan curve, accurate RPM targeting is hard without PID.
    // **Simple Approach:**
    // Map the temperature factor (0-100%) to the PWM range (MinPWM - MaxPWM).
    // MinPWM is fixed at ~50 (start voltage). MaxPWM is 255.
    // We ignore the specific RPM values for *control* (open loop) but use them for *status* logic if we had PID.
    // However, the user specifically asked for "Min Fan Speed" and "Max Fan Speed" sliders.
    // Ideally, we would use PID to hold target RPM.
    // Given complexity, I will map the *User's Range* to PWM 0-255 effectively scaling the "power".
    // Actually, "Max Speed" slider might imply a limit.
    // Let's assume the user wants linear scaling between StartTemp and MaxTemp.
    // At StartTemp, we want fan to start (PWM ~50 or user Min Speed equivalent).
    // At MaxTemp, we want fan to be at user Max Speed equivalent.
    // Since we don't know RPM->PWM map, we will assume linear 0-5000 RPM = 0-255 PWM.

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
}

void loop() {
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
    }

    // 2. Read Tach & Send Status (Every 1000ms)
    if (now - last_tach_time > 1000) {
        last_tach_time = now;

        for(int i=0; i<4; i++) {
            // 2 pulses per revolution usually
            noInterrupts();
            uint32_t pulses = fan_pulses[i];
            fan_pulses[i] = 0;
            interrupts();

            fan_rpm[i] = (pulses * 60) / 2;
        }

        // Send Status
        // Payload: [Float Temp (4)][RPM1 (2)][RPM2 (2)][RPM3 (2)][RPM4 (2)]
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
            if (rxIdx == 0 && b != FAN_UART_START_BYTE) continue; // Sync

            rxBuf[rxIdx++] = b;

            // Check Header
            if (rxIdx >= 3) {
                uint8_t len = rxBuf[2];
                if (rxIdx == 3 + len + 1) {
                    // Full Packet
                    if (calc_crc8(rxBuf, 3 + len) == rxBuf[3 + len]) {
                        // Valid
                        uint8_t cmd = rxBuf[1];
                        if (cmd == FAN_CMD_SET_CONFIG) {
                            if (len == sizeof(FanConfig)) {
                                memcpy(&config, &rxBuf[3], sizeof(FanConfig));
                                save_config();
                            }
                        } else if (cmd == FAN_CMD_GET_CONFIG) {
                            send_packet(FAN_CMD_CONFIG_RESP, (uint8_t*)&config, sizeof(FanConfig));
                        }
                    }
                    rxIdx = 0;
                }
            }
            if (rxIdx >= 64) rxIdx = 0; // Overflow safety
        }
    }
}
