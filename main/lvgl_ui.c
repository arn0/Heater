/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: CC0-1.0
 */

// This demo UI is adapted from LVGL official example: https://docs.lvgl.io/master/widgets/extra/meter.html#simple-meter

#include "time.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_spiffs.h"
#include "lvgl.h"

#include "task_priorities"
#include "heater.h"
#include "clock.h"
#include "logger.h"
#include "lvgl_ui.h"

#define DAY_TO_S(TimeInDays) ( ( time_t ) ( ( ( time_t ) ( TimeInDays ) * ( time_t ) 24*60*60 ) ) )

#ifdef CONFIG_EXAMPLE_ENABLE_LCD

static const char *TAG = "lvgl_ui";

bool lvg_ui_started = false;

#define DATA_POINTS 24*8

lv_coord_t data[DATA_POINTS];
lv_coord_t min, max;
int16_t i, data_points = 0;

static void analytics_create(lv_obj_t * parent);
static void chart_event_cb(lv_event_t * e);

#ifdef ENABLE_LOG
void stats_start();
#endif

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
static lv_style_t style_t_small;
static lv_style_t style_t_red;                      // for heater status
static lv_style_t style_t_blue;
static lv_style_t style_t_gray;

static lv_chart_series_t * series1;

static const lv_font_t * font_large;
static const lv_font_t * font_normal;
static const lv_font_t * font_small;

lv_color_t c_red = LV_COLOR_MAKE(255, 0, 0);
lv_color_t c_blue = LV_COLOR_MAKE(0, 0, 255);


void lvgl_ui_update(void)
{
	if(lvg_ui_started)
	{
		lv_label_set_text(obj_time, strftime_buf);
		lv_label_set_text_fmt(obj_t_4, "%.01f\u00b0C", heater_status.target);
		lv_label_set_text_fmt(obj_t_1, "%.01f\u00b0C", heater_status.fnt);
		lv_label_set_text_fmt(obj_t_2, "%.01f\u00b0C", heater_status.top);
		lv_label_set_text_fmt(obj_t_3, "%.01f\u00b0C", heater_status.bot);

		if(heater_status.one_on)
		{
			lv_obj_set_style_text_color(obj_h_1, c_red, LV_PART_MAIN);
			lv_obj_set_style_text_opa(obj_h_1, LV_OPA_COVER, LV_PART_MAIN);
		} else {
			lv_obj_set_style_text_color(obj_h_1, c_blue, LV_PART_MAIN);
			lv_obj_set_style_text_opa(obj_h_1, LV_OPA_40, LV_PART_MAIN);
		}

		if(heater_status.two_on)
		{
			lv_obj_set_style_text_color(obj_h_2, c_red, LV_PART_MAIN);
			lv_obj_set_style_text_opa(obj_h_2, LV_OPA_COVER, LV_PART_MAIN);
		} else {
			lv_obj_set_style_text_color(obj_h_2, c_blue, LV_PART_MAIN);
			lv_obj_set_style_text_opa(obj_h_2, LV_OPA_40, LV_PART_MAIN);
		}

		if(heater_status.wifi)
		{
			lv_obj_set_style_text_opa(obj_wifi, LV_OPA_COVER, LV_PART_MAIN);
		} else {
			lv_obj_set_style_text_opa(obj_wifi, LV_OPA_40, LV_PART_MAIN);
		}
	}
}

