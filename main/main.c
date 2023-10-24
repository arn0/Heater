/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"
#include "nvs_flash.h"
#include "time.h"
#include "esp_netif_sntp.h"


#include <string.h>
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"






#include "heater_task.h"
#include "t_monitor_task.h"
#include "wifi_station.h"
#include "webmdns.h"
#include "events.h"
#include "webserver.h"
#include "gpio_pins.h"
#include "rgb_led.h"
#include "../../secret.h"




static const char *TAG = "example";


void app_main(void)
{
	//Initialize NVS
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	start_heater_task();	// error check here
	start_t_monitor_task();
	led_strip_start();


	setenv( "TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 100 );		// set timezone and daylight saving time
	tzset();

	ESP_LOGI(TAG, "Start wifi_init_station()");
	wifi_init_station();









    while (true){
    
    
	//esp_sntp_config_t sntp_config = ESP_NETIF_SNTP_DEFAULT_CONFIG( SECRET_ADDR );
	EventBits_t bits;
	TaskHandle_t xHandle_control_loop = NULL;
    
    
   		/* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
		 * number of re-tries (WIFI_DISCONNECTED_BIT). The bits are set by event_handler() (see above) */

		bits = xEventGroupWaitBits( s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_DISCONNECTED_BIT | SLEEP_WAKEUP_BIT | TCP_CONNECTED_BIT | TCP_FAILED_BIT, pdTRUE, pdFALSE, portMAX_DELAY );

		if ( bits & WIFI_CONNECTED_BIT ) {
			ESP_LOGI( TAG, "connected to ap SSID:%s", SECRET_SSID );
			//esp_netif_sntp_init( &sntp_config );						/* Use sntp server to set system time */
     		//if ( esp_netif_sntp_sync_wait( pdMS_TO_TICKS( 10000 ) ) != ESP_OK ) {
         	//ESP_LOGE( TAG, "Failed to update system time within 10s timeout" );
 			//}
 			start_webserver();
			start_mdns_service();

            //xTaskCreate( tcp_transport_client_task, "tcp_transport_client", 4096, NULL, 5, NULL );
		} else if ( bits & WIFI_DISCONNECTED_BIT ) {
			ESP_LOGI(TAG, "Failed to connect to SSID:%s", SECRET_SSID);
			//vTaskDelay( 200 / portTICK_PERIOD_MS );
			ESP_ERROR_CHECK( esp_wifi_stop() );
			//xTaskCreate(light_sleep_task, "light_sleep_task", 4096, s_wifi_event_group, 6, NULL);
		} else if ( bits & SLEEP_WAKEUP_BIT ) {
			ESP_LOGI(TAG, "SLEEP_WAKEUP_BIT received");
			ESP_ERROR_CHECK( esp_wifi_start ());
		} else if( bits & TCP_CONNECTED_BIT ){
			ESP_LOGI(TAG, "TCP_CONNECTED_BIT received");
			/* next step */
 			//xTaskCreate( control_loop, "control_loop", 4096, NULL, 5, &xHandle_control_loop );
		} else if( bits & TCP_FAILED_BIT ){
			ESP_LOGI(TAG, "TCP_FAILED_BIT received");
			/* kill control loop task */
			if( xHandle_control_loop ) {
				//vTaskDelete( xHandle_control_loop );
				xHandle_control_loop = NULL;
			}
			/* wait and start tcp task again */
			//vTaskDelay( 15000 / portTICK_PERIOD_MS );
 			//xTaskCreate( tcp_transport_client_task, "tcp_transport_client", 4096, NULL, 5, NULL );
		} else {
			ESP_LOGE(TAG, "UNEXPECTED EVENT");
		}

    
    
    
    
    
    
    
    }
}
