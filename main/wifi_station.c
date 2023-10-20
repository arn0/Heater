/* WiFi station Example

	This example code is in the Public Domain (or CC0 licensed, at your option.)

	Unless required by applicable law or agreed to in writing, this
	software is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
	CONDITIONS OF ANY KIND, either express or implied.
*/
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "esp_system.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_log.h"
#include "nvs_flash.h"

#include "lwip/err.h"
#include "lwip/sys.h"

#include "events.h"
#include "wifi_station.h"
#include "../../secret.h"

#define WIFI_MAXIMUM_RETRY 4
#define WIFI_WAIT_TIME 2000


static const char *TAG = "> wifi station";

/* FreeRTOS event group to signal when we are connected*/
EventGroupHandle_t s_wifi_event_group;

static int s_retry_num = 0;

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	if (event_base == WIFI_EVENT) {
		switch (event_id) {
		case WIFI_EVENT_WIFI_READY: /**< WiFi ready */
			ESP_LOGI(TAG, "event handler: WIFI_EVENT_WIFI_READY");
			break;
		case WIFI_EVENT_SCAN_DONE: /**< Finished scanning AP */
			ESP_LOGI(TAG, "event handler: WIFI_EVENT_SCAN_DONE");
			break;
		case WIFI_EVENT_STA_START: /**< Station start */
			ESP_LOGI(TAG, "event handler: WIFI_EVENT_STA_START");
			esp_wifi_connect();
			ESP_LOGI(TAG, "event handler: esp_wifi_connect() done");
			break;
		case WIFI_EVENT_STA_STOP: /**< Station stop */
			ESP_LOGI(TAG, "event handler: WIFI_EVENT_STA_STOP");
			break;
		case WIFI_EVENT_STA_CONNECTED: /**< Station connected to AP */
			ESP_LOGI(TAG, "event handler: WIFI_EVENT_STA_CONNECTED");
			break;
		case WIFI_EVENT_STA_DISCONNECTED: /**< Station disconnected from AP */
			ESP_LOGI(TAG, "event handler: WIFI_EVENT_STA_DISCONNECTED");
			if (s_retry_num < WIFI_MAXIMUM_RETRY) {
				vTaskDelay( WIFI_WAIT_TIME / portTICK_PERIOD_MS );
				esp_wifi_connect();
				ESP_LOGI(TAG, "event handler: esp_wifi_connect() done, retry %d", s_retry_num);
				s_retry_num++;
			} else {
				xEventGroupSetBits(s_wifi_event_group, WIFI_DISCONNECTED_BIT);
				ESP_LOGI(TAG, "event handler: xEventGroupSetBits(s_wifi_event_group, WIFI_DISCONNECTED_BIT) done");
				s_retry_num = 0;
			}
			ESP_LOGI(TAG, "event handler: connect to the AP failed");
			break;
		case WIFI_EVENT_STA_AUTHMODE_CHANGE: /**< the auth mode of AP connected by device's station changed */
			ESP_LOGI(TAG, "event handler: WIFI_EVENT_STA_AUTHMODE_CHANGE");
			break;
		default:
			ESP_LOGI(TAG, "event handler: event_id = %lx", event_id);
			break;
		}
	} else {
		ESP_LOGE(TAG, "event handler: event_base = %s ", event_base);
	}
}

static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	if (event_base == IP_EVENT) {
		switch (event_id) {
		case IP_EVENT_STA_GOT_IP: /*!< station got IP from connected AP */
			ESP_LOGI(TAG, "event handler: IP_EVENT_STA_GOT_IP");
			ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
			ESP_LOGI(TAG, "event handler: got ip:" IPSTR, IP2STR(&event->ip_info.ip));
			s_retry_num = 0;
			xEventGroupSetBits(s_wifi_event_group, WIFI_CONNECTED_BIT);
			break;
		case IP_EVENT_STA_LOST_IP: /*!< station lost IP and the IP is reset to 0 */
			ESP_LOGI(TAG, "event handler: IP_EVENT_STA_LOST_IP");
			break;
		case IP_EVENT_AP_STAIPASSIGNED: /*!< soft-AP assign an IP to a connected station */
			ESP_LOGI(TAG, "event handler: IP_EVENT_AP_STAIPASSIGNED");
			break;
		case IP_EVENT_GOT_IP6: /*!< station or ap or ethernet interface v6IP addr is preferred */
			ESP_LOGI(TAG, "event handler: IP_EVENT_GOT_IP6");
			break;
		case IP_EVENT_ETH_GOT_IP: /*!< ethernet got IP from connected AP */
			ESP_LOGI(TAG, "event handler: IP_EVENT_ETH_GOT_IP");
			break;
		case IP_EVENT_ETH_LOST_IP: /*!< ethernet lost IP and the IP is reset to 0 */
			ESP_LOGI(TAG, "event handler: IP_EVENT_ETH_LOST_IP");
			break;
		case IP_EVENT_PPP_GOT_IP: /*!< PPP interface got IP */
			ESP_LOGI(TAG, "event handler: IP_EVENT_PPP_GOT_IP");
			break;
		case IP_EVENT_PPP_LOST_IP: /*!< PPP interface lost IP */
			ESP_LOGI(TAG, "event handler: IP_EVENT_PPP_LOST_IP");
			break;
		default:
			ESP_LOGI(TAG, "event handler: event_id %ld", event_id);
			break;
		}
	} else {
		ESP_LOGE(TAG, "event handler: event_base = %s ", event_base);
	}
}

void wifi_init_station(void)
{
	s_wifi_event_group = xEventGroupCreate();

	ESP_ERROR_CHECK(esp_netif_init());

	ESP_ERROR_CHECK(esp_event_loop_create_default());
	esp_netif_create_default_wifi_sta();

	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK(esp_wifi_init(&cfg));

	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;
	ESP_ERROR_CHECK(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id));
	ESP_ERROR_CHECK(esp_event_handler_instance_register(IP_EVENT, ESP_EVENT_ANY_ID, &ip_event_handler, NULL, &instance_got_ip));

	wifi_config_t wifi_config = {
			.sta = {
					.ssid = SECRET_SSID,
					.password = SECRET_PASS,
					.threshold.authmode = WIFI_AUTH_WPA2_PSK,
					.sae_pwe_h2e = WPA3_SAE_PWE_HUNT_AND_PECK,
					.sae_h2e_identifier = "",
			},
	};
	ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
	ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &wifi_config));
	ESP_ERROR_CHECK(esp_wifi_start());

	ESP_LOGD(TAG, "wifi_init_station() finished.");
}