void example_lvgl_demo_ui(lv_disp_t *disp)
{
	font_large = &lv_font_montserrat_46;
	font_normal = LV_FONT_DEFAULT;
	font_small = &lv_font_montserrat_16;

	lv_style_init(&style_t_large);
	lv_style_set_text_font(&style_t_large, font_large);
	lv_style_init(&style_t_normal);
	lv_style_set_text_font(&style_t_normal, font_normal);
	lv_style_init(&style_t_small);
	lv_style_set_text_font(&style_t_small, font_small);

	lv_style_init(&style_t_red);
	lv_style_set_text_font(&style_t_red, font_normal);
	lv_style_set_text_color(&style_t_red, lv_color_make(255,0,0));
	lv_style_set_text_opa(&style_t_red, LV_OPA_COVER);

	lv_style_init(&style_t_blue);
	lv_style_set_text_font(&style_t_blue, font_normal);
	lv_style_set_text_color(&style_t_blue, lv_color_make(0,0,255));

	lv_style_init(&style_t_gray);
	lv_style_set_text_font(&style_t_gray, font_normal);
	lv_style_set_text_color(&style_t_gray, lv_color_make(0,0,255));
	lv_style_set_text_opa(&style_t_gray, LV_OPA_40);

	lv_obj_t *lv_scr = lv_disp_get_scr_act(disp);

	lv_obj_set_style_text_font(lv_scr, font_normal, 0);

	obj_t_1 = lv_label_create(lv_scr);                              // environment temperature
	lv_obj_add_style(obj_t_1, &style_t_large, LV_PART_MAIN);
	lv_label_set_text(obj_t_1, "--.-\u00b0C");
	lv_obj_align(obj_t_1, LV_ALIGN_CENTER, 0, -60);

	obj_t_2 = lv_label_create(lv_scr);                              // radiator top temperature
	lv_obj_add_style(obj_t_2, &style_t_normal, LV_PART_MAIN);
	lv_label_set_text(obj_t_2, "--.-\u00b0C");
	lv_obj_align(obj_t_2, LV_ALIGN_CENTER, -56, 52);

	obj_t_3 = lv_label_create(lv_scr);                              // radiator bottom temperature
	lv_obj_add_style(obj_t_3, &style_t_normal, LV_PART_MAIN);
	lv_label_set_text(obj_t_3, "-.-\u00b0C");
	lv_obj_align(obj_t_3, LV_ALIGN_CENTER, 56, 52);

	obj_t_4 = lv_label_create(lv_scr);                              // target temperature
	lv_obj_add_style(obj_t_4, &style_t_normal, LV_PART_MAIN);
	lv_label_set_text(obj_t_4, "--.-\u00b0C");
	lv_obj_align(obj_t_4, LV_ALIGN_CENTER, 0, -98);

	obj_time = lv_label_create(lv_scr);                             // clock
	lv_obj_add_style(obj_time, &style_t_small, LV_PART_MAIN);
	lv_label_set_text(obj_time, "--:--:--");
	lv_obj_align(obj_time, LV_ALIGN_CENTER, 0, 52+20);

	obj_h_1 = lv_label_create(lv_scr_act());                        // heater one status
	lv_obj_add_style(obj_h_1, &style_t_gray, LV_PART_MAIN);
	lv_obj_align(obj_h_1, LV_ALIGN_CENTER, 0-48, 52+20+12);
	lv_label_set_text(obj_h_1, LV_SYMBOL_POWER);

	obj_h_2 = lv_label_create(lv_scr_act());                        // heater two status
	lv_obj_add_style(obj_h_2, &style_t_gray, LV_PART_MAIN);
	lv_obj_align(obj_h_2, LV_ALIGN_CENTER, 0+48, 52+20+12);
	lv_label_set_text(obj_h_2, LV_SYMBOL_POWER);

	obj_wifi = lv_label_create(lv_scr);                             // wifi status
	lv_obj_add_style(obj_wifi, &style_t_gray, LV_PART_MAIN);
	lv_label_set_text(obj_wifi, LV_SYMBOL_WIFI);
	lv_obj_align(obj_wifi, LV_ALIGN_CENTER, 0, 52+24+24);

	analytics_create(lv_scr);
	lvg_ui_started = true;

	vTaskDelay(pdMS_TO_TICKS(5000));
	
#ifdef ENABLE_LOG
	stats_start();
#endif

	lv_obj_invalidate(lv_scr_act());
}

