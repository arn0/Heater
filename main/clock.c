#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_sntp.h"

#include "task_priorities"
#include "lvgl_ui.h"


time_t previous, now;
char strftime_buf[64];
struct tm timeinfo;

static const char *TAG = "clock";


void clock_task()
{
	TickType_t xPreviousWakeTime;
	const TickType_t xTimeIncrement = pdMS_TO_TICKS(CLOCK_TASK_DELAY_MS);
	BaseType_t xWasDelayed;

	xPreviousWakeTime = xTaskGetTickCount ();									// Initialise the xPreviousWakeTime variable with the current time.

	do {
		// Wait for the next cycle.
		xWasDelayed = xTaskDelayUntil( &xPreviousWakeTime, xTimeIncrement );

		if( xWasDelayed == pdFALSE ){
			ESP_LOGW( TAG, "Task was not delayed" );
		}

		time(&now);
		if(now > previous){
			localtime_r(&now, &timeinfo);
			strftime(strftime_buf, sizeof(strftime_buf), "%k:%M:%S", &timeinfo);
			lvgl_ui_update();
		}
  }while (true);
}

void clock_start(void)
{
	//setenv("TZ", "CEST-1CET,M3.2.0/2:00:00,M11.1.0/2:00:00", 1);			// Set timezone to Central European Time
	//tzset();
	previous = 0;

	xTaskCreate( clock_task, "clock", 4096, NULL, CLOCK_TASK_PRIORITY, NULL );
}