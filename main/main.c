/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_event.h"
#include "nvs_flash.h"
#include "time.h"
#include "esp_wifi.h"

#include "heater_task.h"
#include "control_task.h"
#include "monitor.h"
#include "wifi_station.h"
#include "sntp.h"
#include "webmdns.h"
#include "events.h"
#include "webserver.h"
#include "gpio_pins.h"
#include "rgb_led.h"
#include "spi_lcd.h"
#include "clock.h"
#include "../../secret.h"


extern void real_time_stats(void);

static const char *TAG = "main";


void app_main(void)
{
	timezone_set();

	//Initialize NVS
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	//real_time_stats();
	start_heater_task();			// error check here
	start_monitor_task();
	lcd_start();
	//led_strip_start();
	start_control_task();
	clock_start();

	vTaskDelay(pdMS_TO_TICKS(1000));		// need a little time before wifi is ready

	ESP_LOGI(TAG, "Start wifi_init_station()");
	wifi_init_station();

	do {
		EventBits_t bits;
	 
		/* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
		 * number of re-tries (WIFI_DISCONNECTED_BIT). The bits are set by event_handler() */

		bits = xEventGroupWaitBits( s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_DISCONNECTED_BIT | SLEEP_WAKEUP_BIT | TCP_CONNECTED_BIT | TCP_FAILED_BIT, pdTRUE, pdFALSE, portMAX_DELAY );

		if ( bits & WIFI_CONNECTED_BIT ) {
			ESP_LOGI( TAG, "connected to ap SSID:%s", SECRET_SSID );

 			sntp_client_start();
			start_webserver();
			start_mdns_service();

		} else if ( bits & WIFI_DISCONNECTED_BIT ) {
			ESP_LOGI(TAG, "Failed to connect to SSID:%s", SECRET_SSID);
			ESP_ERROR_CHECK( esp_wifi_stop() );

			sntp_client_stop();

		} else if ( bits & SLEEP_WAKEUP_BIT ) {
			ESP_LOGI(TAG, "SLEEP_WAKEUP_BIT received");
			ESP_ERROR_CHECK( esp_wifi_start ());
		} else if( bits & TCP_CONNECTED_BIT ){
			ESP_LOGI(TAG, "TCP_CONNECTED_BIT received");
		} else if( bits & TCP_FAILED_BIT ){
			ESP_LOGI(TAG, "TCP_FAILED_BIT received");
		} else {
			ESP_LOGE(TAG, "UNEXPECTED EVENT");
		}
	 } while (true);
}
