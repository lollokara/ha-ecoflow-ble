#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <LittleFS.h>

// Pins
#define PIN_UART_TX 0
#define PIN_UART_RX 1
#define PIN_ONEWIRE 2
#define PIN_FAN1_PWM 3
#define PIN_FAN2_PWM 4
#define PIN_FAN3_PWM 5
#define PIN_FAN4_PWM 6
#define PIN_FAN1_TACH 7
#define PIN_FAN2_TACH 8
#define PIN_FAN3_TACH 9
#define PIN_FAN4_TACH 10

// Constants
#define NUM_FANS 4
#define NUM_GROUPS 2
#define PWM_FREQ 25000
#define PWM_RES 8
#define FAN_MAX_RPM 6000

// Fan Group Config
struct FanConfig {
  uint16_t minSpeed; // RPM
  uint16_t maxSpeed; // RPM
  uint8_t startTemp; // Celsius
  uint8_t maxTemp;   // Celsius
};

FanConfig groups[NUM_GROUPS]; // 0: Fans 1&2, 1: Fans 3&4

// Fan State
struct FanState {
  uint16_t current_rpm;
  uint16_t target_rpm;
  uint8_t pwm;
};
FanState fans[NUM_FANS];

// Temperature
OneWire oneWire(PIN_ONEWIRE);
DallasTemperature sensors(&oneWire);
float currentTemp = 0.0;

// Tachometer Calculation
volatile uint32_t tachCounts[NUM_FANS] = {0};
unsigned long lastTachTime = 0;

void onTach1() { tachCounts[0]++; }
void onTach2() { tachCounts[1]++; }
void onTach3() { tachCounts[2]++; }
void onTach4() { tachCounts[3]++; }

// UART Protocol
#define PKT_START 0xAA
#define CMD_SET_CONFIG 0x10
#define CMD_GET_STATUS 0x11
#define CMD_GET_CONFIG 0x12
#define RESP_STATUS    0x20
#define RESP_CONFIG    0x21

void saveConfig() {
    File f = LittleFS.open("/config.bin", "w");
    if (f) {
        f.write((uint8_t*)groups, sizeof(groups));
        f.close();
    }
}

void loadConfig() {
  if (!LittleFS.begin()) {
      LittleFS.format();
      LittleFS.begin();
  }

  File f = LittleFS.open("/config.bin", "r");
  if (f) {
      f.read((uint8_t*)groups, sizeof(groups));
      f.close();
  } else {
      // Defaults
      for (int i=0; i<NUM_GROUPS; i++) {
          groups[i].minSpeed = 500;
          groups[i].maxSpeed = 3000;
          groups[i].startTemp = 30;
          groups[i].maxTemp = 40;
      }
      saveConfig(); // Save defaults immediately
  }
}

void setup() {
    Serial.begin(115200); // Debug USB
    Serial1.setTX(PIN_UART_TX);
    Serial1.setRX(PIN_UART_RX);
    Serial1.begin(115200);

    // Allow USB to connect
    delay(2000);
    Serial.println("RP2040 Fan Controller Booting...");

    sensors.begin();
    loadConfig();

    // PWM Setup
    analogWriteFreq(PWM_FREQ);
    analogWriteRange(255);
    pinMode(PIN_FAN1_PWM, OUTPUT);
    pinMode(PIN_FAN2_PWM, OUTPUT);
    pinMode(PIN_FAN3_PWM, OUTPUT);
    pinMode(PIN_FAN4_PWM, OUTPUT);

    // Tach Setup
    pinMode(PIN_FAN1_TACH, INPUT_PULLUP);
    pinMode(PIN_FAN2_TACH, INPUT_PULLUP);
    pinMode(PIN_FAN3_TACH, INPUT_PULLUP);
    pinMode(PIN_FAN4_TACH, INPUT_PULLUP);

    attachInterrupt(digitalPinToInterrupt(PIN_FAN1_TACH), onTach1, RISING);
    attachInterrupt(digitalPinToInterrupt(PIN_FAN2_TACH), onTach2, RISING);
    attachInterrupt(digitalPinToInterrupt(PIN_FAN3_TACH), onTach3, RISING);
    attachInterrupt(digitalPinToInterrupt(PIN_FAN4_TACH), onTach4, RISING);
}

