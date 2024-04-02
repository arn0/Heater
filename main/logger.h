#ifndef LOGGER_H
#define LOGGER_H

#include "time.h"
#include "esp_system.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SNAPSHOT_BUFFER_SIZE 24*60

struct snapshot {
	time_t time;
	float target;
	float env;
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
	bool one_set : 1;
	bool one_pwr : 1;
	bool two_set : 1;
	bool two_pwr : 1;
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
