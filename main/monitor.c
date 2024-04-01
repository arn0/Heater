#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "driver/temperature_sensor.h"
#include "ds18b20.h"
#include "onewire_bus.h"

#include "gpio_pins.h"
#include "task_priorities"
#include "heater_task.h"
#include "lvgl_ui.h"
#include "logger.h"
#include "monitor.h"

#define HEATER_ONEWIRE_MAX_DS18B20 3							// No more then 3 installed

// ds18b20 addresses
#define DS18B20_ENV 0xC283138009646128
#define DS18B20_TOP 0x94116F8009646128
#define DS18B20_BOT 0x2CB9958109646128

static const char *TAG = "monitor";

// chip temperature sensor
temperature_sensor_handle_t temp_sensor = NULL;

ds18b20_device_handle_t ds18b20s[HEATER_ONEWIRE_MAX_DS18B20];

ds18b20_device_handle_t t_sensor_env = NULL;
ds18b20_device_handle_t t_sensor_top = NULL;
ds18b20_device_handle_t t_sensor_bot = NULL;

int ds18b20_device_num = 0;										// number of ds18b20 devices found

time_t log_saved_time, log_update_time, now;

void monitor_task(){
	TickType_t xPreviousWakeTime;
	const TickType_t xTimeIncrement = pdMS_TO_TICKS(MONITOR_TASK_DELAY_MS);
	BaseType_t xWasDelayed;
	int step = 0;

	xPreviousWakeTime = xTaskGetTickCount ();					// Initialise the xPreviousWakeTime variable with the current time

	do {
		xWasDelayed = xTaskDelayUntil( &xPreviousWakeTime, xTimeIncrement );					// Wait for the next cycle

		if( xWasDelayed == pdFALSE ) {
			ESP_LOGW(TAG, "Task was not delayed");
		}
		
		switch( step++ ) {
			case 0:
				// chip temperature reading
				ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_sensor, &heater_status.chip));
				heater_status.web |= CHP_W_FL;
				break;

				// ds18b20 temperature reading
			case 1:
				if(t_sensor_env){
					ESP_ERROR_CHECK(ds18b20_trigger_temperature_conversion(t_sensor_env));
				}
				break;

			case 2:
				if(t_sensor_env){
					ESP_ERROR_CHECK(ds18b20_get_temperature(t_sensor_env, &heater_status.env));
					heater_status.web |= ENV_W_FL;
				}
				break;

			case 3:
				if(t_sensor_top){
					ESP_ERROR_CHECK(ds18b20_trigger_temperature_conversion(t_sensor_top));
				}
				break;

			case 4:
				if(t_sensor_top){
					ESP_ERROR_CHECK(ds18b20_get_temperature(t_sensor_top, &heater_status.top));
					heater_status.web |= TOP_W_FL;
				}
				break;

			case 5:
				if(t_sensor_bot){
					ESP_ERROR_CHECK(ds18b20_trigger_temperature_conversion(t_sensor_bot));
				}
				break;

			case 6:
				if(t_sensor_bot){
					ESP_ERROR_CHECK(ds18b20_get_temperature(t_sensor_bot, &heater_status.bot));
					heater_status.web |= BOT_W_FL;
				}
				break;

			case 7:
				time(&now);
				if(now > log_update_time + 60){
					log_add();
					log_update_time = now;
				}
				if(now > log_saved_time + 60*10){
					//log_save();
					log_saved_time = now;
				}
				break;

			default:
				step = 0;
		}
	} while (true);
}

/* Initialize requirements for the heater control loop
 * previously stored target temperature
 * all available temperature sensors
 * heater solid state relais
*/

bool start_monitor_task(){

	// start internal on-chip temperature sensor
	// FIXME: need to go over config details!!!
	ESP_LOGI(TAG, "Install internal temperature sensor, expected temp ranger range: 10~50 ℃");
	temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);
	ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor_config, &temp_sensor));

	ESP_LOGI(TAG, "Enable temperature sensor");
	ESP_ERROR_CHECK(temperature_sensor_enable(temp_sensor));


	// install 1-wire bus
	onewire_bus_handle_t bus = NULL;
	onewire_bus_config_t bus_config = { .bus_gpio_num = HEATER_ONEWIRE_BUS_GPIO,    };
	
	// 1 byte ROM command + 8 byte ROM number + 1 byte device command
	onewire_bus_rmt_config_t rmt_config = { .max_rx_bytes = 10,     };
	ESP_ERROR_CHECK(onewire_new_bus_rmt(&bus_config, &rmt_config, &bus));

	onewire_device_iter_handle_t iter = NULL;
	onewire_device_t next_onewire_device;
	esp_err_t search_result = ESP_OK;

	// create 1-wire device iterator, which is used for device search
	ESP_ERROR_CHECK(onewire_new_device_iter(bus, &iter));
	ESP_LOGI(TAG, "Device iterator created, start searching...");
	do {
		search_result = onewire_device_iter_get_next(iter, &next_onewire_device);
		if (search_result == ESP_OK) { // found a new device, let's check if we can upgrade it to a DS18B20
			ds18b20_config_t ds_cfg = {};
			// check if the device is a DS18B20, if so, return the ds18b20 handle
			if (ds18b20_new_device(&next_onewire_device, &ds_cfg, &ds18b20s[ds18b20_device_num]) == ESP_OK) {
				ESP_LOGI(TAG, "Found a DS18B20[%d], address: %016llX", ds18b20_device_num, next_onewire_device.address);
				ds18b20_device_num++;
			} else {
				ESP_LOGI(TAG, "Found an unknown device, address: %016llX", next_onewire_device.address);
			}
		}
	} while (search_result != ESP_ERR_NOT_FOUND);
	ESP_ERROR_CHECK(onewire_del_device_iter(iter));
	ESP_LOGI(TAG, "Searching done, %d DS18B20 device(s) found", ds18b20_device_num);

	// Now you have the DS18B20 sensor handle, you can use it to read the temperature

	// Here we have to match the found sensors to the position these are mounted

	t_sensor_env = ds18b20s[0];
	t_sensor_top = ds18b20s[1];
	t_sensor_bot = ds18b20s[2];

	time(&log_saved_time);
	log_update_time = log_saved_time;
	//log_read();

	xTaskCreate( monitor_task, "monitor", 4096, NULL, MONITOR_TASK_PRIORITY, NULL );

	return(true);
}