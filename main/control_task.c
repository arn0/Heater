#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"


#include "control_task.h"
#include "heater_task.h"


/*
 * Control loop 
 *
 * 
 */

#define CONTROL_TASK_TIME_MS 500

static const char *TAG = "control_task";

void control_task(){
	TickType_t xLastWakeTime;
	const TickType_t xTimeIncrement = pdMS_TO_TICKS(CONTROL_TASK_TIME_MS);
	BaseType_t xWasDelayed;
	float delta;

	// Initialise the xLastWakeTime variable with the current time.
	xLastWakeTime = xTaskGetTickCount ();

	do{
		// Wait for the next cycle.
		xWasDelayed = xTaskDelayUntil( &xLastWakeTime, xTimeIncrement );

		if( xWasDelayed == pdFALSE ){
			ESP_LOGE( TAG, "Task ran out of time" );
		}

		heater_status.safe = true;

		// here we need to implement a feedback control loop
		// now just for testing

		delta = heater_status.target - heater_status.env;
		if( delta <= 0 ){
			heater_status.one_d = false;
			heater_status.two_d = false;
		} else if( delta > 2 ) {
			heater_status.one_d = true;
			heater_status.two_d = true;
		} else if( delta > 1 ) {
			heater_status.one_d = false;
			heater_status.two_d = true;
		} else {
			heater_status.one_d = true;
			heater_status.two_d = false;
		}




	}while (true);
}


bool start_control_task(){

	// First thing to do considering safety:
	// turn the heaters OFF

	heater_status.one_d = false;
	heater_status.two_d = false;
	heater_status.safe = false;

	// Now we start the contol loop
	// default priority is 5
	// lowest priority is 0, the idle task has priority zero
	// highest priority is configMAX_PRIORITIES - 1,
	// on this system 25 - 1 = 24
	// so a somewhat higher priority for this task: 8

	xTaskCreate( control_task, "control_task", 4096, NULL, 8, NULL );

	return(true);
}