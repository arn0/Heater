#ifndef HEATER_H
#define HEATER_H

#ifdef __cplusplus
extern "C" {
#endif

//struct heat_dat {
//	float val;
//	bool web;
//	bool dsp;
//	bool mat;
//};

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