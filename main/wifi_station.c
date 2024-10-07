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
#include "heater.h"
#include "lvgl_ui.h"
#include "sntp.h"
#include "../../secret.h"

#define WIFI_MAXIMUM_RETRY 4
#define WIFI_SHORT_DELAY_MS 3 * 1000
#define WIFI_LONG_DELAY_MS 60 * 1000

static const char *TAG = "wifi station";

/* FreeRTOS event group to signal when we are connected*/
EventGroupHandle_t s_wifi_event_group;

static int s_retry_num = 0;

static void wifi_event_handler( void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data )
{
	if ( event_base == WIFI_EVENT ) {
		switch ( event_id ) {
		case WIFI_EVENT_WIFI_READY:
			ESP_LOGI( TAG, "wifi event handler: WiFi ready" );
			break;
		case WIFI_EVENT_SCAN_DONE:
			ESP_LOGI( TAG, "wifi event handler: Finished scanning AP" );
			break;
		case WIFI_EVENT_STA_START:
			ESP_LOGI( TAG, "wifi event handler: Station start" );
			esp_wifi_connect();
			ESP_LOGI( TAG, "wifi event handler: esp_wifi_connect() done" );
			break;
		case WIFI_EVENT_STA_STOP:
			ESP_LOGI( TAG, "wifi event handler: Station stop" );
			break;
		case WIFI_EVENT_STA_CONNECTED:
			ESP_LOGI( TAG, "wifi event handler: Station connected to AP" );
			s_retry_num = 0;
			break;
		case WIFI_EVENT_STA_DISCONNECTED:
			ESP_LOGI( TAG, "wifi event handler: Station disconnected from AP" );
			heater_status.wifi = false;
			sntp_client_stop();

#ifdef CONFIG_EXAMPLE_ENABLE_LCD
			lvgl_ui_update();
#endif
			if ( s_retry_num < WIFI_MAXIMUM_RETRY ) {
				vTaskDelay( pdMS_TO_TICKS( WIFI_SHORT_DELAY_MS ) );
				esp_wifi_connect();
				ESP_LOGI( TAG, "wifi event handler: esp_wifi_connect() done, retry %d", s_retry_num );
				s_retry_num++;
			} else {
				vTaskDelay(pdMS_TO_TICKS(WIFI_LONG_DELAY_MS));
				esp_wifi_connect();
				ESP_LOGI(TAG, "wifi event handler: esp_wifi_connect() done, retry %d", s_retry_num);

				//xEventGroupSetBits(s_wifi_event_group, WIFI_DISCONNECTED_BIT);
				//ESP_LOGI(TAG, "event handler: xEventGroupSetBits(s_wifi_event_group, WIFI_DISCONNECTED_BIT) done");
				//s_retry_num = 0;
			}
			ESP_LOGI(TAG, "wifi event handler: connect to the AP failed");
			break;
		case WIFI_EVENT_STA_AUTHMODE_CHANGE:
			ESP_LOGI(TAG, "wifi event handler: The auth mode of AP connected by device's station changed");
			break;
		case WIFI_EVENT_STA_WPS_ER_SUCCESS:
			ESP_LOGI(TAG, "wifi event handler: Station wps succeeds in enrollee mode");
			break;
		case WIFI_EVENT_STA_WPS_ER_FAILED:
			ESP_LOGI(TAG, "wifi event handler: Station wps fails in enrollee mode");
			break;
		case WIFI_EVENT_STA_WPS_ER_TIMEOUT: /**< Station wps timeout in enrollee mode */
			ESP_LOGI(TAG, "wifi event handler: WIFI_EVENT_STA_WPS_ER_TIMEOUT");
			break;
		case WIFI_EVENT_STA_WPS_ER_PIN: /**< Station wps pin code in enrollee mode */
			ESP_LOGI(TAG, "wifi event handler: WIFI_EVENT_STA_WPS_ER_PIN");
			break;
		case WIFI_EVENT_STA_WPS_ER_PBC_OVERLAP:  /**< Station wps overlap in enrollee mode */
			ESP_LOGI(TAG, "wifi event handler: WIFI_EVENT_STA_WPS_ER_PBC_OVERLAP");
			break;
		case WIFI_EVENT_AP_START: /**< Soft-AP start */
			ESP_LOGI(TAG, "wifi event handler: WIFI_EVENT_AP_START");
			break;
		case WIFI_EVENT_AP_STOP: /**< Soft-AP stop */
			ESP_LOGI(TAG, "wifi event handler: WIFI_EVENT_AP_STOP");
			break;
		case WIFI_EVENT_AP_STACONNECTED: /**< a station connected to Soft-AP */
			ESP_LOGI(TAG, "wifi event handler: WIFI_EVENT_AP_STACONNECTED");
			break;
		case WIFI_EVENT_AP_STADISCONNECTED: /**< a station disconnected from Soft-AP */
			ESP_LOGI(TAG, "wifi event handler: WIFI_EVENT_AP_STADISCONNECTED");
			sntp_client_stop();
			break;
		case WIFI_EVENT_AP_PROBEREQRECVED: /**< Receive probe request packet in soft-AP interface */
			ESP_LOGI(TAG, "wifi event handler: WIFI_EVENT_AP_PROBEREQRECVED");
			break;
		case WIFI_EVENT_FTM_REPORT: /**< Receive report of FTM procedure */
			ESP_LOGI(TAG, "wifi event handler: WIFI_EVENT_FTM_REPORT");
			break;
		case WIFI_EVENT_STA_BSS_RSSI_LOW: /**< AP's RSSI crossed configured threshold */
			ESP_LOGI(TAG, "wifi event handler: WIFI_EVENT_STA_BSS_RSSI_LOW");
			break;
		case WIFI_EVENT_ACTION_TX_STATUS: /**< Status indication of Action Tx operation */
			ESP_LOGI(TAG, "wifi event handler: WIFI_EVENT_ACTION_TX_STATUS");
			break;
		case WIFI_EVENT_ROC_DONE: /**< Remain-on-Channel operation complete */
			ESP_LOGI(TAG, "wifi event handler: WIFI_EVENT_ROC_DONE");
			break;
		case WIFI_EVENT_STA_BEACON_TIMEOUT: /**< Station beacon timeout */
			ESP_LOGI(TAG, "wifi event handler: WIFI_EVENT_STA_BEACON_TIMEOUT");
			break;
		case WIFI_EVENT_ITWT_SETUP: /**< iTWT setup */
			ESP_LOGI(TAG, "wifi event handler: WIFI_EVENT_ITWT_SETUP");
			break;
		case WIFI_EVENT_ITWT_TEARDOWN: /**< iTWT teardown */
			ESP_LOGI(TAG, "wifi event handler: WIFI_EVENT_ITWT_TEARDOWN");
			break;
		case WIFI_EVENT_ITWT_PROBE: /**< iTWT probe */
			ESP_LOGI(TAG, "wifi event handler: WIFI_EVENT_ITWT_PROBE");
			break;
		case WIFI_EVENT_ITWT_SUSPEND: /**< iTWT suspend */
			ESP_LOGI(TAG, "wifi event handler: WIFI_EVENT_ITWT_SUSPEND");
			break;
		case WIFI_EVENT_NAN_STARTED: /**< NAN Discovery has started */
			ESP_LOGI(TAG, "wifi event handler: WIFI_EVENT_NAN_STARTED");
			break;
		case WIFI_EVENT_NAN_STOPPED: /**< NAN Discovery has stopped */
			ESP_LOGI(TAG, "wifi event handler: WIFI_EVENT_NAN_STOPPED");
			break;
		case WIFI_EVENT_NAN_SVC_MATCH: /**< NAN Service Discovery match found */
			ESP_LOGI(TAG, "wifi event handler: WIFI_EVENT_NAN_SVC_MATCH");
			break;
		case WIFI_EVENT_NAN_REPLIED: /**< Replied to a NAN peer with Service Discovery match */
			ESP_LOGI(TAG, "wifi event handler: WIFI_EVENT_NAN_REPLIED");
			break;
		case WIFI_EVENT_NAN_RECEIVE: /**< Received a Follow-up message */
			ESP_LOGI(TAG, "wifi event handler: WIFI_EVENT_NAN_RECEIVE");
			break;
		case WIFI_EVENT_NDP_INDICATION: /**< Received NDP Request from a NAN Peer */
			ESP_LOGI(TAG, "wifi event handler: WIFI_EVENT_NDP_INDICATION");
			break;
		case WIFI_EVENT_NDP_CONFIRM: /**< NDP Confirm Indication */
			ESP_LOGI(TAG, "wifi event handler: WIFI_EVENT_NDP_CONFIRM");
			break;
		case WIFI_EVENT_NDP_TERMINATED: /**< NAN Datapath terminated indication */
			ESP_LOGI(TAG, "wifi event handler: WIFI_EVENT_NDP_TERMINATED");
			break;
		case WIFI_EVENT_HOME_CHANNEL_CHANGE:
			ESP_LOGI(TAG, "wifi event handler: WiFi home channel change，doesn't occur when scanning");
			break;
		default:
			ESP_LOGE(TAG, "wifi event handler: event_id = %ld", event_id);
			break;
		}
	} else {
		ESP_LOGE(TAG, "wifi event handler: event_base = %s ", event_base);
	}
}

