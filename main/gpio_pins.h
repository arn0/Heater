/*
 * All GPIO pin assignments here
 * For ESP32S3
 */

#ifndef GPIO_PINS_H
#define GPIO_PINS_H

#include "driver/gpio.h"

/*
 * USB Serial/JTAG Controller uses GPIO 12 & 13 (ESP32-S3)
 */

// display GPIO GC9A01
#define LCD_SPI_SCL_GPIO_PIN GPIO_NUM_39   	// serial interface clock
#define LCD_SPI_SDA_GPIO_PIN GPIO_NUM_40   	// serial interface input/output pin
#define LCD_SPI_DC_GPIO_PIN GPIO_NUM_41    	// data/command selection pin
#define LCD_SPI_CS_GPIO_PIN GPIO_NUM_NC    	// chip select pin, low=enable (module has pull-down resistor, not needed)
#define LCD_SPI_RST_GPIO_PIN GPIO_NUM_42   	// reset device, must be applied to properly initialize the chip, active low
#define LCD_BACKLIGHT_GPIO_PIN GPIO_NUM_NC 	// reseved for backlight led control

// switch GPIO
#define TOUCH_TEMP_UP_GPIO_PIN GPIO_NUM_NC
#define TOUCH_TEMP_DN_GPIO_PIN GPIO_NUM_NC

// sound
#define SOUND_OUT_GPIO_PIN GPIO_NUM_NC

// GPIO assignment for build-in rgb led WS2812B
#define LED_STRIP_GPIO_PIN GPIO_NUM_47

// 1-wire bus for ds18b20 temperature sensors
#define ONEWIRE_BUS_GPIO_PIN GPIO_NUM_15

// level converters for heater ssr's
#define SSR_ONE_GPIO_PIN GPIO_NUM_6
#define SSR_TWO_GPIO_PIN GPIO_NUM_7

// voltage and current measuring
#define PZEM_TX_GPIO_PIN GPIO_NUM_4
#define PZEM_RX_GPIO_PIN GPIO_NUM_5

#endif // GPIO_PINS_H