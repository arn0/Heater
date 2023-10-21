/*
 * All GPIO pin assignments here
 */

// display GPIO GC9A01
#define LCD_SPI_SCL_GPIO	0			// serial interface clock
#define LCD_SPI_SDA_GPIO	0			// serial interface input/output pin
#define LCD_SPI_DC_GPIO		0			// data/command selection pin
#define LCD_SPI_CS_GPIO		0			// chip select pin, low=enable (module has pull-down resistor, not needed)
#define LCD_SPI_RST_GPIO	0			// reset device, must be applied to properly initialize the chip, active low
#define LCD_BACKLIGHT_GPIO	0			// reseved for backlight led control

// touch GPIO
#define TOUCH_TEMP_UP_GPIO	0
#define TOUCH_TEMP_DN_GPIO	0

// sound
#define SOUND_OUT_GPIO	0

// GPIO assignment for build-in rgb led WS2812B
#define LED_STRIP_BLINK_GPIO  8

// 1-wire bus for ds18b20 temperature sensors
#define HEATER_ONEWIRE_BUS_GPIO	10

// level converters for heater ssr's
#define HEATER_SSR_ONE_GPIO	0
#define HEATER_SSR_TWO_GPIO	0

// voltage and current measuring
#define MAINS_VOLTAGE_GPIO	0
#define MAINS_CURRENT_GPIO	0