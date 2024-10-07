#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"

#include "gpio_pins.h"
#include "heater.h"
#include "task_priorities"
#include "lvgl_ui.h"


static const char *TAG = "heaters";

struct heater_status heater_status = { .target = TARGET_DEFAULT, .safe = false, .one_gpio = false, .one_on = false, .two_gpio = false, .two_on = false, .blue = false };


void heater_task() {
	TickType_t xPreviousWakeTime;
	const TickType_t xTimeIncrement = pdMS_TO_TICKS( HEATER_TASK_DELAY_MS );
	BaseType_t xWasDelayed;
	bool tick = false;

	// Initialise the xLastWakeTime variable with the current time.
	xPreviousWakeTime = xTaskGetTickCount ();

	do {
		// set heaters

		if( heater_status.safe ) {
			if( tick ) {
				tick = false;
				if( heater_status.one_gpio != heater_status.one_on ) {
					if ( gpio_set_level( SSR_ONE_GPIO_PIN, heater_status.one_on ) != ESP_OK ) {
						heater_status.safe = false;
					} else {
						heater_status.one_on = heater_status.one_gpio;
					}
				}
			} else {
				tick = true;
				if( heater_status.two_gpio != heater_status.two_on ) {
					if ( gpio_set_level( SSR_TWO_GPIO_PIN, heater_status.two_on ) != ESP_OK ) {
						heater_status.safe = false;
					} else {
						heater_status.two_on = heater_status.two_gpio;
					}
				}
			}
		} else {
			gpio_set_level( SSR_ONE_GPIO_PIN, 0 );
			heater_status.one_gpio = false;
			heater_status.one_on = false;
			gpio_set_level( SSR_TWO_GPIO_PIN, 0 );
			heater_status.two_on = false;
			heater_status.two_gpio = false;
		}

		// Wait for the next cycle.
		xWasDelayed = xTaskDelayUntil( &xPreviousWakeTime, xTimeIncrement );

		if ( xWasDelayed == pdFALSE ) {
			ESP_LOGW( TAG, "Task was not delayed" );
		}
	} while (true);
}

/* Initialize requirements for the heater control loop
 * heater solid state relais gpio
 */

bool start_heater_task() {

	// First thing to do considering safety: set the GPIO pins
	// for the heater ssr's and turn the ssr's OFF

	esp_err_t ret;

	gpio_config_t io_conf = {};						//zero-initialize the config structure
	io_conf.intr_type = GPIO_INTR_DISABLE;			//disable interrupt
	io_conf.mode = GPIO_MODE_OUTPUT;					//set as output mode
	io_conf.pin_bit_mask = GPIO_OUTPUT_PIN_SEL;	//bit mask of the pins that you want to set, e.g. GPIO18/19
	io_conf.pull_down_en = 0;							//disable pull-down mode
	io_conf.pull_up_en = 0;								//disable pull-up mode
	ret = gpio_config( &io_conf) ;					//configure GPIO with the given settings
	if ( ret != ESP_OK ){
		ESP_ERROR_CHECK( ret );
		return( false );
	}
	ret = gpio_set_level( SSR_ONE_GPIO_PIN, 0 );
	if ( ret != ESP_OK ){
		ESP_ERROR_CHECK( ret );
		return( false );
	}
	ret = gpio_set_level( SSR_TWO_GPIO_PIN, 0 );
	if ( ret != ESP_OK ){
		ESP_ERROR_CHECK( ret );
		return( false );
	}

	// Now we start the heater contol loop

	xTaskCreate( heater_task, "heaters", 4096/2, NULL, HEATER_TASK_PRIORITY, NULL );

	return( true );
}