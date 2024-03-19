/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

// This demo UI is adapted from LVGL official example: https://docs.lvgl.io/master/widgets/extra/meter.html#simple-meter

//#define LV_CONF_INCLUDE_SIMPLE

#include "lvgl.h"

#include "lvgl_ui.h"

static void analytics_create(lv_obj_t * parent);
static void chart_event_cb(lv_event_t * e);

static lv_obj_t * obj_t_1;
static lv_obj_t * obj_t_2;
static lv_obj_t * obj_t_3;
static lv_obj_t * obj_t_4;
static lv_obj_t * obj_h_1;
static lv_obj_t * obj_h_2;
static lv_obj_t * obj_wifi;
static lv_obj_t * obj_chart;
static lv_obj_t * obj_time;

static lv_style_t style_t_large;
static lv_style_t style_t_normal;
static lv_style_t style_t_red;
static lv_style_t style_t_blue;

static lv_chart_series_t * series1;

static const lv_font_t * font_large;
static const lv_font_t * font_normal;

void lvgl_ui_set_t_env(float t){
    lv_label_set_text_fmt(obj_t_1, "%.01f\u00b0C", t);
}

void lvgl_ui_set_t_top(float t){
    lv_label_set_text_fmt(obj_t_2, "%.01f\u00b0C", t);
}

void lvgl_ui_set_t_bot(float t){
    lv_label_set_text_fmt(obj_t_3, "%.01f\u00b0C", t);
}

void lvgl_ui_set_t_tar(float t){
    lv_label_set_text_fmt(obj_t_4, "%.01f\u00b0C", t);
}

void lvgl_ui_set_time(char *string){
    lv_label_set_text(obj_time, string);
}

void lvgl_ui_set_t_h1(bool t)
{
    if(t)
        lv_obj_add_style(obj_h_1, &style_t_red, LV_PART_MAIN);
    else
        lv_obj_add_style(obj_h_1, &style_t_blue, LV_PART_MAIN);
}

void lvgl_ui_set_t_h2(bool t)
{
    if(t)
        lv_obj_add_style(obj_h_2, &style_t_red, LV_PART_MAIN);
    else
        lv_obj_add_style(obj_h_2, &style_t_blue, LV_PART_MAIN);
}

void lvgl_ui_set_wifi(bool t)
{

}

