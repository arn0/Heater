/*
 * All GPIO pin assignments here
 * For ESP32C6
 */

#include "driver/gpio.h"

/*
 * USB Serial/JTAG Controller uses GPIO 12 & 13
 */

// display GPIO GC9A01
#define LCD_SPI_SCL_GPIO	GPIO_NUM_18			// serial interface clock
#define LCD_SPI_SDA_GPIO	GPIO_NUM_19			// serial interface input/output pin
#define LCD_SPI_DC_GPIO		GPIO_NUM_20			// data/command selection pin
#define LCD_SPI_CS_GPIO		GPIO_NUM_NC			// chip select pin, low=enable (module has pull-down resistor, not needed)
#define LCD_SPI_RST_GPIO	GPIO_NUM_21			// reset device, must be applied to properly initialize the chip, active low
#define LCD_BACKLIGHT_GPIO	GPIO_NUM_NC			// reseved for backlight led control

// touch GPIO
#define TOUCH_TEMP_UP_GPIO	GPIO_NUM_NC
#define TOUCH_TEMP_DN_GPIO	GPIO_NUM_NC

// sound
#define SOUND_OUT_GPIO	   GPIO_NUM_16

// GPIO assignment for build-in rgb led WS2812B
#define LED_STRIP_GPIO     GPIO_NUM_8

// 1-wire bus for ds18b20 temperature sensors
#define ONEWIRE_BUS_GPIO	GPIO_NUM_10

// level converters for heater ssr's
#define SSR_ONE_GPIO	      GPIO_NUM_6
#define SSR_TWO_GPIO	      GPIO_NUM_7

// voltage and current measuring
#define PZEM_TX_GPIO	      GPIO_NUM_5
#define PZEM_RX_GPIO	      GPIO_NUM_4