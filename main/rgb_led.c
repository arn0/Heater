#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_system.h"
#include "esp_log.h"
#include "led_strip.h"
#include "driver/rmt_tx.h"

#include "gpio_pins.h"
#include "rgb_led.h"

static const char *TAG = "example";

static uint8_t led_strip_pixels[LED_STRIP_LED_NUMBERS * 3];

uint32_t red = 0;
uint32_t green = 0;
uint32_t blue = 0;
uint16_t hue = 0;
uint16_t running_hue = 0;
uint16_t start_rgb = 0;

led_strip_handle_t led_strip;



// Simple helper function, converting HSV color space to RGB color space
 
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
		.strip_gpio_num = LED_STRIP_GPIO_PIN,   // The GPIO that connected to the LED strip's data line
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

void led_strip_step(){
			// Build RGB pixels
			hue = running_hue += 10;
			led_strip_hsv2rgb(hue, 100, 10, &red, &green, &blue);
			led_strip_pixels[0] = green;
			led_strip_pixels[1] = blue;
			led_strip_pixels[2] = red;
		
		// Flush RGB values to LEDs
		ESP_ERROR_CHECK(led_strip_set_pixel(led_strip, 0, led_strip_pixels[0], led_strip_pixels[1], led_strip_pixels[2]));
		ESP_ERROR_CHECK(led_strip_refresh(led_strip));
}

void led_strip_task(){
	TickType_t xLastWakeTime;
	const TickType_t xFrequency = pdMS_TO_TICKS(3000);
	BaseType_t xWasDelayed;

	// Initialise the xLastWakeTime variable with the current time.
	xLastWakeTime = xTaskGetTickCount ();

	do{
		// Wait for the next cycle.
		xWasDelayed = xTaskDelayUntil( &xLastWakeTime, xFrequency );

		if( xWasDelayed == pdFALSE ){
			ESP_LOGE(TAG, "Task ran out of time");
		}
		led_strip_step();
	}while (true);
}

void led_strip_start(){
	led_strip = configure_led();
	xTaskCreate( led_strip_task, "led_strip_task", 4096, NULL, 5, NULL );
}