void updateFanControl() {
    // Determine PWM for each group based on Temp
    for (int g=0; g<NUM_GROUPS; g++) {
        uint16_t target = 0;

        if (currentTemp >= groups[g].maxTemp) {
            target = groups[g].maxSpeed;
        } else if (currentTemp <= groups[g].startTemp) {
            target = 0; // Off below start temp
        } else {
            // Linear map Temp -> RPM
            float tempRange = groups[g].maxTemp - groups[g].startTemp;
            float factor = (currentTemp - groups[g].startTemp) / tempRange;
            float speedRange = groups[g].maxSpeed - groups[g].minSpeed;
            target = groups[g].minSpeed + (uint16_t)(speedRange * factor);
        }

        // Apply to fans in group
        int startFan = g * 2;
        int endFan = startFan + 2;
        for (int i=startFan; i<endFan; i++) {
             fans[i].target_rpm = target;

             if (target == 0) {
                 fans[i].pwm = 0;
             } else {
                 long pwm_val = map(target, 0, FAN_MAX_RPM, 0, 255);
                 if (pwm_val > 255) pwm_val = 255;
                 if (pwm_val < 30 && target > 0) pwm_val = 30; // Min start PWM
                 fans[i].pwm = (uint8_t)pwm_val;
             }

             // Write PWM
             switch(i) {
                 case 0: analogWrite(PIN_FAN1_PWM, fans[i].pwm); break;
                 case 1: analogWrite(PIN_FAN2_PWM, fans[i].pwm); break;
                 case 2: analogWrite(PIN_FAN3_PWM, fans[i].pwm); break;
                 case 3: analogWrite(PIN_FAN4_PWM, fans[i].pwm); break;
             }
        }
    }
}

void calculateRPM() {
    unsigned long now = millis();
    unsigned long dt = now - lastTachTime;
    if (dt >= 1000) {
        for(int i=0; i<NUM_FANS; i++) {
             // 2 pulses per rev usually
             noInterrupts();
             uint32_t count = tachCounts[i];
             tachCounts[i] = 0;
             interrupts();

             // RPM = (count / 2) * (60000 / dt)
             fans[i].current_rpm = (count * 30000) / dt;
        }
        lastTachTime = now;
    }
}

void sendStatus() {
    // Packet: [START 0xAA] [CMD 0x20] [LEN] [T_int] [T_dec] [F1H F1L] [F2H F2L] [F3H F3L] [F4H F4L] [CRC]
    uint8_t buf[32];
    buf[0] = PKT_START;
    buf[1] = RESP_STATUS;
    buf[2] = 10; // Payload len

    // Payload
    int t_int = (int)currentTemp;
    int t_dec = (int)((currentTemp - t_int) * 100);
    buf[3] = (uint8_t)t_int;
    buf[4] = (uint8_t)t_dec;

    for(int i=0; i<4; i++) {
        buf[5 + i*2] = (fans[i].current_rpm >> 8) & 0xFF;
        buf[6 + i*2] = fans[i].current_rpm & 0xFF;
    }

    // CRC
    uint8_t crc = 0;
    for(int i=0; i<10; i++) crc += buf[3+i];
    buf[13] = crc;

    Serial1.write(buf, 14);

    Serial.print("TX STATUS: T="); Serial.print(currentTemp);
    Serial.print(" F1="); Serial.print(fans[0].current_rpm);
    Serial.println();
}

