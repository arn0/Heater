#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "esp_netif_sntp.h"
#include "lwip/ip_addr.h"
#include "esp_sntp.h"

#include "lvgl_ui.h"
#include "sntp.h"

static const char *TAG = "sntp";
bool sntp_started = false;
bool sntp_valid = false;


void time_sync_notification_cb(struct timeval *tv)
{
	sntp_valid = true;
	ESP_LOGI(TAG, "Notification of a time synchronization event");
	if(stats_read_postponed == true){
		stats_read();
	}
}

void timezone_set()
{
	setenv( "TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 100 );		// set timezone and daylight saving time
	tzset();
}

void sntp_client_start(){
	if(sntp_started){
			ESP_LOGW(TAG, "SNTP already started");
	} else {
		ESP_LOGI(TAG, "Initializing and starting SNTP");
		/*
		 * This is the basic default config with one server and starting the service
		 */
		esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_SNTP_TIME_SERVER);
		//config.start = false;											// start SNTP service explicitly (after connecting)
		config.sync_cb = time_sync_notification_cb;     // Note: This is only needed if we want

		if(esp_netif_sntp_init(&config) == ESP_OK){
			sntp_started = true;
		}else{
			ESP_LOGE(TAG, "Error on starting SNTP");
		}
	}
}

void sntp_client_stop(){
	sntp_started = false;
	ESP_LOGI(TAG, "Stopping SNTP");
	esp_netif_sntp_deinit();
	sntp_started = false;
}
