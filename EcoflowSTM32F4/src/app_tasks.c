#include "app_tasks.h"
#include "FreeRTOS.h"
#include "task.h"
#include "lvgl.h"
#include "lv_port_disp.h"
#include "lv_port_indev.h"
#include "ui/ui_layout.h"
#include "ecoflow_protocol.h"
#include "main.h"

#define GUI_TASK_STACK_SIZE 4096
#define COMM_TASK_STACK_SIZE 1024
#define GUI_TASK_PRIORITY   (tskIDLE_PRIORITY + 2)
#define COMM_TASK_PRIORITY  (tskIDLE_PRIORITY + 3)

extern UART_HandleTypeDef huart1;

static void GuiTask(void *argument);
static void CommTask(void *argument);

void App_Tasks_Init(void)
{
    xTaskCreate(GuiTask, "GUI Task", GUI_TASK_STACK_SIZE, NULL, GUI_TASK_PRIORITY, NULL);
    xTaskCreate(CommTask, "Comm Task", COMM_TASK_STACK_SIZE, NULL, COMM_TASK_PRIORITY, NULL);
    vTaskStartScheduler();
}

static void GuiTask(void *argument)
{
    lv_init();
    lv_port_disp_init();
    lv_port_indev_init();
    
    UI_Init();

    while(1)
    {
        lv_timer_handler();
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

static void CommTask(void *argument)
{
    uint8_t rx_buffer[1]; // Read 1 byte at a time
    uint8_t packet_buffer[MAX_PAYLOAD_LEN + 5]; // START, CMD, LEN, PAYLOAD, CRC
    uint16_t packet_idx = 0;
    
    BatteryStatus batt;
    Temperature temp;
    UartConnectionState conn;

    while(1)
    {
        // Blocking read with timeout
        if (HAL_UART_Receive(&huart1, rx_buffer, 1, 100) == HAL_OK)
        {
            uint8_t byte = rx_buffer[0];
            
            // Simple state machine for packet parsing
            // [START][CMD][LEN][PAYLOAD][CRC8]
            
            if (packet_idx == 0) {
                if (byte == START_BYTE) {
                    packet_buffer[packet_idx++] = byte;
                }
            } else if (packet_idx == 1) { // CMD
                packet_buffer[packet_idx++] = byte;
            } else if (packet_idx == 2) { // LEN
                packet_buffer[packet_idx++] = byte;
            } else {
                // Payload + CRC
                packet_buffer[packet_idx++] = byte;
                uint8_t len = packet_buffer[2];
                if (packet_idx == len + 4) { // Complete packet received
                   // Calculate CRC (excluding CRC byte)
                   uint8_t received_crc = packet_buffer[packet_idx-1];
                   uint8_t calculated_crc = calculate_crc8(packet_buffer, packet_idx-1);
                   
                   if (received_crc == calculated_crc) {
                       uint8_t cmd = packet_buffer[1];
                       uint8_t *payload = &packet_buffer[3];
                       
                       switch(cmd) {
                           case CMD_BATTERY_STATUS:
                               if (unpack_battery_status_message(payload, &batt) == 0) {
                                   UI_UpdateBattery(&batt);
                               }
                               break;
                           case CMD_TEMPERATURE:
                               // Need unpack function or manual unpack
                               // Assuming simple struct copy for now or implement unpack in library
                               if (len == sizeof(Temperature)) {
                                   memcpy(&temp, payload, sizeof(Temperature));
                                   UI_UpdateTemperature(&temp);
                               }
                               break;
                           case CMD_CONNECTION_STATE:
                               if (len == sizeof(UartConnectionState)) {
                                   memcpy(&conn, payload, sizeof(UartConnectionState));
                                   UI_UpdateConnection(&conn);
                               }
                               break;
                       }
                   }
                   packet_idx = 0; // Reset
                }
            }
            
            // Safety reset if buffer overflow
            if (packet_idx >= sizeof(packet_buffer)) {
                packet_idx = 0;
            }
        }
        else {
            // Yield if no data
            vTaskDelay(pdMS_TO_TICKS(10));
        }
    }
}
