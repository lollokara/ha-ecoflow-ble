#include "ui_app.h"
#include "lvgl.h"
#include <stdio.h>
#include "img_mdi.h" // Import images

// UI Objects
static lv_obj_t * bar_battery;
static lv_obj_t * label_soc;
static lv_obj_t * label_in_power;
static lv_obj_t * label_out_power;
static lv_obj_t * label_time;

// Styles
static lv_style_t style_bar_indic;

void UI_Init(void) {
    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_color(scr, lv_color_hex(0xFFFFFF), 0);

    // 1. Header
    lv_obj_t * header = lv_obj_create(scr);
    lv_obj_set_size(header, 800, 140);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
    lv_obj_set_style_radius(header, 0, 0);

    lv_obj_t * title = lv_label_create(header);
    lv_label_set_text(title, "Battery Status");
    lv_obj_align(title, LV_ALIGN_TOP_LEFT, 10, 10);

    // Battery Bar
    bar_battery = lv_bar_create(header);
    lv_obj_set_size(bar_battery, 700, 60);
    lv_obj_align(bar_battery, LV_ALIGN_CENTER, 0, -10);
    lv_bar_set_range(bar_battery, 0, 100);
    lv_bar_set_value(bar_battery, 50, LV_ANIM_OFF);

    lv_style_init(&style_bar_indic);
    lv_style_set_bg_color(&style_bar_indic, lv_color_hex(0x4CAF50));
    lv_style_set_bg_grad_color(&style_bar_indic, lv_color_hex(0xFF9800));
    lv_style_set_bg_grad_dir(&style_bar_indic, LV_GRAD_DIR_HOR);
    lv_obj_add_style(bar_battery, &style_bar_indic, LV_PART_INDICATOR);

    label_soc = lv_label_create(header);
    lv_label_set_text(label_soc, "50%");
    lv_obj_set_style_text_font(label_soc, &lv_font_montserrat_24, 0);
    lv_obj_align_to(label_soc, bar_battery, LV_ALIGN_CENTER, 0, 0);

    // Stats
    label_in_power = lv_label_create(header);
    lv_label_set_text(label_in_power, "In: 0W");
    lv_obj_align(label_in_power, LV_ALIGN_BOTTOM_LEFT, 50, -10);

    label_out_power = lv_label_create(header);
    lv_label_set_text(label_out_power, "Out: 0W");
    lv_obj_align(label_out_power, LV_ALIGN_BOTTOM_MID, 0, -10);

    label_time = lv_label_create(header);
    lv_label_set_text(label_time, "-- remaining");
    lv_obj_align(label_time, LV_ALIGN_BOTTOM_RIGHT, -50, -10);

    // 2. Energy Flow (Middle)
    lv_obj_t * flow = lv_obj_create(scr);
    lv_obj_set_size(flow, 800, 210);
    lv_obj_set_pos(flow, 0, 140);
    lv_obj_set_style_border_width(flow, 0, 0);

    // Solar Icon (Left Top)
    lv_obj_t * icon_solar = lv_image_create(flow);
    lv_image_set_src(icon_solar, &img_mdi_solar);
    lv_obj_set_style_image_recolor(icon_solar, lv_color_hex(0x333333), 0);
    lv_obj_set_style_image_recolor_opa(icon_solar, LV_OPA_COVER, 0);
    lv_obj_align(icon_solar, LV_ALIGN_TOP_LEFT, 40, 20);

    // AC Plug (Left Bottom)
    lv_obj_t * icon_plug = lv_image_create(flow);
    lv_image_set_src(icon_plug, &img_mdi_plug);
    lv_obj_set_style_image_recolor(icon_plug, lv_color_hex(0x4CAF50), 0);
    lv_obj_set_style_image_recolor_opa(icon_plug, LV_OPA_COVER, 0);
    lv_obj_align(icon_plug, LV_ALIGN_BOTTOM_LEFT, 40, -20);

    // Battery Icon (Center)
    lv_obj_t * icon_bat = lv_image_create(flow);
    lv_image_set_src(icon_bat, &img_mdi_battery);
    lv_image_set_scale(icon_bat, 512); // 2x scale (48->96)? No, 256 is 1x. 512 is 2x.
    // MDI icons are 48x48. We want bigger center battery?
    // Let's keep 1x for sharpness or scale carefully.
    lv_obj_set_style_image_recolor(icon_bat, lv_color_hex(0x00BCD4), 0);
    lv_obj_set_style_image_recolor_opa(icon_bat, LV_OPA_COVER, 0);
    lv_obj_align(icon_bat, LV_ALIGN_CENTER, 0, 0);

    // USB (Right Top)
    lv_obj_t * icon_usb = lv_image_create(flow);
    lv_image_set_src(icon_usb, &img_mdi_usb);
    lv_obj_set_style_image_recolor(icon_usb, lv_color_hex(0x333333), 0);
    lv_obj_set_style_image_recolor_opa(icon_usb, LV_OPA_COVER, 0);
    lv_obj_align(icon_usb, LV_ALIGN_TOP_RIGHT, -40, 20);

    // AC Out (Right Bottom)
    lv_obj_t * icon_ac = lv_image_create(flow);
    lv_image_set_src(icon_ac, &img_mdi_socket);
    lv_obj_set_style_image_recolor(icon_ac, lv_color_hex(0x333333), 0);
    lv_obj_set_style_image_recolor_opa(icon_ac, LV_OPA_COVER, 0);
    lv_obj_align(icon_ac, LV_ALIGN_BOTTOM_RIGHT, -40, -20);

    // 3. Footer
    lv_obj_t * footer = lv_obj_create(scr);
    lv_obj_set_size(footer, 800, 130);
    lv_obj_set_pos(footer, 0, 350);

    lv_obj_t * btn_settings = lv_button_create(footer);
    lv_obj_align(btn_settings, LV_ALIGN_RIGHT_MID, -20, 0);
    lv_obj_set_style_bg_color(btn_settings, lv_color_hex(0x00BCD4), 0);

    // Settings Icon inside button
    lv_obj_t * icon_set = lv_image_create(btn_settings);
    lv_image_set_src(icon_set, &img_mdi_settings); // Assuming we generated settings
    lv_obj_set_style_image_recolor(icon_set, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_image_recolor_opa(icon_set, LV_OPA_COVER, 0);

    lv_obj_t * lbl_set = lv_label_create(btn_settings);
    lv_label_set_text(lbl_set, "Settings");
}

void UI_Update(DeviceStatus *status) {
    if (status->id == DEV_TYPE_DELTA_PRO_3) {
        int soc = (int)status->data.d3p.batteryLevel;
        lv_bar_set_value(bar_battery, soc, LV_ANIM_ON);
        lv_label_set_text_fmt(label_soc, "%d%%", soc);

        if (soc < 20) lv_style_set_bg_color(&style_bar_indic, lv_color_hex(0xF44336));
        else lv_style_set_bg_color(&style_bar_indic, lv_color_hex(0x4CAF50));
        lv_obj_report_style_change(&style_bar_indic);

        lv_label_set_text_fmt(label_in_power, "In: %dW", (int)status->data.d3p.acInputPower);
        int total_out = (int)(status->data.d3p.acLvOutputPower + status->data.d3p.acHvOutputPower);
        lv_label_set_text_fmt(label_out_power, "Out: %dW", total_out);
    }
}
