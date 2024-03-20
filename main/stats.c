#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_log.h"
#include "lvgl.h"

#include "heater_task.h"
#include "task_priorities"
#include "lvgl_ui.h"

#define DATA_POINTS 24*6

int32_t data[DATA_POINTS];
int32_t min, max;
int16_t i;

void find_min_max()
{
	min = 0xFFFF;
	max = 0;

	for ( i = 0; i < DATA_POINTS; i++)
	{
		if(min > data[i])
		{
			min = data[i];
		}
		if(max < data[i])
		{
			max = data[i]
		}
	}
}

void set_scale()
{
	lv_chart_set_range(obj_chart, LV_CHART_AXIS_PRIMARY, min, max);
}

void add_data()
{
	lv_chart_set_next_value(obj_chart, series1, value);
}

void stats_task() {
	TickType_t xPreviousWakeTime;
	const TickType_t xTimeIncrement = pdMS_TO_TICKS(STATS_TASK_DELAY_MS);
	BaseType_t xWasDelayed;
	bool tick = false;

	// Initialise the xLastWakeTime variable with the current time.
	xPreviousWakeTime = xTaskGetTickCount ();

	do {
		// Wait for the next cycle.
		xWasDelayed = xTaskDelayUntil( &xPreviousWakeTime, xTimeIncrement );

		if( xWasDelayed == pdFALSE ){
			ESP_LOGW( TAG, "Task was not delayed" );
		}

		
	} while (true);
}

void stats_start()
{
	xTaskCreate( stats_task, "stats", 4096, NULL, STATS_TASK_PRIORITY, NULL );
}