static void ip_event_handler( void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data )
{
	if (event_base == IP_EVENT) {
		switch ( event_id ) {
		case IP_EVENT_STA_GOT_IP: /*!< station got IP from connected AP */
			ESP_LOGI( TAG, "ip event handler: IP_EVENT_STA_GOT_IP" );
			ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
			ESP_LOGI( TAG, "ip event handler: got ip:" IPSTR, IP2STR( &event->ip_info.ip ) );
			s_retry_num = 0;
			xEventGroupSetBits( s_wifi_event_group, WIFI_CONNECTED_BIT );
			 heater_status.wifi = true;
#ifdef CONFIG_EXAMPLE_ENABLE_LCD
			lvgl_ui_update();
#endif
			break;
		case IP_EVENT_STA_LOST_IP: /*!< station lost IP and the IP is reset to 0 */
			ESP_LOGI( TAG, "ip event handler: IP_EVENT_STA_LOST_IP" );
			heater_status.wifi = false;
#ifdef CONFIG_EXAMPLE_ENABLE_LCD
			lvgl_ui_update();
#endif
			break;
		case IP_EVENT_AP_STAIPASSIGNED: /*!< soft-AP assign an IP to a connected station */
			ESP_LOGI( TAG, "ip event handler: IP_EVENT_AP_STAIPASSIGNED" );
			break;
		case IP_EVENT_GOT_IP6: /*!< station or ap or ethernet interface v6IP addr is preferred */
			ESP_LOGI( TAG, "ip event handler: IP_EVENT_GOT_IP6" );
			break;
		case IP_EVENT_ETH_GOT_IP: /*!< ethernet got IP from connected AP */
			ESP_LOGI( TAG, "ip event handler: IP_EVENT_ETH_GOT_IP" );
			break;
		case IP_EVENT_ETH_LOST_IP: /*!< ethernet lost IP and the IP is reset to 0 */
			ESP_LOGI( TAG, "ip event handler: IP_EVENT_ETH_LOST_IP" );
			break;
		case IP_EVENT_PPP_GOT_IP: /*!< PPP interface got IP */
			ESP_LOGI( TAG, "ip event handler: IP_EVENT_PPP_GOT_IP" );
			break;
		case IP_EVENT_PPP_LOST_IP: /*!< PPP interface lost IP */
			ESP_LOGI( TAG, "ip event handler: IP_EVENT_PPP_LOST_IP" );
			break;
		default:
			ESP_LOGE( TAG, "ip event handler: event_id %ld", event_id );
			break;
		}
	} else {
		ESP_LOGE( TAG, "ip event handler: event_base = %s ", event_base);
	}
}