void example_lvgl_demo_ui(lv_disp_t *disp)
{
    font_large = &lv_font_montserrat_48;
    font_normal = LV_FONT_DEFAULT;

    lv_style_init(&style_t_large);
    lv_style_set_text_font(&style_t_large, font_large);
    lv_style_init(&style_t_normal);
    lv_style_set_text_font(&style_t_normal, font_normal);

    lv_style_init(&style_t_red);
    lv_style_set_text_font(&style_t_red, font_normal);
    lv_style_set_text_color(&style_t_red, lv_color_make(255,0,0));
    lv_style_set_text_opa(&style_t_red, LV_OPA_COVER);

    lv_style_init(&style_t_blue);
    lv_style_set_text_font(&style_t_blue, font_normal);
    lv_style_set_text_color(&style_t_blue, lv_color_make(0,0,255));
    lv_style_set_text_opa(&style_t_blue, LV_OPA_40);
    lv_style_set_text_font(&style_t_blue, font_normal);

    lv_obj_t *lv_scr = lv_disp_get_scr_act(disp);

    lv_obj_set_style_text_font(lv_scr, font_normal, 0);

    obj_t_1 = lv_label_create(lv_scr);
    lv_obj_add_style(obj_t_1, &style_t_large, LV_PART_MAIN);
    lv_label_set_text(obj_t_1, "--.-\u00b0C");
    lv_obj_align(obj_t_1, LV_ALIGN_CENTER, 0, -60);

    obj_t_2 = lv_label_create(lv_scr);
    lv_obj_add_style(obj_t_2, &style_t_normal, LV_PART_MAIN);
    lv_label_set_text(obj_t_2, "--.-\u00b0C");
    lv_obj_align(obj_t_2, LV_ALIGN_CENTER, -50, 52);

    obj_t_3 = lv_label_create(lv_scr);
    lv_obj_add_style(obj_t_3, &style_t_normal, LV_PART_MAIN);
    lv_label_set_text(obj_t_3, "-.-\u00b0C");
    lv_obj_align(obj_t_3, LV_ALIGN_CENTER, 50, 52);

    obj_t_4 = lv_label_create(lv_scr);
    lv_obj_add_style(obj_t_4, &style_t_normal, LV_PART_MAIN);
    lv_label_set_text(obj_t_4, "--.-\u00b0C");
    lv_obj_align(obj_t_4, LV_ALIGN_CENTER, 0, -92);

    obj_time = lv_label_create(lv_scr);
    lv_obj_add_style(obj_time, &style_t_normal, LV_PART_MAIN);
    lv_label_set_text(obj_time, "12:22:22");
    lv_obj_align(obj_time, LV_ALIGN_CENTER, 0, 52+20);

    obj_h_1 = lv_label_create(lv_scr_act());
    lv_obj_add_style(obj_h_1, &style_t_blue, LV_PART_MAIN);
    lv_obj_align(obj_h_1, LV_ALIGN_CENTER, 0-48, 52+20+12);
    lv_label_set_text(obj_h_1, LV_SYMBOL_POWER);

    obj_h_2 = lv_label_create(lv_scr_act());
    lv_obj_add_style(obj_h_2, &style_t_blue, LV_PART_MAIN);
    lv_obj_align(obj_h_2, LV_ALIGN_CENTER, 0+48, 52+20+12);
    lv_label_set_text(obj_h_2, LV_SYMBOL_POWER);

    obj_wifi = lv_label_create(lv_scr);
    lv_obj_add_style(obj_wifi, &style_t_normal, LV_PART_MAIN);
    lv_label_set_text(obj_wifi, LV_SYMBOL_WIFI);
    lv_obj_align(obj_wifi, LV_ALIGN_CENTER, 0, 52+24+24);

    analytics_create(lv_scr);
}
static void analytics_create(lv_obj_t * parent)
{
    obj_chart = lv_chart_create(parent);
	lv_chart_set_type(obj_chart, LV_CHART_TYPE_LINE);
    lv_chart_set_point_count(obj_chart, 12*4);
    lv_obj_set_size( obj_chart, 192, 64);
    lv_obj_align(obj_chart, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_radius(obj_chart, 0, LV_PART_MAIN);
    lv_obj_set_style_border_width(obj_chart, 0, LV_PART_MAIN);
    lv_obj_set_style_pad_all(obj_chart, 0, LV_PART_MAIN);
    lv_chart_set_div_line_count(obj_chart, 0, 0);

    lv_obj_set_style_size(obj_chart, 0, LV_PART_INDICATOR);
    //lv_obj_set_style_line_width(obj_chart, 0, LV_PART_ITEMS);

    lv_obj_add_event_cb(obj_chart, chart_event_cb, LV_EVENT_ALL, NULL);


    series1 = lv_chart_add_series(obj_chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
    lv_chart_set_next_value(obj_chart, series1, lv_rand(0,100));
}

static void chart_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * obj = lv_event_get_target(e);

    if(code == LV_EVENT_PRESSED || code == LV_EVENT_RELEASED) {
        lv_obj_invalidate(obj); /*To make the value boxes visible*/
    }
    else if(code == LV_EVENT_DRAW_PART_BEGIN) {
        lv_obj_draw_part_dsc_t * dsc = lv_event_get_param(e);

        /*Add the faded area before the lines are drawn */
        if(dsc->part == LV_PART_ITEMS) {
            /*Add  a line mask that keeps the area below the line*/
            if(dsc->p1 && dsc->p2) {
                lv_draw_mask_line_param_t line_mask_param;
                lv_draw_mask_line_points_init(&line_mask_param, dsc->p1->x, dsc->p1->y, dsc->p2->x, dsc->p2->y, LV_DRAW_MASK_LINE_SIDE_BOTTOM);
                int16_t line_mask_id = lv_draw_mask_add(&line_mask_param, NULL);

                /*Add a fade effect: transparent bottom covering top*/
                lv_coord_t h = lv_obj_get_height(obj);
                lv_draw_mask_fade_param_t fade_mask_param;
                lv_draw_mask_fade_init(&fade_mask_param, &obj->coords, LV_OPA_COVER, obj->coords.y1 + h / 8, LV_OPA_70, obj->coords.y2);
                int16_t fade_mask_id = lv_draw_mask_add(&fade_mask_param, NULL);

                /*Draw a rectangle that will be affected by the mask*/
                lv_draw_rect_dsc_t draw_rect_dsc;
                lv_draw_rect_dsc_init(&draw_rect_dsc);
                draw_rect_dsc.bg_opa = LV_OPA_COVER;
                draw_rect_dsc.bg_color = dsc->line_dsc->color;
                //draw_rect_dsc.bg_color = lv_color_make(127,127,255);
                //draw_rect_dsc.bg_grad.dir = LV_GRAD_DIR_VER;
                //draw_rect_dsc.bg_grad.stops->color = lv_color_make(255, 127, 127);

                lv_area_t obj_clip_area;
                _lv_area_intersect(&obj_clip_area, dsc->draw_ctx->clip_area, &obj->coords);
                const lv_area_t * clip_area_ori = dsc->draw_ctx->clip_area;
                dsc->draw_ctx->clip_area = &obj_clip_area;
                lv_area_t a;
                a.x1 = dsc->p1->x;
                a.x2 = dsc->p2->x - 1;
                a.y1 = LV_MIN(dsc->p1->y, dsc->p2->y);
                a.y2 = obj->coords.y2;
                lv_draw_rect(dsc->draw_ctx, &draw_rect_dsc, &a);
                dsc->draw_ctx->clip_area = clip_area_ori;
                /*Remove the masks*/
                lv_draw_mask_remove_id(line_mask_id);
                lv_draw_mask_remove_id(fade_mask_id);
            }
        }
    }
}
