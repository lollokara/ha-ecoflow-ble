#include "fan_task.h"
#include "stm32f4xx_hal.h"
#include <string.h>
#include <stdio.h>
#include "semphr.h"

// --- Hardware Handles ---
UART_HandleTypeDef huart4; // RX (PA1), TX (PA0)
static SemaphoreHandle_t dataMutex;

// --- State ---
static FanStatus currentStatus = {0};
static FanConfig currentConfig = {
    .group1 = { .min_speed = 500, .max_speed = 3000, .start_temp = 30, .max_temp = 45 },
    .group2 = { .min_speed = 500, .max_speed = 3000, .start_temp = 30, .max_temp = 45 }
};
static uint32_t lastPacketTime = 0;

// --- Ring Buffer ---
#define RING_BUFFER_SIZE 1024
typedef struct {
    uint8_t buffer[RING_BUFFER_SIZE];
    volatile uint16_t head;
    volatile uint16_t tail;
} FanRingBuffer;

static FanRingBuffer rxRing;
static uint8_t rxByte;

static void rb_push(FanRingBuffer *rb, uint8_t byte) {
    uint16_t next_head = (rb->head + 1) % RING_BUFFER_SIZE;
    if (next_head != rb->tail) {
        rb->buffer[rb->head] = byte;
        rb->head = next_head;
    }
}

static int rb_pop(FanRingBuffer *rb, uint8_t *byte) {
    if (rb->head == rb->tail) return 0;
    *byte = rb->buffer[rb->tail];
    rb->tail = (rb->tail + 1) % RING_BUFFER_SIZE;
    return 1;
}

// --- CRC8 Helper ---
static uint8_t calc_crc8(uint8_t *data, size_t len) {
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

// --- UART Initialization ---
void MX_Fan_UART_Init(void) {
    // UART4 (TX+RX)
    huart4.Instance = UART4;
    huart4.Init.BaudRate = 115200;
    huart4.Init.WordLength = UART_WORDLENGTH_8B;
    huart4.Init.StopBits = UART_STOPBITS_1;
    huart4.Init.Parity = UART_PARITY_NONE;
    huart4.Init.Mode = UART_MODE_TX_RX;
    huart4.Init.HwFlowCtl = UART_HWCONTROL_NONE;
    huart4.Init.OverSampling = UART_OVERSAMPLING_16;
    HAL_UART_Init(&huart4);
}

// --- Sender ---
static void send_packet(uint8_t cmd, uint8_t *payload, uint8_t len) {
    uint8_t packet[32]; // Max size
    if (len + 5 > sizeof(packet)) return;

    packet[0] = FAN_UART_START_BYTE;
    packet[1] = cmd;
    packet[2] = len;
    if (len > 0 && payload) memcpy(&packet[3], payload, len);
    packet[3 + len] = calc_crc8(packet, 3 + len);

    // Transmit via UART4
    HAL_UART_Transmit(&huart4, packet, 3 + len + 1, 100);
}

// --- Parser State ---
#define RX_PARSE_BUF_SIZE 64
static uint8_t parseBuffer[RX_PARSE_BUF_SIZE];
static int parseIndex = 0;

static void process_byte(uint8_t b) {
    // Simple debug log for raw bytes (limit frequency or only unexpected?)
    // printf("FAN RX: %02X\n", b);

    if (parseIndex == 0) {
        if (b == FAN_UART_START_BYTE) {
            parseBuffer[0] = b;
            parseIndex = 1;
        }
        return;
    }

    parseBuffer[parseIndex++] = b;

    // Check Header
    if (parseIndex >= 3) {
        uint8_t len = parseBuffer[2];
        if (parseIndex == 3 + len + 1) {
            // Full Packet Received
            if (calc_crc8(parseBuffer, 3 + len) == parseBuffer[3 + len]) {
                // Valid Packet
                uint8_t cmd = parseBuffer[1];
                uint8_t *payload = &parseBuffer[3];

                printf("FAN: Valid Packet CMD=%02X LEN=%d\n", cmd, len);

                xSemaphoreTake(dataMutex, portMAX_DELAY);
                lastPacketTime = xTaskGetTickCount();
                currentStatus.connected = true;

                if (cmd == FAN_CMD_STATUS && len == 12) {
                    memcpy(&currentStatus.amb_temp, payload, 4);
                    memcpy(currentStatus.fan_rpm, payload + 4, 8);
                    printf("FAN: Status Updated. Temp=%.2f\n", currentStatus.amb_temp);
                } else if (cmd == FAN_CMD_CONFIG_RESP && len == sizeof(FanConfig)) {
                    memcpy(&currentConfig, payload, sizeof(FanConfig));
                    printf("FAN: Config Received\n");
                }
                xSemaphoreGive(dataMutex);
            } else {
                printf("FAN: CRC Error. Calc=%02X Recv=%02X\n", calc_crc8(parseBuffer, 3 + len), parseBuffer[3 + len]);
            }
            // Reset
            parseIndex = 0;
        }
    }

    if (parseIndex >= RX_PARSE_BUF_SIZE) {
        printf("FAN: Buffer Overflow, Resetting\n");
        parseIndex = 0; // Overflow safety
    }
}

// --- Interface ---
void Fan_GetStatus(FanStatus *status) {
    if (!status) return;
    if (xSemaphoreTake(dataMutex, 10) == pdTRUE) {
        memcpy(status, &currentStatus, sizeof(FanStatus));
        if (xTaskGetTickCount() - lastPacketTime > 3000) {
            status->connected = false;
        }
        xSemaphoreGive(dataMutex);
    }
}

void Fan_SetConfig(const FanConfig *config) {
    if (!config) return;
    if (xSemaphoreTake(dataMutex, 10) == pdTRUE) {
        currentConfig = *config;
        xSemaphoreGive(dataMutex);
        send_packet(FAN_CMD_SET_CONFIG, (uint8_t*)config, sizeof(FanConfig));
    }
}

void Fan_GetConfig(FanConfig *config) {
    if (!config) return;
    if (xSemaphoreTake(dataMutex, 10) == pdTRUE) {
        *config = currentConfig;
        xSemaphoreGive(dataMutex);
    }
}

void Fan_RequestConfig(void) {
    send_packet(FAN_CMD_GET_CONFIG, NULL, 0);
}

// --- Task ---
void StartFanTask(void *argument) {
    dataMutex = xSemaphoreCreateMutex();

    // Initialize split UARTs
    MX_Fan_UART_Init();

    // Start Reception on UART4 (RX Pin)
    HAL_UART_Receive_IT(&huart4, &rxByte, 1);

    // Initial config request delay
    vTaskDelay(pdMS_TO_TICKS(1000));
    Fan_RequestConfig();

    uint8_t b;
    for(;;) {
        // Process Ring Buffer
        while (rb_pop(&rxRing, &b)) {
            process_byte(b);
        }

        // Check timeout
        if (xSemaphoreTake(dataMutex, 10) == pdTRUE) {
            if (xTaskGetTickCount() - lastPacketTime > 3000) {
                currentStatus.connected = false;
            }
            xSemaphoreGive(dataMutex);
        }

        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

// --- IRQ Handlers ---
// UART4_IRQHandler moved to stm32f4xx_it.c to ensure visibility

void Fan_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
    if (huart->Instance == UART4) {
        rb_push(&rxRing, rxByte);
        HAL_UART_Receive_IT(&huart4, &rxByte, 1);
    }
}
