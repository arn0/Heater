#ifndef HEATER_H
#define HEATER_H

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Let's say, GPIO_OUTPUT_IO_0=18, GPIO_OUTPUT_IO_1=19
 * In binary representation,
 * 1ULL<<GPIO_OUTPUT_IO_0 is equal to 0000000000000000000001000000000000000000 and
 * 1ULL<<GPIO_OUTPUT_IO_1 is equal to 0000000000000000000010000000000000000000
 * GPIO_OUTPUT_PIN_SEL                0000000000000000000011000000000000000000
 */

#define GPIO_OUTPUT_PIN_SEL  ((1ULL<<SSR_ONE_GPIO_PIN) | (1ULL<<SSR_TWO_GPIO_PIN))

#define TARGET_DEFAULT 20.5
#define MAX_TEMP 105.0

struct heater_status {
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
	bool one_pwr;
	bool one_set;
	bool two_pwr;
	bool two_set;
	bool safe;
	bool update;
	bool blue;
};

// global access to heater staus
extern struct heater_status heater_status;

bool start_heater_task();

#ifdef __cplusplus
}
#endif

#endif //HEATER_H