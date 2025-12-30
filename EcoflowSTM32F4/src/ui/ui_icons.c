#include "ui_icons.h"
#include <math.h>

#define MAX(a,b) ((a)>(b)?(a):(b))

// Helpers for drawing
static void draw_rect(lv_draw_ctx_t * draw_ctx, const lv_draw_rect_dsc_t * dsc, const lv_area_t * coords) {
    lv_draw_rect(draw_ctx, dsc, coords);
}

static void draw_line(lv_draw_ctx_t * draw_ctx, const lv_draw_line_dsc_t * dsc, const lv_point_t * point1, const lv_point_t * point2) {
    lv_draw_line(draw_ctx, dsc, point1, point2);
}

void ui_draw_icon(lv_obj_t * obj, lv_draw_ctx_t * draw_ctx, ui_icon_type_t type, const lv_area_t * area, lv_color_t color) {
    lv_coord_t w = lv_area_get_width(area);
    lv_coord_t h = lv_area_get_height(area);
    lv_coord_t x = area->x1;
    lv_coord_t y = area->y1;

    lv_draw_rect_dsc_t rect_dsc;
    lv_draw_rect_dsc_init(&rect_dsc);
    rect_dsc.bg_color = color;
    rect_dsc.bg_opa = LV_OPA_COVER;
    rect_dsc.border_width = 0;

    lv_draw_line_dsc_t line_dsc;
    lv_draw_line_dsc_init(&line_dsc);
    line_dsc.color = color;
    line_dsc.width = MAX(2, w / 12);
    line_dsc.round_start = 1;
    line_dsc.round_end = 1;

    if (type == UI_ICON_SOLAR) {
        // Sun Panel
        lv_area_t panel;
        panel.x1 = x + w * 0.1;
        panel.x2 = x + w * 0.9;
        panel.y1 = y + h * 0.4;
        panel.y2 = y + h * 0.9;

        // Draw grid lines instead of full rect
        line_dsc.width = 2;
        // Outline
        lv_point_t p[5] = {
            {panel.x1, panel.y1}, {panel.x2, panel.y1},
            {panel.x2, panel.y2}, {panel.x1, panel.y2}, {panel.x1, panel.y1}
        };
        for(int i=0; i<4; i++) draw_line(draw_ctx, &line_dsc, &p[i], &p[i+1]);

        // Grid
        lv_point_t v1 = {x + w/2, panel.y1}, v2 = {x + w/2, panel.y2};
        draw_line(draw_ctx, &line_dsc, &v1, &v2);
        lv_point_t h1 = {panel.x1, y + h*0.65}, h2 = {panel.x2, y + h*0.65};
        draw_line(draw_ctx, &line_dsc, &h1, &h2);

        // Rays
        lv_point_t r1a = {x + w/2, y + h*0.1}, r1b = {x + w/2, y + h*0.3};
        draw_line(draw_ctx, &line_dsc, &r1a, &r1b);
        lv_point_t r2a = {x + w*0.2, y + h*0.15}, r2b = {x + w*0.35, y + h*0.35};
        draw_line(draw_ctx, &line_dsc, &r2a, &r2b);
        lv_point_t r3a = {x + w*0.8, y + h*0.15}, r3b = {x + w*0.65, y + h*0.35};
        draw_line(draw_ctx, &line_dsc, &r3a, &r3b);
    }
    else if (type == UI_ICON_GRID) {
        // Plug
        lv_area_t plug;
        plug.x1 = x + w*0.3;
        plug.x2 = x + w*0.7;
        plug.y1 = y + h*0.4;
        plug.y2 = y + h*0.8;
        rect_dsc.radius = 4;
        draw_rect(draw_ctx, &rect_dsc, &plug);

        // Prongs
        lv_area_t p1 = {x + w*0.35, y + h*0.2, x + w*0.45, y + h*0.4};
        draw_rect(draw_ctx, &rect_dsc, &p1);
        lv_area_t p2 = {x + w*0.55, y + h*0.2, x + w*0.65, y + h*0.4};
        draw_rect(draw_ctx, &rect_dsc, &p2);

        // Cord
        lv_point_t c1 = {x + w/2, y + h*0.8}, c2 = {x + w/2, y + h};
        draw_line(draw_ctx, &line_dsc, &c1, &c2);
    }
    else if (type == UI_ICON_CAR) {
        // Car Battery
        lv_area_t bat;
        bat.x1 = x + w*0.1;
        bat.x2 = x + w*0.9;
        bat.y1 = y + h*0.3;
        bat.y2 = y + h*0.8;
        rect_dsc.bg_opa = LV_OPA_TRANSP;
        rect_dsc.border_width = 3;
        rect_dsc.border_color = color;
        draw_rect(draw_ctx, &rect_dsc, &bat);

        // Terminals
        rect_dsc.bg_opa = LV_OPA_COVER;
        rect_dsc.border_width = 0;
        lv_area_t t1 = {x + w*0.2, y + h*0.2, x + w*0.3, y + h*0.3};
        draw_rect(draw_ctx, &rect_dsc, &t1);
        lv_area_t t2 = {x + w*0.7, y + h*0.2, x + w*0.8, y + h*0.3};
        draw_rect(draw_ctx, &rect_dsc, &t2);
    }
    else if (type == UI_ICON_USB) {
        // USB Symbol
        lv_point_t p1 = {x + w/2, y + h*0.8}, p2 = {x + w/2, y + h*0.2};
        draw_line(draw_ctx, &line_dsc, &p1, &p2);

        // Trident
        lv_point_t t1 = {x + w/2, y + h*0.5}, t2 = {x + w*0.2, y + h*0.3};
        draw_line(draw_ctx, &line_dsc, &t1, &t2);
        lv_point_t t3 = {x + w/2, y + h*0.5}, t4 = {x + w*0.8, y + h*0.3};
        draw_line(draw_ctx, &line_dsc, &t3, &t4);

        // Heads
        lv_area_t sq = {x + w*0.15, y + h*0.25, x + w*0.25, y + h*0.35};
        draw_rect(draw_ctx, &rect_dsc, &sq); // Square

        // Circle
        // Using arc dsc for circle? No, draw_arc.
        lv_draw_arc_dsc_t arc_dsc;
        lv_draw_arc_dsc_init(&arc_dsc);
        arc_dsc.color = color;
        arc_dsc.width = 3;
        lv_point_t center = {x + w*0.8, y + h*0.3};
        lv_draw_arc(draw_ctx, &arc_dsc, &center, 4, 0, 360);
    }
    else if (type == UI_ICON_AC_OUT) {
        // Socket
        lv_draw_arc_dsc_t arc_dsc;
        lv_draw_arc_dsc_init(&arc_dsc);
        arc_dsc.color = color;
        arc_dsc.width = 2;
        lv_point_t center = {x + w/2, y + h/2};
        lv_draw_arc(draw_ctx, &arc_dsc, &center, w/2 - 2, 0, 360);

        // Holes
        lv_area_t h1 = {x + w*0.3, y + h*0.4, x + w*0.4, y + h*0.5};
        lv_area_t h2 = {x + w*0.6, y + h*0.4, x + w*0.7, y + h*0.5};
        draw_rect(draw_ctx, &rect_dsc, &h1);
        draw_rect(draw_ctx, &rect_dsc, &h2);
    }
}

// Wrapper for LVGL event
static void icon_event_cb(lv_event_t * e) {
    ui_icon_type_t type = (ui_icon_type_t)(intptr_t)lv_event_get_user_data(e);
    lv_obj_t * obj = lv_event_get_target(e);
    lv_draw_ctx_t * draw_ctx = lv_event_get_draw_ctx(e);

    // Determine color from style text color
    lv_color_t color = lv_obj_get_style_text_color(obj, LV_PART_MAIN);

    lv_area_t coords;
    lv_obj_get_coords(obj, &coords);

    ui_draw_icon(obj, draw_ctx, type, &coords, color);
}

lv_obj_t * ui_create_icon_canvas(lv_obj_t * parent, ui_icon_type_t type, lv_coord_t size, lv_color_t color) {
    lv_obj_t * obj = lv_obj_create(parent);
    lv_obj_set_size(obj, size, size);
    lv_obj_set_style_bg_opa(obj, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(obj, 0, 0);
    lv_obj_set_style_text_color(obj, color, 0);

    // Attach draw callback
    lv_obj_add_event_cb(obj, icon_event_cb, LV_EVENT_DRAW_MAIN, (void*)(intptr_t)type);

    return obj;
}
