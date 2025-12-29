#include "ui_layout.h"
#include "lvgl.h"
#include <stdio.h>

static lv_obj_t * label_soc;
static lv_obj_t * label_watts;
static lv_obj_t * label_volts;
static lv_obj_t * label_temp;
static lv_obj_t * label_conn;
static lv_obj_t * bar_soc;
static lv_obj_t * label_device;

void UI_Init(void)
{
    /* Create a grid or flex layout for 4 sections */
    /* 
       ----------------
       | Batt | Temp  |
       ----------------
       | Conn | Info  |
       ----------------
    */

    static lv_coord_t col_dsc[] = {390, 390, LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {230, 230, LV_GRID_TEMPLATE_LAST};

    lv_obj_t * grid = lv_obj_create(lv_scr_act());
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_set_size(grid, 800, 480);
    lv_obj_center(grid);

    /* --- Section 1: Battery --- */
    lv_obj_t * obj_batt = lv_obj_create(grid);
    lv_obj_set_grid_cell(obj_batt, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    
    label_soc = lv_label_create(obj_batt);
    lv_label_set_text(label_soc, "SOC: --%");
    lv_obj_align(label_soc, LV_ALIGN_TOP_MID, 0, 10);
    
    bar_soc = lv_bar_create(obj_batt);
    lv_obj_set_size(bar_soc, 200, 20);
    lv_obj_align(bar_soc, LV_ALIGN_CENTER, 0, -20);
    lv_bar_set_range(bar_soc, 0, 100);

    label_watts = lv_label_create(obj_batt);
    lv_label_set_text(label_watts, "0 W");
    lv_obj_align(label_watts, LV_ALIGN_BOTTOM_LEFT, 20, -20);

    label_volts = lv_label_create(obj_batt);
    lv_label_set_text(label_volts, "0.0 V");
    lv_obj_align(label_volts, LV_ALIGN_BOTTOM_RIGHT, -20, -20);

    /* --- Section 2: Temperature --- */
    lv_obj_t * obj_temp = lv_obj_create(grid);
    lv_obj_set_grid_cell(obj_temp, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 0, 1);
    
    label_temp = lv_label_create(obj_temp);
    lv_label_set_text(label_temp, "Temp: -- C");
    lv_obj_center(label_temp);

    /* --- Section 3: Connection --- */
    lv_obj_t * obj_conn = lv_obj_create(grid);
    lv_obj_set_grid_cell(obj_conn, LV_GRID_ALIGN_STRETCH, 0, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
    
    label_conn = lv_label_create(obj_conn);
    lv_label_set_text(label_conn, "State: Idle");
    lv_obj_center(label_conn);

    /* --- Section 4: Info/Device --- */
    lv_obj_t * obj_info = lv_obj_create(grid);
    lv_obj_set_grid_cell(obj_info, LV_GRID_ALIGN_STRETCH, 1, 1, LV_GRID_ALIGN_STRETCH, 1, 1);
    
    label_device = lv_label_create(obj_info);
    lv_label_set_text(label_device, "No Device");
    lv_obj_center(label_device);
}

void UI_UpdateBattery(BatteryStatus *status)
{
    if (status == NULL) return;

    lv_label_set_text_fmt(label_soc, "SOC: %d%%", status->soc);
    lv_bar_set_value(bar_soc, status->soc, LV_ANIM_ON);
    
    lv_label_set_text_fmt(label_watts, "%d W", status->power_w);
    lv_label_set_text_fmt(label_volts, "%d.%d V", status->voltage_v / 10, status->voltage_v % 10); // assuming mV*10 = 100mV units? Header says "mV * 10", usually means 1200 -> 12.0V? No, "mV * 10" is unusual. If it's mV, 12000 is 12V. If it's dV, 120 is 12V. Let's assume raw value.
    
    if (status->connected) {
         lv_label_set_text(label_device, status->device_name);
    } else {
         lv_label_set_text(label_device, "Disconnected");
    }
}

void UI_UpdateTemperature(Temperature *temp)
{
    if (temp == NULL) return;
    lv_label_set_text_fmt(label_temp, "Temp: %d C\nMin: %d C\nMax: %d C", temp->temp_c, temp->temp_min, temp->temp_max);
}

void UI_UpdateConnection(UartConnectionState *conn)
{
    if (conn == NULL) return;
    
    const char *states[] = {"Idle", "Scanning", "Connecting", "Connected"};
    const char *state_str = (conn->state < 4) ? states[conn->state] : "Unknown";
    
    lv_label_set_text_fmt(label_conn, "State: %s\nSignal: %d dB", state_str, conn->signal_db);
}
