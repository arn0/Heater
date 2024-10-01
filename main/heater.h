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

#define TAR_W_FL 0b1
#define CHP_W_FL 0b10
#define FNT_W_FL 0b100
#define BCK_W_FL 0b1000
#define TOP_W_FL 0b10000
#define BOT_W_FL 0b100000
#define REM_W_FL 0b1000000
#define VLT_W_FL 0b10000000
#define CUR_W_FL 0b100000000
#define ONE_W_FL 0b1000000000
#define TWO_W_FL 0b10000000000

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
	unsigned int web;
	bool one_pwr;
	bool one_set;
	bool two_pwr;
	bool two_set;
	bool safe;
};

// global access to heater staus
extern struct heater_status heater_status;

bool start_heater_task();

#ifdef __cplusplus
}
#endif

#endif //HEATER_H