void sendConfig(uint8_t group) {
    if (group >= NUM_GROUPS) return;

    // Packet: [START 0xAA] [CMD 0x21] [LEN] [Group] [MinH] [MinL] [MaxH] [MaxL] [Start] [Max] [CRC]
    uint8_t buf[16];
    buf[0] = PKT_START;
    buf[1] = RESP_CONFIG;
    buf[2] = 7; // Payload len

    buf[3] = group;
    buf[4] = (groups[group].minSpeed >> 8) & 0xFF;
    buf[5] = groups[group].minSpeed & 0xFF;
    buf[6] = (groups[group].maxSpeed >> 8) & 0xFF;
    buf[7] = groups[group].maxSpeed & 0xFF;
    buf[8] = groups[group].startTemp;
    buf[9] = groups[group].maxTemp;

    uint8_t crc = 0;
    for(int i=0; i<7; i++) crc += buf[3+i];
    buf[10] = crc;

    Serial1.write(buf, 11);
    Serial.print("TX CONFIG G="); Serial.println(group);
}

void processSerial() {
    while (Serial1.available() > 0) {
        // Peek to sync
        if (Serial1.peek() != PKT_START) {
            uint8_t b = Serial1.read();
            Serial.print("RX_SKIP: "); Serial.println(b, HEX);
            continue;
        }

        // Wait for header (3 bytes)
        if (Serial1.available() < 3) return;

        // Peek header without consuming to ensure full packet availability check later?
        // No, we need to read header to know len.
        // We can't peek 3 bytes easily with standard API.
        // Let's read header.

        uint8_t header[3];
        header[0] = Serial1.read(); // 0xAA
        header[1] = Serial1.read(); // CMD
        header[2] = Serial1.read(); // LEN

        Serial.print("RX_HDR: CMD="); Serial.print(header[1], HEX);
        Serial.print(" LEN="); Serial.println(header[2]);

        uint8_t len = header[2];
        if (len > 20) { // Garbage
            Serial.println("ERR: Len too big");
            continue;
        }

        // Wait for payload + CRC
        unsigned long start = millis();
        while(Serial1.available() < len + 1) {
            if (millis() - start > 10) {
                Serial.println("ERR: Timeout payload");
                return; // Timeout
            }
        }

        uint8_t payload[32]; // Increased buffer size for safety
        Serial1.readBytes(payload, len + 1); // Payload + CRC

        uint8_t rx_crc = payload[len];
        uint8_t calc_crc = 0;
        for(int i=0; i<len; i++) calc_crc += payload[i];

        if (rx_crc != calc_crc) {
            Serial.println("ERR: Bad CRC");
            continue;
        }

        Serial.print("RX CMD: "); Serial.println(header[1], HEX);

        if (header[1] == CMD_SET_CONFIG) {
            if (len >= 7) {
                uint8_t g = payload[0];
                if (g < NUM_GROUPS) {
                    groups[g].minSpeed = (payload[1] << 8) | payload[2];
                    groups[g].maxSpeed = (payload[3] << 8) | payload[4];
                    groups[g].startTemp = payload[5];
                    groups[g].maxTemp = payload[6];
                    saveConfig();
                    Serial.println("Config Saved");
                }
            }
        } else if (header[1] == CMD_GET_CONFIG) {
            if (len >= 1) {
                sendConfig(payload[0]);
            }
        }
    }
}

unsigned long lastTempTime = 0;
unsigned long lastSendTime = 0;

void loop() {
    unsigned long now = millis();

    if (now - lastTempTime > 1000) {
        sensors.requestTemperatures();
        float t = sensors.getTempCByIndex(0);
        if (t > -127) currentTemp = t;
        lastTempTime = now;

        calculateRPM();
        updateFanControl();

        Serial.print("Temp: "); Serial.print(currentTemp);
        Serial.print(" RPM: ");
        for(int i=0; i<4; i++) {
            Serial.print(fans[i].current_rpm); Serial.print("/");
            Serial.print(fans[i].target_rpm); Serial.print(" ");
        }
        Serial.println();
    }

    if (now - lastSendTime > 500) {
        sendStatus();
        lastSendTime = now;
    }

    processSerial();
}
