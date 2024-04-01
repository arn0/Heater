#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include "task_priorities"
#include "control_task.h"
#include "heater_task.h"
#include "lvgl_ui.h"


/*
 * Control loop 
 *
 * 
 */

static const char *TAG = "control_task";

void control_task(){
	TickType_t xPreviousWakeTime;
	const TickType_t xTimeIncrement = pdMS_TO_TICKS(CONTROL_TASK_DELAY_MS);
	BaseType_t xWasDelayed;
	float delta;

	// Initialise the xLastWakeTime variable with the current time.
	xPreviousWakeTime = xTaskGetTickCount ();

	do{
		// Wait for the next cycle.
		xWasDelayed = xTaskDelayUntil( &xPreviousWakeTime, xTimeIncrement );

		if( xWasDelayed == pdFALSE ){
			ESP_LOGE( TAG, "Task was not delayed" );
		}

		heater_status.safe = true;

		// here we need to implement a feedback control loop
		// now just for testing

		delta = heater_status.target - heater_status.env;
		if( delta <= 0 ){
			heater_status.one_set = false;
			heater_status.two_set = false;
		} else if( delta > 0.6 ) {
			heater_status.one_set = true;
			heater_status.two_set = true;
		} else if( delta > 0.3 ) {
			heater_status.one_set = false;
			heater_status.two_set = true;
		} else {
			heater_status.one_set = true;
			heater_status.two_set = false;
		}
	}while (true);
}


bool start_control_task(){

	// First thing to do considering safety:
	// turn the heaters OFF

	heater_status.one_set = false;
	heater_status.two_set = false;
	heater_status.safe = false;

	// Now we start the contol loop

	xTaskCreate( control_task, "control_task", 4096, NULL, CONTROL_TASK_PRIORITY, NULL );

	return(true);
}