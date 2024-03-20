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
#include "esp_netif_sntp.h"
#include "lwip/ip_addr.h"
#include "esp_sntp.h"

#include "heater_task.h"
#include "control_task.h"
#include "t_monitor_task.h"
#include "wifi_station.h"
#include "webmdns.h"
#include "events.h"
#include "webserver.h"
#include "gpio_pins.h"
#include "rgb_led.h"
#include "spi_lcd.h"
#include "clock.h"
#include "../../secret.h"

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 48
#endif


extern void real_time_stats(void);

static const char *TAG = "main";

static void print_servers(void)
{
    ESP_LOGI(TAG, "List of configured NTP servers:");

    for (uint8_t i = 0; i < SNTP_MAX_SERVERS; ++i){
        if (esp_sntp_getservername(i)){
            ESP_LOGI(TAG, "server %d: %s", i, esp_sntp_getservername(i));
        } else {
            // we have either IPv4 or IPv6 address, let's print it
            char buff[INET6_ADDRSTRLEN];
            ip_addr_t const *ip = esp_sntp_getserver(i);
            if (ipaddr_ntoa_r(ip, buff, INET6_ADDRSTRLEN) != NULL)
                ESP_LOGI(TAG, "server %d: %s", i, buff);
        }
    }
}

static void sntp_start(void)
{
   ESP_LOGI(TAG, "Initializing and starting SNTP");
	esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_SNTP_TIME_SERVER);
	config.start = false;											// start SNTP service explicitly (after connecting)
	//config.server_from_dhcp = true;								// accept NTP offers from DHCP server, if any (need to enable *before* connecting)
	//config.renew_servers_after_new_IP = true;					// let esp-netif update configured SNTP server(s) after receiving DHCP lease
	//config.index_of_first_server = 1;							// updates from server num 1, leaving server 0 (from DHCP) intact
	esp_netif_sntp_init(&config);
}

void app_main(void)
{
	setenv( "TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 100 );		// set timezone and daylight saving time
	tzset();

	//Initialize NVS
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	real_time_stats();
	start_heater_task();			// error check here
	start_t_monitor_task();
	lcd_start();
	//led_strip_start();
	start_control_task();
	clock_start();

	ESP_LOGI(TAG, "Start wifi_init_station()");
	wifi_init_station();

	do {
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
 			sntp_start();
			start_webserver();
			start_mdns_service();
			print_servers();


				//xTaskCreate( tcp_transport_client_task, "tcp_transport_client", 4096, NULL, 5, NULL );
		} else if ( bits & WIFI_DISCONNECTED_BIT ) {
			ESP_LOGI(TAG, "Failed to connect to SSID:%s", SECRET_SSID);
			//vTaskDelay( 200 / portTICK_PERIOD_MS );
			ESP_ERROR_CHECK( esp_wifi_stop() );

	   	esp_netif_sntp_deinit();

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
	 } while (true);
}
