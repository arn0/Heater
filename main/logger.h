#ifndef LOGGER_H
#define LOGGER_H

#include "time.h"
#include "esp_system.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef CONFIG_EXAMPLE_ENABLE_STATUS_LOG
#define ENABLE_LOG
#endif

#define SNAPSHOT_BUFFER_SIZE 24*60

struct snapshot {
	time_t time;
	float target;
	float fnt;
	float bck;
	float bot;
	float top;
	float chip;
	float rem;
   float voltage;
   float current;
   float power;            // Active Power P. or Real Power W.
   float energy;
   float pf;               // Ratio of active to apparent power, cos(fi), eg pf = 0.77, 77% of current is doing the real work
	uint16_t web;
	bool one_gpio : 1;
	bool one_on : 1;
	bool two_gpio : 1;
	bool two_on : 1;
	bool safe : 1;
};

esp_err_t log_add();
esp_err_t log_save();
esp_err_t log_read();
int16_t log_fill();

#ifdef __cplusplus
}
#endif

#endif //LOGGER_H
