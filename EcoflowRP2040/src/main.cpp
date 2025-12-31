#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>

// --- Pins ---
// PWM: Fans 1-4
#define PIN_FAN1_PWM 10
#define PIN_FAN2_PWM 11
#define PIN_FAN3_PWM 12
#define PIN_FAN4_PWM 13

// Tach: Fans 1-4 (Interrupts)
#define PIN_FAN1_TACH 14
#define PIN_FAN2_TACH 15
#define PIN_FAN3_TACH 16
#define PIN_FAN4_TACH 17

// Temp Sensor
#define PIN_ONE_WIRE 18

// UART (Serial1 on Pico defaults to GP0/GP1 usually, but user specified pins 8/9 is UART1)
// We will configure Serial1 to use specific pins if needed, or rely on board def.
// Standard Arduino Pico (Earle Philhower): Serial1 is UART0 on GP0/GP1. Serial2 is UART1 on GP8/GP9.
// User requested: "Fan Information... The RP2040 will then comunicate with the F4 using another uart".
// The STM32 plan uses PA2 (TX) and PA1 (RX).
// RP2040 TX should go to STM32 RX (PA1).
// RP2040 RX should go to STM32 TX (PA2).

// We'll use Serial1.
// By default Serial1 might be GP0/GP1.
// We can set pins.

// --- Protocol ---
#define FAN_CMD_START 0xBB
#define CMD_FAN_STATUS 0x01
#define CMD_FAN_SET_CONFIG 0x02
#define CMD_FAN_GET_CONFIG 0x03
#define CMD_FAN_CONFIG_RESP 0x04

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

// Status Packet structure matches STM32
typedef struct __attribute__((packed)) {
    float amb_temp;
    uint16_t fan_rpm[4];
} FanStatusPayload;

// --- Globals ---
OneWire oneWire(PIN_ONE_WIRE);
DallasTemperature sensors(&oneWire);

FanConfig config;
volatile uint32_t fan_pulses[4] = {0};
uint16_t fan_rpms[4] = {0};
float current_temp = 25.0f;

// --- ISRs ---
void onPulseFan1() { fan_pulses[0]++; }
void onPulseFan2() { fan_pulses[1]++; }
void onPulseFan3() { fan_pulses[2]++; }
void onPulseFan4() { fan_pulses[3]++; }

// --- Helpers ---
void loadConfig() {
    EEPROM.begin(sizeof(FanConfig) + 1);
    if (EEPROM.read(0) == 0xA5) { // Magic byte
        EEPROM.get(1, config);
    } else {
        // Defaults
        config.group1.min_speed = 1000; config.group1.max_speed = 3000;
        config.group1.start_temp = 35; config.group1.max_temp = 50;
        config.group2 = config.group1;
    }
}

void saveConfig() {
    EEPROM.write(0, 0xA5);
    EEPROM.put(1, config);
    EEPROM.commit();
}

uint8_t calc_crc(uint8_t* data, uint8_t len) {
    uint8_t crc = 0;
    for (int i = 0; i < len; i++) {
        crc += data[i];
    }
    return crc;
}

void sendPacket(uint8_t cmd, uint8_t* payload, uint8_t len) {
    uint8_t header[3];
    header[0] = FAN_CMD_START;
    header[1] = cmd;
    header[2] = len;

    Serial1.write(header, 3);
    uint8_t crc = calc_crc(&header[1], 2);

    if (len > 0) {
        Serial1.write(payload, len);
        crc += calc_crc(payload, len);
    }

    Serial1.write(crc);
}

// --- Control Logic ---
// Maps temperature to a PWM duty cycle (0-255) based on RPM config.
// Assumes 0 RPM = 0 PWM and 5000 RPM (Max) = 255 PWM.
// A typical 4-pin fan might have a different curve, but linear approximation is sufficient here.
uint8_t map_pwm(FanGroupConfig* cfg, float temp) {
    if (temp < cfg->start_temp) return 0; // Fan Off below start temp

    // Calculate RPM target based on temperature interpolation
    uint16_t target_rpm;
    if (temp >= cfg->max_temp) {
        target_rpm = cfg->max_speed;
    } else {
        float temp_range = cfg->max_temp - cfg->start_temp;
        float temp_delta = temp - cfg->start_temp;
        float ratio = temp_delta / temp_range;

        target_rpm = cfg->min_speed + (uint16_t)((cfg->max_speed - cfg->min_speed) * ratio);
    }

    // Map Target RPM to PWM (0-255)
    // Assumption: 5000 RPM is 100% PWM (255)
    // Clamp at 255
    uint32_t pwm_val = (target_rpm * 255) / 5000;
    if (pwm_val > 255) pwm_val = 255;

    // Ensure minimum PWM to start fan if target > 0 (usually > 20% duty cycle needed)
    // If target_rpm > 0 but calculated pwm is too low, boost it?
    // For now, simple mapping.

    return (uint8_t)pwm_val;
}

