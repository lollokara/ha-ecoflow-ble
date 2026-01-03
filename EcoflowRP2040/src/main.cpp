#include <Arduino.h>
#include "hardware/watchdog.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <EEPROM.h>
#include <Adafruit_NeoPixel.h>

// --- Config ---
#define FAN1_PWM_PIN 26
#define FAN2_PWM_PIN 14
#define FAN3_PWM_PIN 6
#define FAN4_PWM_PIN 8

#define FAN1_TACH_PIN 27
#define FAN2_TACH_PIN 15
// Fan 3/4 Tach moved to 18/19 to free up 16 for WS2812 (User Request)
#define FAN3_TACH_PIN 5
#define FAN4_TACH_PIN 7

#define TEMP_PIN 4

// UART for STM32
#define UART_TX_PIN 0
#define UART_RX_PIN 1

#define EEPROM_SIZE 512
#define CONFIG_ADDR 0

// LED
#define LED_PIN 16
#define NUM_LEDS 1

// --- Status Codes ---
#define STATUS_OK           0
#define STATUS_TEMP_ERROR   1
#define STATUS_PWM_ERROR    2
#define STATUS_FAN1_ERROR   3
#define STATUS_FAN2_ERROR   4
#define STATUS_FAN3_ERROR   5
#define STATUS_FAN4_ERROR   6

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
uint8_t current_pwms[4] = {0}; // Track current PWM values
uint32_t last_tach_time = 0;
uint32_t last_ctrl_time = 0;
bool manual_mode = false;
int system_status = STATUS_OK;

OneWire oneWire(TEMP_PIN);
DallasTemperature sensors(&oneWire);
Adafruit_NeoPixel strip(NUM_LEDS, LED_PIN, NEO_GRB + NEO_KHZ800);

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

    // Debug Log Packet Sent (Only if not in manual/CLI mode to reduce noise?)
    // Actually, keep it for monitoring
    // Serial.printf("TX CMD=%02X LEN=%d\n", cmd, len);
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

// --- LED Handling ---
void update_led() {
    static uint32_t last_led_update = 0;
    static int blink_state = 0;
    static int blink_count = 0;
    uint32_t now = millis();
    int interval = 0;

    // Status Code Mapping
    // 0: OK (Green)
    // 1: Temp Error (Red 1 blink)
    // 2: PWM Error (Not implemented trigger yet, assume Red 2 blinks?)
    // 3: Fan 1 Error (Green 2 blinks - User req: "Flash 2 times green because of error on fan 2" -> Fan 1 = 1 blink?)
    // Let's stick to user request style:
    // Temp: Red 1 blink
    // Fan N: Green N blinks (Fan 1=1, Fan 2=2...)

    uint32_t color = strip.Color(0, 0, 0); // Off
    int blinks_needed = 0;
    uint32_t active_color = 0;

    switch(system_status) {
        case STATUS_OK:
            // Solid Green or Heartbeat? User said "0 is all good".
            // Let's do a slow breathe or solid green. Solid Green is simpler.
            strip.setPixelColor(0, strip.Color(0, 10, 0)); // Dim Green
            strip.show();
            return; // No blink logic needed
        case STATUS_TEMP_ERROR:
            blinks_needed = 1;
            active_color = strip.Color(255, 0, 0); // Red
            break;
        case STATUS_PWM_ERROR:
             blinks_needed = 2;
             active_color = strip.Color(255, 0, 0); // Red
             break;
        case STATUS_FAN1_ERROR:
             blinks_needed = 1;
             active_color = strip.Color(0, 255, 0); // Green
             break;
        case STATUS_FAN2_ERROR:
             blinks_needed = 2;
             active_color = strip.Color(0, 255, 0); // Green
             break;
        case STATUS_FAN3_ERROR:
             blinks_needed = 3;
             active_color = strip.Color(0, 255, 0); // Green
             break;
        case STATUS_FAN4_ERROR:
             blinks_needed = 4;
             active_color = strip.Color(0, 255, 0); // Green
             break;
        default:
             blinks_needed = 5;
             active_color = strip.Color(0, 0, 255); // Blue (Unknown)
             break;
    }

    // Blink Logic State Machine
    // States:
    // 0: Idle (Off) - Wait long pause
    // 1: On - Wait short duration
    // 2: Off - Wait short duration (between blinks)

    if (now - last_led_update < 100) return; // minimal update rate check, but we use logic below

    // We need strict timing
    // Cycle: [ON 300ms] -> [OFF 300ms] -> ... -> [Long OFF 1500ms]

    static uint32_t state_start_time = 0;
    if (state_start_time == 0) state_start_time = now;
    uint32_t elapsed = now - state_start_time;

    if (blink_state == 0) {
        // Start Sequence (Wait / Long Pause state)
        if (elapsed > 1500) {
            blink_state = 1; // Start Blinking
            blink_count = 0;
            state_start_time = now;
        } else {
             strip.setPixelColor(0, 0);
             strip.show();
        }
    } else if (blink_state == 1) {
        // ON
        if (elapsed > 300) {
            blink_state = 2; // Turn Off
            state_start_time = now;
        } else {
            strip.setPixelColor(0, active_color);
            strip.show();
        }
    } else if (blink_state == 2) {
        // OFF (Interval)
        if (elapsed > 300) {
            blink_count++;
            if (blink_count >= blinks_needed) {
                blink_state = 0; // Back to Long Pause
            } else {
                blink_state = 1; // Blink again
            }
            state_start_time = now;
        } else {
            strip.setPixelColor(0, 0);
            strip.show();
        }
    }
}

