#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_system.h"
#include "esp_log.h"

#include "gpio_pins.h"
#include "heater_task.h"
#include "task_priorities"
#include "lvgl_ui.h"


/*
 * Let's say, GPIO_OUTPUT_IO_0=18, GPIO_OUTPUT_IO_1=19
 * In binary representation,
 * 1ULL<<GPIO_OUTPUT_IO_0 is equal to 0000000000000000000001000000000000000000 and
 * 1ULL<<GPIO_OUTPUT_IO_1 is equal to 0000000000000000000010000000000000000000
 * GPIO_OUTPUT_PIN_SEL                0000000000000000000011000000000000000000
 */

#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<HEATER_SSR_ONE_GPIO) | (1ULL<<HEATER_SSR_TWO_GPIO))


static const char *TAG = "heaters";

struct heat_stat heater_status;										// global access to heater staus


void heater_task() {
	TickType_t xPreviousWakeTime;
	const TickType_t xTimeIncrement = pdMS_TO_TICKS(HEATER_TASK_DELAY_MS);
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
		
		// set heaters

		if( heater_status.safe ) {
			if( tick ){
				tick = false;
				if( heater_status.one_d != heater_status.one_s ){
					heater_status.one_s = heater_status.one_d;
					heater_status.web |= ONE_W_FL;
					gpio_set_level( HEATER_SSR_ONE_GPIO, heater_status.one_s );
				}
			} else {
				tick = true;
				if( heater_status.two_d != heater_status.two_s ){
					heater_status.two_s = heater_status.two_d;
					heater_status.web |= TWO_W_FL;
					gpio_set_level( HEATER_SSR_TWO_GPIO, heater_status.two_s );
				}
			}
		} else {
			gpio_set_level( HEATER_SSR_ONE_GPIO, 0 );
			gpio_set_level( HEATER_SSR_TWO_GPIO, 0 );
		}
	} while (true);
}

/* Initialize requirements for the heater control loop
 * previously stored target temperature
 * all available temperature sensors
 * heater solid state relais
*/

bool start_heater_task(){

	// First thing to do considering safety:
	// set the GPIO pins for the heater ssr's
	// and turn the ssr's OFF

	esp_err_t ret;

	gpio_config_t io_conf = {};							//zero-initialize the config structure
	io_conf.intr_type = GPIO_INTR_DISABLE;				//disable interrupt
	io_conf.mode = GPIO_MODE_OUTPUT;						//set as output mode
	io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;		//bit mask of the pins that you want to set, e.g. GPIO18/19
	io_conf.pull_down_en = 0;								//disable pull-down mode
	io_conf.pull_up_en = 0;									//disable pull-up mode
	ret = gpio_config(&io_conf);							//configure GPIO with the given settings
	if( ret != ESP_OK ){
		ESP_ERROR_CHECK(ret);
		return(false);
	}
	ret = gpio_set_level(HEATER_SSR_ONE_GPIO, 0);
	if( ret != ESP_OK ){
		ESP_ERROR_CHECK(ret);
		return(false);
	}
	ret = gpio_set_level(HEATER_SSR_TWO_GPIO, 0);
	if( ret != ESP_OK ){
		ESP_ERROR_CHECK(ret);
		return(false);
	}

	heater_status.safe = false;							// initialize heater status
	heater_status.one_s = false;
	heater_status.two_s =false;
	heater_status.one_d = false;
	heater_status.two_d = false;
	heater_status.target = 20;
	
	// Now we start the heater contol loop

	xTaskCreate( heater_task, "heaters", 4096, NULL, HEATER_TASK_PRIORITY, NULL );

	return(true);
}