void updateFans() {
    uint8_t pwm1 = map_pwm(&config.group1, current_temp);
    uint8_t pwm2 = map_pwm(&config.group1, current_temp);
    uint8_t pwm3 = map_pwm(&config.group2, current_temp);
    uint8_t pwm4 = map_pwm(&config.group2, current_temp);

    analogWrite(PIN_FAN1_PWM, pwm1);
    analogWrite(PIN_FAN2_PWM, pwm2);
    analogWrite(PIN_FAN3_PWM, pwm3);
    analogWrite(PIN_FAN4_PWM, pwm4);
}

// --- Parsing State ---
int p_state = 0;
uint8_t p_cmd, p_len, p_idx;
uint8_t p_buf[64];
uint8_t p_chk;

void processByte(uint8_t b) {
    switch(p_state) {
        case 0: if(b == FAN_CMD_START) p_state = 1; break;
        case 1: p_cmd = b; p_chk = b; p_state = 2; break;
        case 2: p_len = b; p_chk += b; p_idx = 0; p_state = (p_len > 0) ? 3 : 4; break;
        case 3:
            p_buf[p_idx++] = b; p_chk += b;
            if(p_idx >= p_len) p_state = 4;
            break;
        case 4:
            if (b == p_chk) {
                if (p_cmd == CMD_FAN_SET_CONFIG && p_len == sizeof(FanConfig)) {
                    memcpy(&config, p_buf, sizeof(FanConfig));
                    saveConfig();
                } else if (p_cmd == CMD_FAN_GET_CONFIG) {
                    sendPacket(CMD_FAN_CONFIG_RESP, (uint8_t*)&config, sizeof(FanConfig));
                }
            }
            p_state = 0;
            break;
    }
}

void setup() {
    Serial1.setTX(8);
    Serial1.setRX(9);
    Serial1.begin(115200);

    pinMode(PIN_FAN1_TACH, INPUT_PULLUP);
    pinMode(PIN_FAN2_TACH, INPUT_PULLUP);
    pinMode(PIN_FAN3_TACH, INPUT_PULLUP);
    pinMode(PIN_FAN4_TACH, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(PIN_FAN1_TACH), onPulseFan1, RISING);
    attachInterrupt(digitalPinToInterrupt(PIN_FAN2_TACH), onPulseFan2, RISING);
    attachInterrupt(digitalPinToInterrupt(PIN_FAN3_TACH), onPulseFan3, RISING);
    attachInterrupt(digitalPinToInterrupt(PIN_FAN4_TACH), onPulseFan4, RISING);

    pinMode(PIN_FAN1_PWM, OUTPUT);
    pinMode(PIN_FAN2_PWM, OUTPUT);
    pinMode(PIN_FAN3_PWM, OUTPUT);
    pinMode(PIN_FAN4_PWM, OUTPUT);

    sensors.begin();
    loadConfig();
}

uint32_t last_ctrl = 0;
uint32_t last_tach = 0;

void loop() {
    uint32_t now = millis();

    while(Serial1.available()) {
        processByte(Serial1.read());
    }

    if (now - last_ctrl > 200) {
        last_ctrl = now;
        sensors.requestTemperatures();
        float t = sensors.getTempCByIndex(0);
        if (t > -127) current_temp = t;
        updateFans();
    }

    if (now - last_tach > 1000) {
        last_tach = now;
        for(int i=0; i<4; i++) {
            // 2 pulses per rev usually
            fan_rpms[i] = (fan_pulses[i] * 60) / 2;
            fan_pulses[i] = 0;
        }

        // Broadcast Status
        FanStatusPayload pl;
        pl.amb_temp = current_temp;
        memcpy(pl.fan_rpm, fan_rpms, 8);
        sendPacket(CMD_FAN_STATUS, (uint8_t*)&pl, sizeof(pl));
    }
}