// --- CLI ---
void handle_cli() {
    static String inputBuffer = "";

    while (Serial.available()) {
        char c = Serial.read();
        if (c == '\n' || c == '\r') {
            if (inputBuffer.length() > 0) {
                // Parse Command
                inputBuffer.trim();
                Serial.println("> " + inputBuffer);

                if (inputBuffer == "help") {
                    Serial.println("Commands:");
                    Serial.println("  status      - Show system status code");
                    Serial.println("  fans        - Show fan RPM and PWM");
                    Serial.println("  errors      - Show active errors");
                    Serial.println("  auto        - Enable Auto Mode (Temp controlled)");
                    Serial.println("  setpwm F V  - Set Fan F (1-4) to Value V (0-255) (Enables Manual Mode)");
                } else if (inputBuffer == "status") {
                    Serial.printf("Status Code: %d\n", system_status);
                } else if (inputBuffer == "fans") {
                    Serial.printf("Fan 1: PWM=%d RPM=%d\n", current_pwms[0], fan_rpm[0]);
                    Serial.printf("Fan 2: PWM=%d RPM=%d\n", current_pwms[1], fan_rpm[1]);
                    Serial.printf("Fan 3: PWM=%d RPM=%d\n", current_pwms[2], fan_rpm[2]);
                    Serial.printf("Fan 4: PWM=%d RPM=%d\n", current_pwms[3], fan_rpm[3]);
                } else if (inputBuffer == "errors") {
                    if (system_status == 0) Serial.println("No Errors");
                    else if (system_status == 1) Serial.println("Error: Temp Sensor Missing/Invalid");
                    else if (system_status == 2) Serial.println("Error: PWM Fault");
                    else if (system_status >= 3 && system_status <= 6) Serial.printf("Error: Fan %d Fault\n", system_status - 2);
                } else if (inputBuffer == "auto") {
                    manual_mode = false;
                    Serial.println("Auto Mode Enabled");
                } else if (inputBuffer.startsWith("setpwm ")) {
                    int firstSpace = inputBuffer.indexOf(' ');
                    int secondSpace = inputBuffer.indexOf(' ', firstSpace + 1);
                    if (secondSpace > 0) {
                        int fan = inputBuffer.substring(firstSpace + 1, secondSpace).toInt();
                        int val = inputBuffer.substring(secondSpace + 1).toInt();
                        if (fan >= 1 && fan <= 4 && val >= 0 && val <= 255) {
                            manual_mode = true;
                            // Update PWM immediately
                            if (fan == 1) analogWrite(FAN1_PWM_PIN, val);
                            if (fan == 2) analogWrite(FAN2_PWM_PIN, val);
                            if (fan == 3) analogWrite(FAN3_PWM_PIN, val);
                            if (fan == 4) analogWrite(FAN4_PWM_PIN, val);
                            current_pwms[fan-1] = val; // Manually track since loop won't update
                            Serial.printf("Manual Mode: Fan %d set to %d\n", fan, val);
                        } else {
                            Serial.println("Invalid Fan or Value");
                        }
                    } else {
                        Serial.println("Usage: setpwm <fan_num> <value>");
                    }
                } else {
                    Serial.println("Unknown command. Type 'help'.");
                }

                inputBuffer = "";
            }
        } else {
            inputBuffer += c;
        }
    }
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

    strip.begin();
    strip.setBrightness(50);
    strip.show(); // Initialize all pixels to 'off'

    // Enable Watchdog (8s timeout, pause on debug)
    watchdog_enable(8000, 1);
}

