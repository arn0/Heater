#include <string.h>
#include <time.h>
#include <sys/time.h>
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "esp_netif_sntp.h"
#include "lwip/ip_addr.h"
#include "esp_sntp.h"

#include "sntp.h"

#ifndef INET6_ADDRSTRLEN
#define INET6_ADDRSTRLEN 48
#endif

static const char *TAG = "sntp";
bool sntp_valid = false;


void time_sync_notification_cb(struct timeval *tv)
{
	sntp_valid = true;
	ESP_LOGI(TAG, "Notification of a time synchronization event");
}

//void sntp_init()
//{
//	setenv( "TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 100 );		// set timezone and daylight saving time
//	tzset();
	
//}

//void sntp_start(){
	
//}

//void sntp_stop(){
	 //esp_netif_sntp_deinit();
//}








static void obtain_time(void);

//void app_main(void)
//{
//	 time_t now;
//	 struct tm timeinfo;
//	 time(&now);
//	 localtime_r(&now, &timeinfo);

	 // Is time set? If not, tm_year will be (1970 - 1900).
	 //if //(timeinfo.tm_year < (2016 - 1900)) {
		  //ESP_LOGI(TAG, "Time is not set yet. Connecting to WiFi and getting time over NTP.");
		  //obtain_time();
		  // update 'now' variable with current time
		  //time(&now);
	 //}
//}

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

static void obtain_time(void)
{
#if LWIP_DHCP_GET_NTP_SRV
	 /**
	  * NTP server address could be acquired via DHCP,
	  * see following menuconfig options:
	  * 'LWIP_DHCP_GET_NTP_SRV' - enable STNP over DHCP
	  * 'LWIP_SNTP_DEBUG' - enable debugging messages
	  *
	  * NOTE: This call should be made BEFORE esp acquires IP address from DHCP,
	  * otherwise NTP option would be rejected by default.
	  */
	 ESP_LOGI(TAG, "Initializing SNTP");
	 esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_SNTP_TIME_SERVER);
	 config.start = false;                       // start SNTP service explicitly (after connecting)
	 config.server_from_dhcp = true;             // accept NTP offers from DHCP server, if any (need to enable *before* connecting)
	 config.renew_servers_after_new_IP = true;   // let esp-netif update configured SNTP server(s) after receiving DHCP lease
	 config.index_of_first_server = 1;           // updates from server num 1, leaving server 0 (from DHCP) intact
	 // configure the event on which we renew servers
	 config.ip_event_to_renew = IP_EVENT_STA_GOT_IP;
	 esp_netif_sntp_init(&config);
#endif /* LWIP_DHCP_GET_NTP_SRV */

#if LWIP_DHCP_GET_NTP_SRV
	 ESP_LOGI(TAG, "Starting SNTP");
	 esp_netif_sntp_start();

#else
	 ESP_LOGI(TAG, "Initializing and starting SNTP");
	 /*
	  * This is the basic default config with one server and starting the service
	  */
	 esp_sntp_config_t config = ESP_NETIF_SNTP_DEFAULT_CONFIG(CONFIG_SNTP_TIME_SERVER);

	 config.sync_cb = time_sync_notification_cb;     // Note: This is only needed if we want

	 esp_netif_sntp_init(&config);
#endif

	 print_servers();

}