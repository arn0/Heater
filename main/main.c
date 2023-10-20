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





#include "driver/temperature_sensor.h"
#include "ds18b20.h"
#include "onewire_bus.h"
#include "driver/rmt_tx.h"
#include "led_strip.h"

#include "wifi_station.h"
#include "events.h"
#include "webserver.h"
#include "../../secret.h"


//#define EXAMPLE_ONEWIRE_BUS_GPIO    10
//#define EXAMPLE_ONEWIRE_MAX_DS18B20 2


// GPIO assignment
#define LED_STRIP_BLINK_GPIO  8
// Numbers of the LED in the strip
#define LED_STRIP_LED_NUMBERS 1
// 10MHz resolution, 1 tick = 0.1us (led strip needs a high resolution)
#define LED_STRIP_RMT_RES_HZ  (10 * 1000 * 1000)

#define EXAMPLE_CHASE_SPEED_MS      10






static const char *TAG = "example";

static uint8_t led_strip_pixels[LED_STRIP_LED_NUMBERS * 3];

/**
 * @brief Simple helper function, converting HSV color space to RGB color space
 *
 * Wiki: https://en.wikipedia.org/wiki/HSL_and_HSV
 *
 */
void led_strip_hsv2rgb(uint32_t h, uint32_t s, uint32_t v, uint32_t *r, uint32_t *g, uint32_t *b)
{
    h %= 360; // h -> [0,360]
    uint32_t rgb_max = v * 2.55f;
    uint32_t rgb_min = rgb_max * (100 - s) / 100.0f;

    uint32_t i = h / 60;
    uint32_t diff = h % 60;

    // RGB adjustment amount by hue
    uint32_t rgb_adj = (rgb_max - rgb_min) * diff / 60;

    switch (i) {
    case 0:
        *r = rgb_max;
        *g = rgb_min + rgb_adj;
        *b = rgb_min;
        break;
    case 1:
        *r = rgb_max - rgb_adj;
        *g = rgb_max;
        *b = rgb_min;
        break;
    case 2:
        *r = rgb_min;
        *g = rgb_max;
        *b = rgb_min + rgb_adj;
        break;
    case 3:
        *r = rgb_min;
        *g = rgb_max - rgb_adj;
        *b = rgb_max;
        break;
    case 4:
        *r = rgb_min + rgb_adj;
        *g = rgb_min;
        *b = rgb_max;
        break;
    default:
        *r = rgb_max;
        *g = rgb_min;
        *b = rgb_max - rgb_adj;
        break;
    }
}

led_strip_handle_t configure_led(void)
{
    // LED strip general initialization, according to your led board design
    led_strip_config_t strip_config = {
        .strip_gpio_num = LED_STRIP_BLINK_GPIO,   // The GPIO that connected to the LED strip's data line
        .max_leds = LED_STRIP_LED_NUMBERS,        // The number of LEDs in the strip,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Pixel format of your LED strip
        .led_model = LED_MODEL_WS2812,            // LED strip model
        .flags.invert_out = false,                // whether to invert the output signal
    };

    // LED strip backend configuration: RMT
    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
        .resolution_hz = LED_STRIP_RMT_RES_HZ, // RMT counter clock frequency
        .flags.with_dma = false,               // DMA feature is available on ESP target like ESP32-S3
    };

    // LED Strip object handle
    led_strip_handle_t led_strip;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    ESP_LOGI(TAG, "Created LED strip object with RMT backend");
    return led_strip;
}


void app_main(void)
{


    uint32_t red = 0;
    uint32_t green = 0;
    uint32_t blue = 0;
    uint16_t hue = 0;
    uint16_t start_rgb = 0;

    led_strip_handle_t led_strip = configure_led();

	//Initialize NVS
	esp_err_t ret = nvs_flash_init();
	if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
		ESP_ERROR_CHECK(nvs_flash_erase());
		ret = nvs_flash_init();
	}
	ESP_ERROR_CHECK(ret);

	setenv( "TZ", "CET-1CEST,M3.5.0/2,M10.5.0/3", 100 );		// set timezone and daylight saving time
	tzset();

	ESP_LOGI(TAG, "Start wifi_init_station()");
	wifi_init_station();







    // install 1-wire bus
    //onewire_bus_handle_t bus = NULL;
    //onewire_bus_config_t bus_config = {
    //    .bus_gpio_num = EXAMPLE_ONEWIRE_BUS_GPIO,
    //};
    //onewire_bus_rmt_config_t rmt_config = {
    //    .max_rx_bytes = 10, // 1byte ROM command + 8byte ROM number + 1byte device command
    //};
    //ESP_ERROR_CHECK(onewire_new_bus_rmt(&bus_config, &rmt_config, &bus));

    //int ds18b20_device_num = 0;
    //ds18b20_device_handle_t ds18b20s[EXAMPLE_ONEWIRE_MAX_DS18B20];
    //onewire_device_iter_handle_t iter = NULL;
    //onewire_device_t next_onewire_device;
    //esp_err_t search_result = ESP_OK;

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




    float temperature;

    ESP_LOGI(TAG, "\n\n\n\nInstall temperature sensor, expected temp ranger range: 10~50 ℃");
    temperature_sensor_handle_t temp_sensor = NULL;
    temperature_sensor_config_t temp_sensor_config = TEMPERATURE_SENSOR_CONFIG_DEFAULT(10, 50);
    ESP_ERROR_CHECK(temperature_sensor_install(&temp_sensor_config, &temp_sensor));

    ESP_LOGI(TAG, "Enable temperature sensor");
    ESP_ERROR_CHECK(temperature_sensor_enable(temp_sensor));

    ESP_LOGI(TAG, "Read temperature");
    int cnt = 200;
    float tsens_value;
    while (cnt--) {
        ESP_ERROR_CHECK(temperature_sensor_get_celsius(temp_sensor, &tsens_value));
        ESP_LOGI(TAG, "Temperature value %.02f ℃", tsens_value);
        vTaskDelay(pdMS_TO_TICKS(1000));
    
       for (int i = 0; i < ds18b20_device_num; i ++) {
        ESP_ERROR_CHECK(ds18b20_trigger_temperature_conversion(ds18b20s[i]));
        ESP_ERROR_CHECK(ds18b20_get_temperature(ds18b20s[i], &temperature));
        ESP_LOGI(TAG, "temperature read from DS18B20[%d]: %.2fC", i, temperature);
        }









        for (int i = 0; i < 3; i++) {
            for (int j = i; j < LED_STRIP_LED_NUMBERS; j += 3) {
                // Build RGB pixels
                hue = j * 90 / LED_STRIP_LED_NUMBERS + start_rgb;
                led_strip_hsv2rgb(hue, 100, 10, &red, &green, &blue);
                led_strip_pixels[j * 3 + 0] = green;
                led_strip_pixels[j * 3 + 1] = blue;
                led_strip_pixels[j * 3 + 2] = red;
            }
            // Flush RGB values to LEDs
            ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, led_strip_pixels[0 * 3 + 0], led_strip_pixels[0 * 3 + 1], led_strip_pixels[0 * 3 + 2]));
            ESP_ERROR_CHECK(led_strip_refresh(led_strip));
        }
        start_rgb += 60;
    
    
    
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
 			init_webserver();
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