void wifi_init_station()
{
	s_wifi_event_group = xEventGroupCreate();
	ESP_ERROR_CHECK( esp_event_loop_create_default() );
	
	ESP_ERROR_CHECK( esp_netif_init() );
	esp_netif_create_default_wifi_sta();
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	ESP_ERROR_CHECK( esp_wifi_init( &cfg ) );

	esp_event_handler_instance_t instance_any_id;
	esp_event_handler_instance_t instance_got_ip;
	ESP_ERROR_CHECK( esp_event_handler_instance_register( WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL, &instance_any_id ) );
	ESP_ERROR_CHECK( esp_event_handler_instance_register( IP_EVENT, ESP_EVENT_ANY_ID, &ip_event_handler, NULL, &instance_got_ip ) );

	wifi_config_t wifi_config = {
			.sta = {
					.ssid = SECRET_SSID,
					.password = SECRET_PASS,
					.threshold.authmode = WIFI_AUTH_WPA2_PSK,
					.sae_pwe_h2e = WPA3_SAE_PWE_HUNT_AND_PECK,
					.sae_h2e_identifier = "",
					.failure_retry_cnt = 3,
			},
	};
	ESP_ERROR_CHECK( esp_wifi_set_mode( WIFI_MODE_STA ) );
	ESP_ERROR_CHECK( esp_wifi_set_config( WIFI_IF_STA, &wifi_config ) );
	vTaskDelay( pdMS_TO_TICKS( 200 ) );
	ESP_ERROR_CHECK( esp_wifi_start() );

	ESP_LOGI(TAG, "wifi_init_station() finished.");
}
