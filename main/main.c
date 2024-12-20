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

#include "heater.h"
#include "control.h"
#include "monitor.h"
#include "wifi_station.h"
#include "sntp.h"
#include "webmdns.h"
#include "events.h"
#include "mount.h"
#include "webserver.h"
#include "bluetooth.h"
#include "gpio_pins.h"
#include "spi_lcd.h"
#include "clock.h"
#include "../../secret.h"

extern void real_time_stats( void );

static const char *TAG = "main";

bool bluetooth = false;


void app_main( void ) {
   timezone_set(); // First, to get right timestamp in log messages

   esp_err_t ret = nvs_flash_init(); // Initialize NVS
   if ( ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND ) {
      ESP_ERROR_CHECK( nvs_flash_erase() );
      ret = nvs_flash_init();
   }
   ESP_ERROR_CHECK( ret );

   ESP_ERROR_CHECK( spiffs_init( "/data" ) ); // Initialize SPIFFS which holds the HTML/CSS/JS files we serve to client browser
                                              // and to store statistics file
#ifdef DO_NOT_DO_THIS
   real_time_stats();
#endif

   if ( !heater_task_start() ) {
      ESP_LOGE( TAG, "Heater task did not start!" );
   }
   monitor_task_start();

#ifdef CONFIG_EXAMPLE_ENABLE_LCD
   lcd_start();
   clock_start();
#endif
#ifdef DO_NOT_DO_THIS
   led_strip_start();
#endif

   control_task_start();

   vTaskDelay( pdMS_TO_TICKS( 1000 ) ); // need a little time before wifi is ready

   ESP_LOGI( TAG, "Start wifi_init_station()" );
   wifi_init_station();

   ESP_LOGI( TAG, "Start bluetooth" );
   ret = bluetooth_start();
   if ( ret == ESP_OK ) {
      bluetooth = true;
   }

   do {
      EventBits_t bits;

      /* Waiting until either the connection is established (WIFI_CONNECTED_BIT) or connection failed for the maximum
       * number of re-tries (WIFI_DISCONNECTED_BIT). The bits are set by event_handler() */

      bits = xEventGroupWaitBits( s_wifi_event_group, WIFI_CONNECTED_BIT | WIFI_DISCONNECTED_BIT | SLEEP_WAKEUP_BIT | TCP_CONNECTED_BIT | TCP_FAILED_BIT, pdTRUE, pdFALSE, portMAX_DELAY );

      if ( bits & WIFI_CONNECTED_BIT ) {
         ESP_LOGI( TAG, "connected to ap SSID:%s", SECRET_SSID );

         sntp_client_start();
         webserver_start();
         start_mdns_service();

      } else if ( bits & WIFI_DISCONNECTED_BIT ) {
         ESP_LOGI( TAG, "Failed to connect to SSID:%s", SECRET_SSID );
         ESP_ERROR_CHECK( esp_wifi_stop() );

         sntp_client_stop();

      } else if ( bits & SLEEP_WAKEUP_BIT ) {
         ESP_LOGI( TAG, "SLEEP_WAKEUP_BIT received" );
         ESP_ERROR_CHECK( esp_wifi_start() );
      } else if ( bits & TCP_CONNECTED_BIT ) {
         ESP_LOGI( TAG, "TCP_CONNECTED_BIT received" );
      } else if ( bits & TCP_FAILED_BIT ) {
         ESP_LOGI( TAG, "TCP_FAILED_BIT received" );
      } else {
         ESP_LOGE( TAG, "UNEXPECTED EVENT" );
      }

      if ( !bluetooth ) {
         ESP_LOGI( TAG, "Start bluetooth()" );
         ret = bluetooth_start();
         if ( ret == ESP_OK ) {
            bluetooth = true;
         } else {
            bluetooth = false;
         }
      }
   } while ( true );
}