void loop() {
    watchdog_update();
    uint32_t now = millis();

    // Check Inputs
    handle_cli();

    // 1. Read Temp & Control Fans (Every 200ms)
    if (now - last_ctrl_time > 200) {
        last_ctrl_time = now;

        sensors.requestTemperatures();
        currentTemp = sensors.getTempCByIndex(0);
        if (currentTemp == DEVICE_DISCONNECTED_C) currentTemp = 0.0f;

        if (!manual_mode) {
            // Group 1: Fans 1 & 2
            int pwm1 = calc_pwm(&config.group1, currentTemp);
            analogWrite(FAN1_PWM_PIN, pwm1);
            analogWrite(FAN2_PWM_PIN, pwm1);
            current_pwms[0] = pwm1;
            current_pwms[1] = pwm1;

            // Group 2: Fans 3 & 4
            int pwm2 = calc_pwm(&config.group2, currentTemp);
            analogWrite(FAN3_PWM_PIN, pwm2);
            analogWrite(FAN4_PWM_PIN, pwm2);
            current_pwms[2] = pwm2;
            current_pwms[3] = pwm2;
        }
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

        // --- Unified Error Checking ---
        // Priority: Temp > PWM Config > Fans

        int new_status = STATUS_OK;

        // Check Temp
        if (currentTemp == 0.0f) {
            new_status = STATUS_TEMP_ERROR;
        }
        // Check PWM Logic (If temp is high but PWM is low in Auto Mode)
        else if (!manual_mode && currentTemp > config.group1.start_temp && current_pwms[0] == 0) {
            // Simple sanity check: if temp > start_temp, fan should be running.
            // If it's not (and config isn't weird), flag PWM error.
            // This covers "PWM Error" requested by user.
             new_status = STATUS_PWM_ERROR;
        }
        else {
            // Check Fans
            for(int i=0; i<4; i++) {
                // If PWM is high enough to spin (> 60) but RPM is 0
                if (current_pwms[i] > 60 && fan_rpm[i] == 0) {
                    new_status = STATUS_FAN1_ERROR + i; // 3, 4, 5, 6
                    break; // Return first error found
                }
            }
        }

        system_status = new_status;

        // Only log periodic status if NOT typing in CLI?
        // Or just print it. It might interrupt typing.
        // Let's print it only if no CLI input in progress? Hard to know.
        // Just print it cleanly.
        // Serial.printf("Temp: %.2f C | RPM: %d %d %d %d\n", currentTemp, fan_rpm[0], fan_rpm[1], fan_rpm[2], fan_rpm[3]);

        // Send Status to STM32
        uint8_t payload[12];
        memcpy(&payload[0], &currentTemp, 4);
        memcpy(&payload[4], &fan_rpm[0], 2);
        memcpy(&payload[6], &fan_rpm[1], 2);
        memcpy(&payload[8], &fan_rpm[2], 2);
        memcpy(&payload[10], &fan_rpm[3], 2);

        send_packet(FAN_CMD_STATUS, payload, 12);
    }

    // Update LED based on calculated system_status
    update_led();

    // 3. Handle UART Reception (STM32)
    if (Serial1.available()) {
        static uint8_t rxBuf[64];
        static int rxIdx = 0;

        while (Serial1.available()) {
            uint8_t b = Serial1.read();
            if (rxIdx == 0 && b != FAN_UART_START_BYTE) {
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
                        // Serial.printf("RX CMD=%02X LEN=%d\n", cmd, len);

                        if (cmd == FAN_CMD_SET_CONFIG) {
                            if (len == sizeof(FanConfig)) {
                                memcpy(&config, &rxBuf[3], sizeof(FanConfig));
                                save_config();
                            }
                        } else if (cmd == FAN_CMD_GET_CONFIG) {
                            send_packet(FAN_CMD_CONFIG_RESP, (uint8_t*)&config, sizeof(FanConfig));
                        }
                    } else {
                        // Serial.printf("CRC Fail: Calc=%02X Recv=%02X\n", crc, rxBuf[3+len]);
                    }
                    rxIdx = 0;
                }
            }
            if (rxIdx >= 64) rxIdx = 0; // Overflow safety
        }
    }
}