static void analytics_create(lv_obj_t * parent)
{
	obj_chart = lv_chart_create(parent);
	lv_chart_set_type(obj_chart, LV_CHART_TYPE_LINE);
	series1 = lv_chart_add_series(obj_chart, lv_palette_main(LV_PALETTE_RED), LV_CHART_AXIS_PRIMARY_Y);
	lv_chart_set_ext_y_array(obj_chart, series1, data);
	lv_chart_set_point_count(obj_chart, 1);
	lv_chart_set_update_mode(obj_chart, LV_CHART_UPDATE_MODE_SHIFT);
	lv_chart_set_all_value(obj_chart, series1, 0);
	lv_obj_set_size( obj_chart, 192+24, 64);
	lv_obj_align(obj_chart, LV_ALIGN_CENTER, 0, 0);
	lv_obj_set_style_radius(obj_chart, 0, LV_PART_MAIN);
	lv_obj_set_style_border_width(obj_chart, 0, LV_PART_MAIN);
	lv_obj_set_style_pad_all(obj_chart, 0, LV_PART_MAIN);
	lv_chart_set_div_line_count(obj_chart, 0, 0);

	lv_obj_set_style_size(obj_chart, 0, LV_PART_INDICATOR);
	//lv_obj_set_style_line_width(obj_chart, 0, LV_PART_ITEMS);

	lv_obj_add_event_cb(obj_chart, chart_event_cb, LV_EVENT_ALL, NULL);
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

void find_min_max()
{
	min = 0x7FFF;
	max = 0;
	int16_t i;

	for ( i = 0; i < data_points; i++)
	{
		if(min > data[i])
		{
			min = data[i];
		}
		if(max < data[i])
		{
			max = data[i];
		}
	}
}

#ifdef ENABLE_LOG

void stats_task() {
	TickType_t xPreviousWakeTime;
	TickType_t xTimeIncrement = pdMS_TO_TICKS(STATS_TASK_DELAY_MS);
	BaseType_t xWasDelayed;

	// Initialise the xLastWakeTime variable with the current time.
	xPreviousWakeTime = xTaskGetTickCount ();

	do {
		// Wait for the next cycle.
		xWasDelayed = xTaskDelayUntil( &xPreviousWakeTime, xTimeIncrement );

		if( xWasDelayed == pdFALSE ){
			ESP_LOGW( "stats", "Task was not delayed" );
		}
		if(data_points < DATA_POINTS){
			data[data_points++] = (int16_t) (heater_status.fnt * 100);
		} else {
			lv_chart_set_next_value(obj_chart, series1, (int16_t) (heater_status.fnt * 100));
		}
		find_min_max();
		if(max - min > 10) {
			lv_chart_set_range(obj_chart, LV_CHART_AXIS_PRIMARY_Y, min, max);
		} else {
			lv_chart_set_range(obj_chart, LV_CHART_AXIS_PRIMARY_Y, min, min + 10);

		}
		//lv_chart_refresh(obj_chart);
		ESP_LOGD( "stats", "New data_point temp %.03f\u00b0C value %d, total %d, min %d max %d delay %ld", heater_status.fnt, data[data_points-1], data_points, min, max, xTimeIncrement);

	} while (true);
}

#define STATS_FILE "/data/stats.bin"

esp_err_t stats_save(){
   struct stat file_stat;
	FILE *fd = NULL;
	size_t filesize;
	time_t now;

	if(data_points == 0){
		ESP_LOGW(TAG, "No datapoints!");
      return ESP_FAIL;
	}
	else{
		ESP_LOGW(TAG, "Have %d datapoints.", data_points);
	}
	if(stat(STATS_FILE, &file_stat) == 0){										// Is there a previous stats file?
		ESP_LOGI(TAG, "Deleting file : %s", STATS_FILE);
		unlink(STATS_FILE);																// Delete it
	}
   fd = fopen(STATS_FILE, "w");
   if(!fd) {
      ESP_LOGE(TAG, "Failed to create file : %s", STATS_FILE);
      return ESP_FAIL;
   }
	time(&now);
	if(fwrite(&now, 1, sizeof(now), fd) != sizeof(now)){
		fclose(fd);
		unlink(STATS_FILE);	
		ESP_LOGE(TAG, "File write failed!");
		return ESP_FAIL;
	}
	if(fwrite(&data_points, 1, sizeof(data_points), fd) != sizeof(data_points)){
		fclose(fd);
		unlink(STATS_FILE);	
		ESP_LOGE(TAG, "File write failed!");
		return ESP_FAIL;
	}
	filesize =  data_points * sizeof(data[0]);
	if(fwrite(&data, 1, filesize, fd) != filesize){
		fclose(fd);
		unlink(STATS_FILE);	
		ESP_LOGE(TAG, "File write failed!");
		return ESP_FAIL;
	}
   ESP_LOGI(TAG, "Writen to file : %s, %d bytes", STATS_FILE, sizeof(time_t) + sizeof(data_points) + filesize);
	fclose(fd);
	return(ESP_OK);
}

#define SIZE_TIME_T sizeof(time_t)
#define MAX_BUFFER_SIZE (8+4+(24*8*2)+4)

bool stats_read_postponed = false;

esp_err_t stats_read(){
	struct stat file_stat;
	size_t filesize;
	FILE *fd = NULL;
	void *buffer;
	time_t now, *saved_time;
	int16_t *saved_count, empty_count, count;
	lv_coord_t *saved_points, floor = 0x7FFF;

	if (stat(STATS_FILE, &file_stat) == -1) {									// File available?
		ESP_LOGE(TAG, "Failed to stat file: %s", STATS_FILE);
		return ESP_FAIL;
	}
	if(file_stat.st_size > MAX_BUFFER_SIZE){
		ESP_LOGE(TAG, "File %stoo long, %ld bytes", STATS_FILE, file_stat.st_size);
		return ESP_FAIL;
	}
	filesize = file_stat.st_size;
	buffer = malloc(filesize);
	if(buffer == NULL){
		ESP_LOGE(TAG, "Can not allocate %d bytes", filesize);
		return ESP_FAIL;
	}
	fd = fopen(STATS_FILE, "r");													// Open for reading
	if (!fd) {
		ESP_LOGE(TAG, "Failed to read existing file: %s", STATS_FILE);
		return ESP_FAIL;
	}
	if(fread(buffer, 1, filesize, fd) != filesize){
		fclose(fd);
		unlink(STATS_FILE);	
		ESP_LOGE(TAG, "File read failed!");
		return ESP_FAIL;
	}
	fclose(fd);

	time(&now);
	if(now < 60*60){
		stats_read_postponed = true;
		free(buffer);
		ESP_LOGW(TAG, "Time not valid, stats read defered.");
		return ESP_FAIL;
	}
	saved_time = buffer;
	saved_count = buffer + 8;
	saved_points = buffer + 10;
	
	if( *saved_time < (now - DAY_TO_S(1))){
		unlink(STATS_FILE);	
		free(buffer);
		ESP_LOGW(TAG, "Data more than 24 h old.");
		return ESP_FAIL;
	} else if(*saved_time > now){
		stats_read_postponed = true;
		free(buffer);
		ESP_LOGW(TAG, "File time in the future, read stats defered.");
		return ESP_FAIL;
	}
	empty_count = (now - *saved_time) / (60*7.5);
	ESP_LOGI(TAG, "Delta time %lld seconds or %lld minutes or %lld hours.", (now - *saved_time), (now - *saved_time)/60, (now - *saved_time)/(60*60));
	ESP_LOGI(TAG, "Empty count %d.", empty_count);
	ESP_LOGI(TAG, "Saved count %d.", *saved_count);

	if((empty_count + *saved_count) > DATA_POINTS)
	{
		count = DATA_POINTS - empty_count;
	}else{
		count = *saved_count;
	}
	for (size_t i = 0; i < count; i++)
	{
		if(*saved_points < floor)
		{
			floor = *saved_points;
		}
		ESP_LOGI(TAG, "Save point %d to %d", i, data_points);
		data[data_points++] = *saved_points++;
	}
	for (size_t i = 0; i < empty_count; i++)
	{
		ESP_LOGI(TAG, "Save empty point %d to %d", i, data_points);
		data[data_points++] = floor;
	}
	free(buffer);
	find_min_max();
	if(max - min > 10)
	{
		lv_chart_set_range(obj_chart, LV_CHART_AXIS_PRIMARY_Y, min, max);
	} else {
		lv_chart_set_range(obj_chart, LV_CHART_AXIS_PRIMARY_Y, min, min + 10);
	}
	stats_read_postponed = false;
	return ESP_OK;
}

void stats_start()
{
//	lv_chart_set_point_count(obj_chart, DATA_POINTS);
//	stats_read();
//	xTaskCreate( stats_task, "stats", 4096, NULL, STATS_TASK_PRIORITY, NULL );
}

#endif // ENABLE_LOG
#endif // CONFIG_EXAMPLE_ENABLE_LCD
