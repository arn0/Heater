// Single place to define all apps FreeRTOS task priorities
// and delay times

// Not actualy true, vtaskdelay is also used in esperessif_ds18b20, at least.

// default priority is 5
// lowest priority is 0, the idle task has priority zero
// highest priority is configMAX_PRIORITIES - 1,
// on this system 25 - 1 = 24

#ifndef TASK_PRIORITIES_H
#define TASK_PRIORITIES_H

#define HEATER_TASK_PRIORITY		12
#define HEATER_TASK_DELAY_MS		200
#define CONTROL_TASK_PRIORITY		10
#define CONTROL_TASK_DELAY_MS		500
#define MONITOR_TASK_PRIORITY		8
#define MONITOR_TASK_DELAY_MS		1000				// DS18B20 are slow (12 bit conversion takes 750 ms), delay (calculated against resolution) is included in component (esperessif_ds18b20) code, so need some time here.
#define WS_UPDATE_TASK_PRIORITY	6
#define WS_UPDATE_TASK_DELAY_MS	300
#define LVGL_TASK_PRIORITY			5
#define LVGL_TASK_DELAY_MS			10					// from example
#define CLOCK_TASK_PRIORITY		4
#define CLOCK_TASK_DELAY_MS		1000
#define STATS_TASK_PRIORITY		2
#define STATS_TASK_DELAY_MS		1000*60*7.5		// 24 * 8 datapoints so 24 h of data

#endif // TASK_PRIORITIES_H